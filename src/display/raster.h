#ifndef SEMU_RASTER_H
#define SEMU_RASTER_H

#include "semu/types.h"

/*
 * Integer raster primitives and clipping (ticket 510).
 * Checked integer RGB565 clear/rectangle/triangle with top-left edge
 * rule.  No texture, blend, publication, or floating point.
 *
 * Evidence: E-NEMA-LISTS-001 (first-frame clear/clip/geometry).
 * Partial-edge triangles are refused when bit-identical edge rule
 * is not established.
 */

#define RASTER_FP_SHIFT 16
#define RASTER_FP_ONE   (1u << RASTER_FP_SHIFT)
#define RASTER_FP_FRAC_MASK (RASTER_FP_ONE - 1u)

typedef struct {
    uint8_t *pixels;   /* RGB565LE, 2 bytes per pixel, borrowed */
    uint32_t width;
    uint32_t height;
    uint32_t stride;   /* bytes per row */
} raster_target;

typedef struct {
    uint32_t min_x, min_y;
    uint32_t max_x, max_y;   /* exclusive */
} raster_bounds;

/* Pack an RGB565 color. */
uint16_t raster_rgb565(uint8_t r, uint8_t g, uint8_t b);

/* Clear a rectangular region of the target with a solid color.
 * Writes dirty bounds if dirty is non-NULL. */
semu_status raster_clear(raster_target *target,
                          uint32_t x, uint32_t y,
                          uint32_t w, uint32_t h,
                          uint16_t color,
                          raster_bounds *dirty,
                          semu_error *error);

/* Draw a clipped solid rectangle.  Clip bounds are intersected with
 * the target bounds.  Empty intersection is a no-op (not an error). */
semu_status raster_rect(raster_target *target,
                         const raster_bounds *clip,
                         uint32_t x, uint32_t y,
                         uint32_t w, uint32_t h,
                         uint16_t color,
                         raster_bounds *dirty,
                         semu_error *error);

/* Compute intersection of clip and target bounds.
 * Returns 1 if non-empty, 0 if empty. */
int raster_clip_intersect(const raster_bounds *clip,
                           uint32_t target_w, uint32_t target_h,
                           raster_bounds *out);

/* Draw an axis-aligned triangle from 16.16 fixed-point vertices.
 * Only accepts triangles where all vertices have integer coordinates
 * and at least one horizontal and one vertical edge exist (full
 * pixel coverage, no partial edges).  Other triangles are refused. */
semu_status raster_triangle(raster_target *target,
                             const raster_bounds *clip,
                             uint32_t p0x, uint32_t p0y,
                             uint32_t p1x, uint32_t p1y,
                             uint32_t p2x, uint32_t p2y,
                             uint16_t color,
                             raster_bounds *dirty,
                             semu_error *error);

#endif
