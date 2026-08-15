/*
 * Integer raster primitives (ticket 510).
 * Checked integer RGB565 clear and rectangle operations.
 */

#include "raster.h"

uint16_t raster_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((uint16_t)(r >> 3) << 11) |
                      ((uint16_t)(g >> 2) << 5) |
                      (uint16_t)(b >> 3));
}

static void put_pixel(raster_target *t, uint32_t x, uint32_t y,
                       uint16_t color)
{
    size_t off = (size_t)y * t->stride + (size_t)x * 2u;
    t->pixels[off] = (uint8_t)color;
    t->pixels[off + 1u] = (uint8_t)(color >> 8);
}

static semu_status validate_target(const raster_target *t, semu_error *error)
{
    if (t == NULL || t->pixels == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "raster: null target");
        return SEMU_ERR_ARGUMENT;
    }
    if (t->width == 0u || t->height == 0u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "raster: zero dimensions");
        return SEMU_ERR_ARGUMENT;
    }
    if (t->stride < t->width * 2u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "raster: stride %u < %u", t->stride, t->width * 2u);
        return SEMU_ERR_UNSUPPORTED;
    }
    return SEMU_OK;
}

static void fill_rect(raster_target *t, uint32_t x, uint32_t y,
                       uint32_t w, uint32_t h, uint16_t color)
{
    uint32_t row;
    for (row = 0u; row < h; ++row) {
        uint32_t col;
        for (col = 0u; col < w; ++col) {
            put_pixel(t, x + col, y + row, color);
        }
    }
}

static void set_dirty(raster_bounds *dirty, uint32_t x, uint32_t y,
                       uint32_t w, uint32_t h)
{
    if (dirty != NULL) {
        dirty->min_x = x;
        dirty->min_y = y;
        dirty->max_x = x + w;
        dirty->max_y = y + h;
    }
}

semu_status raster_clear(raster_target *target,
                          uint32_t x, uint32_t y,
                          uint32_t w, uint32_t h,
                          uint16_t color,
                          raster_bounds *dirty,
                          semu_error *error)
{
    semu_status st = validate_target(target, error);
    if (st != SEMU_OK) return st;

    /* Bounds check before any write */
    if (x > target->width || y > target->height ||
        w > target->width - x || h > target->height - y) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "raster: clear (%u,%u,%u,%u) out of bounds %ux%u",
                       x, y, w, h, target->width, target->height);
        return SEMU_ERR_RANGE;
    }

    fill_rect(target, x, y, w, h, color);
    set_dirty(dirty, x, y, w, h);
    return SEMU_OK;
}

semu_status raster_rect(raster_target *target,
                         const raster_bounds *clip,
                         uint32_t x, uint32_t y,
                         uint32_t w, uint32_t h,
                         uint16_t color,
                         raster_bounds *dirty,
                         semu_error *error)
{
    raster_bounds effective_clip;
    raster_bounds rect_bounds;
    uint32_t rx0, ry0, rx1, ry1;

    semu_status st = validate_target(target, error);
    if (st != SEMU_OK) return st;

    /* If no clip provided, use full target */
    if (clip != NULL) {
        if (!raster_clip_intersect(clip, target->width, target->height,
                                     &effective_clip)) {
            /* Empty clip intersection — no-op */
            set_dirty(dirty, 0u, 0u, 0u, 0u);
            return SEMU_OK;
        }
    } else {
        effective_clip.min_x = 0u;
        effective_clip.min_y = 0u;
        effective_clip.max_x = target->width;
        effective_clip.max_y = target->height;
    }

    /* Compute rectangle bounds (exclusive max) */
    /* Check overflow before computing max */
    if (x > target->width || y > target->height) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "raster: rect origin out of bounds");
        return SEMU_ERR_RANGE;
    }
    rx0 = x;
    ry0 = y;
    rx1 = (w > target->width - x) ? target->width : x + w;
    ry1 = (h > target->height - y) ? target->height : y + h;

    /* Intersect with clip */
    if (rx0 < effective_clip.min_x) rx0 = effective_clip.min_x;
    if (ry0 < effective_clip.min_y) ry0 = effective_clip.min_y;
    if (rx1 > effective_clip.max_x) rx1 = effective_clip.max_x;
    if (ry1 > effective_clip.max_y) ry1 = effective_clip.max_y;

    if (rx0 >= rx1 || ry0 >= ry1) {
        set_dirty(dirty, 0u, 0u, 0u, 0u);
        return SEMU_OK;
    }

    fill_rect(target, rx0, ry0, rx1 - rx0, ry1 - ry0, color);
    rect_bounds.min_x = rx0;
    rect_bounds.min_y = ry0;
    rect_bounds.max_x = rx1;
    rect_bounds.max_y = ry1;
    if (dirty != NULL) *dirty = rect_bounds;
    return SEMU_OK;
}
