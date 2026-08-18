/*
 * 1:1 RGB565 texture copy (ticket 512).
 * Reads all source pixels into a staging buffer before writing
 * any target pixel.  Refusal leaves target unchanged.
 */

#include "sampling.h"

#include <stdlib.h>

semu_status draw_texture(raster_target *target,
                         const raster_bounds *clip,
                         semu_bus *bus,
                         const nema_texture_desc *src,
                         uint32_t src_x, uint32_t src_y,
                         uint32_t dst_x, uint32_t dst_y,
                         uint32_t w, uint32_t h,
                         raster_bounds *dirty,
                         semu_error *error)
{
    raster_bounds region;
    int32_t dx, dy;
    uint32_t rw, rh, row, col;
    uint32_t source_dx, source_dy;
    uint16_t *stage;
    semu_status st;

    if (bus == NULL || src == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "draw_texture: null");
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
                       "draw_texture: region %ux%u exceeds max",
                       rw, rh);
        return SEMU_ERR_UNSUPPORTED;
    }

    st = nema_texture_validate(bus, src, error);
    if (st != SEMU_OK) return st;

    /* Check each addition by subtraction so extreme coordinates cannot wrap. */
    source_dx = (uint32_t)dx;
    source_dy = (uint32_t)dy;
    if (src_x > src->width || src_y > src->height ||
        source_dx > src->width - src_x ||
        source_dy > src->height - src_y ||
        rw > src->width - src_x - source_dx ||
        rh > src->height - src_y - source_dy) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "draw_texture: source range exceeds texture");
        return SEMU_ERR_RANGE;
    }

    stage = (uint16_t *)calloc((size_t)rw * rh, sizeof(uint16_t));
    if (stage == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "draw_texture: staging alloc");
        return SEMU_ERR_NOMEM;
    }

    /* Read all source pixels into staging buffer */
    for (row = 0u; row < rh; ++row) {
        for (col = 0u; col < rw; ++col) {
            nema_texel t;
            uint32_t sx = src_x + source_dx + col;
            uint32_t sy = src_y + source_dy + row;
            st = nema_texture_sample(bus, src, sx, sy, &t, error);
            if (st != SEMU_OK) {
                free(stage);
                return st;
            }
            stage[row * rw + col] =
                blend_pack_rgb565(t.r, t.g, t.b);
        }
    }

    /* Commit all pixels to target */
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
