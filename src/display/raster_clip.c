/*
 * Raster clipping helper (ticket 510).
 */

#include "raster.h"

int raster_clip_intersect(const raster_bounds *clip,
                           uint32_t target_w, uint32_t target_h,
                           raster_bounds *out)
{
    uint32_t cx0, cy0, cx1, cy1;

    if (clip == NULL || out == NULL) return 0;

    cx0 = clip->min_x;
    cy0 = clip->min_y;
    cx1 = clip->max_x;
    cy1 = clip->max_y;

    /* Clamp to target bounds */
    if (cx0 > target_w) cx0 = target_w;
    if (cy0 > target_h) cy0 = target_h;
    if (cx1 > target_w) cx1 = target_w;
    if (cy1 > target_h) cy1 = target_h;

    if (cx0 >= cx1 || cy0 >= cy1) return 0;

    out->min_x = cx0;
    out->min_y = cy0;
    out->max_x = cx1;
    out->max_y = cy1;
    return 1;
}
