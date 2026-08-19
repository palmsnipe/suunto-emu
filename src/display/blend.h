#ifndef SEMU_BLEND_H
#define SEMU_BLEND_H

#include "semu/types.h"

/*
 * RGB565 and A2LE blend math (ticket 511).
 * Pure integer functions for evidence-proven straight-alpha
 * tint-over-RGB565 blend.  No raster loop, texture decoder, or
 * global state.
 *
 * Native mode/coverage is evidenced by E-NEMA-LISTS-001.  The
 * sub-LSB channel rounding below is an explicit software-renderer
 * convention; it is not claimed as hardware-equivalent.
 */

/* Evidenced blend mode IDs. */
#define NEMA_BL_SIMPLE 0x504u

typedef struct {
    uint8_t r, g, b;
} rgb8;

/* Unpack RGB565LE pixel to 8-bit channels. */
void blend_unpack_rgb565(uint16_t pixel, rgb8 *out);

/* Pack 8-bit channels to RGB565LE. */
uint16_t blend_pack_rgb565(uint8_t r, uint8_t g, uint8_t b);

/* Extract tint color from packed RGBA TEX_COLOR word. */
rgb8 blend_tint_from_tex_color(uint32_t tex_color);

/*
 * Straight-alpha blend: Cout = Csrc * alpha + Cdst * (255 - alpha).
 *
 * All 8-bit coverage values are accepted.  The result truncates each
 * channel after the integer 255-denominator blend.
 *
 * Returns SEMU_OK with packed RGB565LE in *out, or
 * SEMU_ERR_UNSUPPORTED for unsupported mode.
 */
semu_status blend_simple(uint32_t mode, rgb8 src, rgb8 dst,
                          uint8_t alpha, uint16_t *out,
                          semu_error *error);

#endif
