/*
 * Texture sampling helper (ticket 512).
 * Nearest-neighbor sampling for RGB565 and A2LE textures.
 */

#include "sampling.h"

#include <limits.h>

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
    raster_bounds effective_clip, result = {0u, 0u, 0u, 0u};
    uint32_t rx0, ry0, rx1, ry1;
    int32_t dx = 0, dy = 0;

    if (target == NULL || target->pixels == NULL || out_clip == NULL ||
        dx_adjust == NULL || dy_adjust == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "sampling: null argument");
        return SEMU_ERR_ARGUMENT;
    }
    if (target->width == 0u || target->height == 0u ||
        target->width > UINT32_MAX / 2u ||
        target->stride < target->width * 2u ||
        (uint64_t)target->stride * target->height > SIZE_MAX) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "sampling: invalid target layout");
        return SEMU_ERR_UNSUPPORTED;
    }
    if (w > UINT32_MAX - dst_x || h > UINT32_MAX - dst_y ||
        (clip != NULL && (clip->min_x > clip->max_x || clip->min_y > clip->max_y))) {
        semu_error_set(error, SEMU_ERR_RANGE, "sampling: invalid rectangle bounds");
        return SEMU_ERR_RANGE;
    }

    if (clip != NULL) {
        if (!raster_clip_intersect(clip, target->width, target->height,
                                     &effective_clip)) {
            goto commit;
        }
    } else {
        effective_clip.min_x = 0u;
        effective_clip.min_y = 0u;
        effective_clip.max_x = target->width;
        effective_clip.max_y = target->height;
    }

    rx0 = dst_x;
    ry0 = dst_y;
    rx1 = dst_x + w;
    ry1 = dst_y + h;

    if (rx0 < effective_clip.min_x) rx0 = effective_clip.min_x;
    if (ry0 < effective_clip.min_y) ry0 = effective_clip.min_y;
    if (rx1 > effective_clip.max_x) rx1 = effective_clip.max_x;
    if (ry1 > effective_clip.max_y) ry1 = effective_clip.max_y;

    /* A valid offscreen draw has no coverage, not an inverted region.
     * E-SAP-UI-239-001: native language-transition glyphs cross the viewport. */
    if (rx0 >= rx1 || ry0 >= ry1) goto commit;
    if (rx0 - dst_x > INT32_MAX || ry0 - dst_y > INT32_MAX) {
        semu_error_set(error, SEMU_ERR_RANGE, "sampling: source adjustment out of range");
        return SEMU_ERR_RANGE;
    }
    result.min_x = rx0; result.min_y = ry0;
    result.max_x = rx1; result.max_y = ry1;
    dx = (int32_t)(rx0 - dst_x);
    dy = (int32_t)(ry0 - dst_y);
commit:
    *out_clip = result;
    *dx_adjust = dx;
    *dy_adjust = dy;
    semu_error_clear(error);
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
