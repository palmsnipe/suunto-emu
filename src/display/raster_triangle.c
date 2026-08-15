/*
 * Triangle rasterization (ticket 510).
 * Only axis-aligned full-coverage triangles with integer coordinates
 * are accepted.  Partial-edge triangles are refused.
 */

#include "raster.h"

/* Check if a 16.16 fixed-point value has a zero fractional part. */
static int is_integer_fp(uint32_t v)
{
    return (v & RASTER_FP_FRAC_MASK) == 0u;
}

/* Check if two fixed-point values share the same integer part. */
static int same_x(uint32_t a, uint32_t b)
{
    return is_integer_fp(a) && is_integer_fp(b) &&
           (a >> RASTER_FP_SHIFT) == (b >> RASTER_FP_SHIFT);
}

static int same_y(uint32_t a, uint32_t b)
{
    return is_integer_fp(a) && is_integer_fp(b) &&
           (a >> RASTER_FP_SHIFT) == (b >> RASTER_FP_SHIFT);
}

/*
 * Check if the triangle is axis-aligned: at least one horizontal edge
 * (shared Y) and one vertical edge (shared X), with all integer coords.
 */
static int is_axis_aligned(uint32_t p0x, uint32_t p0y,
                            uint32_t p1x, uint32_t p1y,
                            uint32_t p2x, uint32_t p2y)
{
    int has_horiz, has_vert;

    if (!is_integer_fp(p0x) || !is_integer_fp(p0y) ||
        !is_integer_fp(p1x) || !is_integer_fp(p1y) ||
        !is_integer_fp(p2x) || !is_integer_fp(p2y)) {
        return 0;
    }

    has_horiz = same_y(p0y, p1y) || same_y(p1y, p2y) || same_y(p0y, p2y);
    has_vert = same_x(p0x, p1x) || same_x(p1x, p2x) || same_x(p0x, p2x);
    return has_horiz && has_vert;
}

/* Edge function: positive if (px,py) is on the left side of p0->p1. */
static int32_t edge_func(int32_t x0, int32_t y0,
                          int32_t x1, int32_t y1,
                          int32_t px, int32_t py)
{
    return (x1 - x0) * (py - y0) - (y1 - y0) * (px - x0);
}

semu_status raster_triangle(raster_target *target,
                             const raster_bounds *clip,
                             uint32_t p0x, uint32_t p0y,
                             uint32_t p1x, uint32_t p1y,
                             uint32_t p2x, uint32_t p2y,
                             uint16_t color,
                             raster_bounds *dirty,
                             semu_error *error)
{
    raster_bounds effective_clip;
    int32_t v0x, v0y, v1x, v1y, v2x, v2y;
    int32_t min_x, min_y, max_x, max_y;
    int32_t area;
    int32_t px, py;
    uint32_t drawn = 0u;

    if (target == NULL || target->pixels == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "raster: null target");
        return SEMU_ERR_ARGUMENT;
    }
    if (target->stride < target->width * 2u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "raster: bad stride");
        return SEMU_ERR_UNSUPPORTED;
    }

    if (!is_axis_aligned(p0x, p0y, p1x, p1y, p2x, p2y)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "raster: non-axis-aligned triangle refused");
        return SEMU_ERR_UNSUPPORTED;
    }

    /* Set up clip */
    if (clip != NULL) {
        if (!raster_clip_intersect(clip, target->width, target->height,
                                     &effective_clip)) {
            return SEMU_OK;
        }
    } else {
        effective_clip.min_x = 0u;
        effective_clip.min_y = 0u;
        effective_clip.max_x = target->width;
        effective_clip.max_y = target->height;
    }

    /* Convert to integer pixel coordinates */
    v0x = (int32_t)(p0x >> RASTER_FP_SHIFT);
    v0y = (int32_t)(p0y >> RASTER_FP_SHIFT);
    v1x = (int32_t)(p1x >> RASTER_FP_SHIFT);
    v1y = (int32_t)(p1y >> RASTER_FP_SHIFT);
    v2x = (int32_t)(p2x >> RASTER_FP_SHIFT);
    v2y = (int32_t)(p2y >> RASTER_FP_SHIFT);

    /* Bounding box */
    min_x = v0x; max_x = v0x;
    if (v1x < min_x) min_x = v1x;
    if (v1x > max_x) max_x = v1x;
    if (v2x < min_x) min_x = v2x;
    if (v2x > max_x) max_x = v2x;
    min_y = v0y; max_y = v0y;
    if (v1y < min_y) min_y = v1y;
    if (v1y > max_y) max_y = v1y;
    if (v2y < min_y) min_y = v2y;
    if (v2y > max_y) max_y = v2y;

    /* Clamp bounding box to clip */
    if ((uint32_t)min_x < effective_clip.min_x)
        min_x = (int32_t)effective_clip.min_x;
    if ((uint32_t)min_y < effective_clip.min_y)
        min_y = (int32_t)effective_clip.min_y;
    if ((uint32_t)(max_x + 1) > effective_clip.max_x)
        max_x = (int32_t)effective_clip.max_x - 1;
    if ((uint32_t)(max_y + 1) > effective_clip.max_y)
        max_y = (int32_t)effective_clip.max_y - 1;

    if (min_x > max_x || min_y > max_y) return SEMU_OK;

    area = edge_func(v0x, v0y, v1x, v1y, v2x, v2y);
    if (area == 0) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "raster: degenerate triangle");
        return SEMU_ERR_UNSUPPORTED;
    }

    for (py = min_y; py <= max_y; ++py) {
        for (px = min_x; px <= max_x; ++px) {
            int32_t e0, e1, e2;
            /* Pixel center at (px+0.5, py+0.5), but we use integer
             * coordinates scaled by 2 to avoid floats. */
            int32_t cx = px * 2 + 1;
            int32_t cy = py * 2 + 1;
            int32_t w0x = v0x * 2, w0y = v0y * 2;
            int32_t w1x = v1x * 2, w1y = v1y * 2;
            int32_t w2x = v2x * 2, w2y = v2y * 2;

            e0 = edge_func(w1x, w1y, w2x, w2y, cx, cy);
            e1 = edge_func(w2x, w2y, w0x, w0y, cx, cy);
            e2 = edge_func(w0x, w0y, w1x, w1y, cx, cy);

            if (area > 0) {
                if (e0 >= 0 && e1 >= 0 && e2 >= 0) {
                    size_t off = (size_t)py * target->stride +
                                 (size_t)px * 2u;
                    target->pixels[off] = (uint8_t)color;
                    target->pixels[off + 1u] = (uint8_t)(color >> 8);
                    ++drawn;
                }
            } else {
                if (e0 <= 0 && e1 <= 0 && e2 <= 0) {
                    size_t off = (size_t)py * target->stride +
                                 (size_t)px * 2u;
                    target->pixels[off] = (uint8_t)color;
                    target->pixels[off + 1u] = (uint8_t)(color >> 8);
                    ++drawn;
                }
            }
        }
    }

    if (dirty != NULL) {
        dirty->min_x = (uint32_t)min_x;
        dirty->min_y = (uint32_t)min_y;
        dirty->max_x = (uint32_t)(max_x + 1);
        dirty->max_y = (uint32_t)(max_y + 1);
    }
    (void)drawn;
    return SEMU_OK;
}
