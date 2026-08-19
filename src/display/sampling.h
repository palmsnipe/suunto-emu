#ifndef SEMU_SAMPLING_H
#define SEMU_SAMPLING_H

#include "raster.h"
#include "nema_texture.h"
#include "blend.h"
#include "semu/bus.h"
#include "semu/types.h"

/*
 * Texture sampling and masked draw (ticket 512).
 * Combines verified texture sampling, raster coverage, and blend
 * into atomic draw operations.  All pixels are staged before any
 * target write; refusal leaves target unchanged.
 *
 * Evidence: E-NEMA-LISTS-001, E-NEMA-TEXTURE-001,
 *           E-NEMA-A2LE-001 (missing — endpoint coverage only).
 */

#define SAMPLING_MAX_DIM 240u
#define SAMPLING_MAX_PIXELS (SAMPLING_MAX_DIM * SAMPLING_MAX_DIM)

/* Captured NEMA matrix registers are IEEE-754 binary32 bit patterns. */
typedef struct {
    uint32_t mm00, mm01, mm02;
    uint32_t mm10, mm11, mm12;
} nema_affine_matrix;

/* Shared clip-region helper used by draw_texture and draw_mask.
 * Computes intersection of dst rect with clip and target bounds.
 * Returns adjusted source offsets in *dx, *dy. */
semu_status sampling_compute_clip(raster_target *target,
                                   const raster_bounds *clip,
                                   uint32_t dst_x, uint32_t dst_y,
                                   uint32_t w, uint32_t h,
                                   raster_bounds *out_clip,
                                   int32_t *dx, int32_t *dy,
                                   semu_error *error);

/*
 * 1:1 RGB565 texture copy to target.  All source pixels are read
 * and staged before any target write.  Source coordinates start at
 * (src_x, src_y) and map 1:1 to (dst_x, dst_y) for w*h pixels.
 */
semu_status draw_texture(raster_target *target,
                         const raster_bounds *clip,
                         semu_bus *bus,
                         const nema_texture_desc *src,
                         uint32_t src_x, uint32_t src_y,
                         uint32_t dst_x, uint32_t dst_y,
                         uint32_t w, uint32_t h,
                         raster_bounds *dirty,
                         semu_error *error);

/*
 * A2LE mask draw: for each pixel, sample A2LE coverage and blend
 * tint color (from tex_color) over destination using blend_mode.
 * All observed A2LE coverage levels are staged before target mutation.
 */
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
                      semu_error *error);

/*
 * Evidence-bound affine A2LE draw.  The matrix maps absolute integer
 * destination coordinates to source texture coordinates.  Source samples
 * outside the validated texture are transparent, as in the native path.
 * All output is staged before any target write.
 */
semu_status draw_mask_affine(raster_target *target,
                             const raster_bounds *clip,
                             semu_bus *bus,
                             const nema_texture_desc *mask,
                             uint32_t dst_x, uint32_t dst_y,
                             uint32_t w, uint32_t h,
                             const nema_affine_matrix *matrix,
                             uint32_t blend_mode,
                             uint32_t tex_color,
                             raster_bounds *dirty,
                             semu_error *error);

#endif
