/*
 * A2LE masked draw (ticket 512).
 * Samples A2LE coverage, blends tint over destination, and stages
 * all pixels before any target write.  Intermediate coverage (85,
 * 170) causes atomic refusal — no target mutation.
 */

#include "sampling.h"

#include <stdlib.h>

semu_status draw_mask(raster_target *target,
                      const raster_bounds *clip,
                      semu_bus *bus,
                      const nema_texture_desc *mask,
                      uint32_t mask_x, uint32_t mask_y,
                      uint32_t dst_x, uint32_t dst_y,
                      uint32_t w, uint32_t h,
                      uint32_t blend_mode,
                      uint32_t tex_color,
                      raster_bounds *dirty,
                      semu_error *error)
{
    raster_bounds region;
    int32_t dx, dy;
    uint32_t rw, rh, row, col;
    uint32_t source_dx, source_dy;
    uint16_t *stage;
    rgb8 tint;
    semu_status st;

    if (bus == NULL || mask == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "draw_mask: null");
        return SEMU_ERR_ARGUMENT;
    }

    st = sampling_compute_clip(target, clip, dst_x, dst_y, w, h,
                                &region, &dx, &dy, error);
    if (st != SEMU_OK) return st;

    rw = region.max_x - region.min_x;
    rh = region.max_y - region.min_y;
    if (rw == 0u || rh == 0u) return SEMU_OK;
    if (rw > SAMPLING_MAX_DIM || rh > SAMPLING_MAX_DIM) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "draw_mask: region %ux%u exceeds max", rw, rh);
        return SEMU_ERR_UNSUPPORTED;
    }

    st = nema_texture_validate(bus, mask, error);
    if (st != SEMU_OK) return st;

    /* Check each addition by subtraction so extreme coordinates cannot wrap. */
    source_dx = (uint32_t)dx;
    source_dy = (uint32_t)dy;
    if (mask_x > mask->width || mask_y > mask->height ||
        source_dx > mask->width - mask_x ||
        source_dy > mask->height - mask_y ||
        rw > mask->width - mask_x - source_dx ||
        rh > mask->height - mask_y - source_dy) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "draw_mask: mask range exceeds texture");
        return SEMU_ERR_RANGE;
    }

    if (mask->format != NEMA_TEX_FMT_A2LE) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "draw_mask: mask format must be A2LE");
        return SEMU_ERR_UNSUPPORTED;
    }

    stage = (uint16_t *)calloc((size_t)rw * rh, sizeof(uint16_t));
    if (stage == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "draw_mask: staging alloc");
        return SEMU_ERR_NOMEM;
    }

    tint = blend_tint_from_tex_color(tex_color);

    /* Sample all mask pixels and compute staged output */
    for (row = 0u; row < rh; ++row) {
        for (col = 0u; col < rw; ++col) {
            uint32_t mx = mask_x + source_dx + col;
            uint32_t my = mask_y + source_dy + row;
            uint8_t alpha;
            uint32_t tx = region.min_x + col;
            uint32_t ty = region.min_y + row;
            size_t dst_off = (size_t)ty * target->stride + tx * 2u;
            rgb8 dst_px;
            uint16_t dst_pixel;

            dst_pixel = (uint16_t)(target->pixels[dst_off] |
                                    (target->pixels[dst_off + 1u] << 8));
            blend_unpack_rgb565(dst_pixel, &dst_px);

            if (mask->sampling == NEMA_TEX_SAMPLING_BILINEAR) {
                st = nema_a2le_sample_bilinear(
                    bus, mask->base, mask->stride, mask->width,
                    mask->height, mx * 256u + 128u,
                    my * 256u + 128u, &alpha, error);
            } else {
                st = nema_a2le_sample(bus, mask->base, mask->stride,
                                      mask->width, mask->height,
                                      mx, my, &alpha, error);
            }
            if (st != SEMU_OK) {
                free(stage);
                return st;
            }

            st = blend_simple(blend_mode, tint, dst_px, alpha,
                              &stage[row * rw + col], error);
            if (st != SEMU_OK) {
                free(stage);
                return st;
            }
        }
    }

    /* Commit all staged pixels to target */
    for (row = 0u; row < rh; ++row) {
        for (col = 0u; col < rw; ++col) {
            uint32_t tx = region.min_x + col;
            uint32_t ty = region.min_y + row;
            size_t off = (size_t)ty * target->stride + tx * 2u;
            uint16_t px = stage[row * rw + col];
            target->pixels[off] = (uint8_t)px;
            target->pixels[off + 1u] = (uint8_t)(px >> 8);
        }
    }

    free(stage);
    if (dirty != NULL) *dirty = region;
    return SEMU_OK;
}
