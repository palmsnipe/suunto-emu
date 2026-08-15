/*
 * Texture sampling helper (ticket 512).
 * Nearest-neighbor sampling for RGB565 and A2LE textures.
 */

#include "sampling.h"

#include <stdlib.h>

/*
 * Compute the intersection of the destination rectangle with clip
 * and target bounds.  Returns the clipped destination region and
 * the corresponding source offset adjustment.
 */
static semu_status compute_clip_region(raster_target *target,
                                        const raster_bounds *clip,
                                        uint32_t dst_x, uint32_t dst_y,
                                        uint32_t w, uint32_t h,
                                        raster_bounds *out_clip,
                                        int32_t *dx_adjust,
                                        int32_t *dy_adjust,
                                        semu_error *error)
{
    raster_bounds effective_clip;
    uint32_t rx0, ry0, rx1, ry1;

    if (target == NULL || target->pixels == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "sampling: null target");
        return SEMU_ERR_ARGUMENT;
    }
    if (target->stride < target->width * 2u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "sampling: bad stride");
        return SEMU_ERR_UNSUPPORTED;
    }

    if (clip != NULL) {
        if (!raster_clip_intersect(clip, target->width, target->height,
                                     &effective_clip)) {
            out_clip->min_x = 0u; out_clip->min_y = 0u;
            out_clip->max_x = 0u; out_clip->max_y = 0u;
            *dx_adjust = 0;
            *dy_adjust = 0;
            return SEMU_OK;
        }
    } else {
        effective_clip.min_x = 0u;
        effective_clip.min_y = 0u;
        effective_clip.max_x = target->width;
        effective_clip.max_y = target->height;
    }

    if (dst_x > target->width || dst_y > target->height) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "sampling: dst origin out of bounds");
        return SEMU_ERR_RANGE;
    }

    rx0 = dst_x;
    ry0 = dst_y;
    rx1 = (w > target->width - dst_x) ? target->width : dst_x + w;
    ry1 = (h > target->height - dst_y) ? target->height : dst_y + h;

    if (rx0 < effective_clip.min_x) rx0 = effective_clip.min_x;
    if (ry0 < effective_clip.min_y) ry0 = effective_clip.min_y;
    if (rx1 > effective_clip.max_x) rx1 = effective_clip.max_x;
    if (ry1 > effective_clip.max_y) ry1 = effective_clip.max_y;

    out_clip->min_x = rx0;
    out_clip->min_y = ry0;
    out_clip->max_x = rx1;
    out_clip->max_y = ry1;
    *dx_adjust = (int32_t)rx0 - (int32_t)dst_x;
    *dy_adjust = (int32_t)ry0 - (int32_t)dst_y;
    return SEMU_OK;
}

/* Shared helper used by draw_texture and draw_mask. */
semu_status sampling_compute_clip(raster_target *target,
                                   const raster_bounds *clip,
                                   uint32_t dst_x, uint32_t dst_y,
                                   uint32_t w, uint32_t h,
                                   raster_bounds *out_clip,
                                   int32_t *dx, int32_t *dy,
                                   semu_error *error)
{
    return compute_clip_region(target, clip, dst_x, dst_y, w, h,
                               out_clip, dx, dy, error);
}
