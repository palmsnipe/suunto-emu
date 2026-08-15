#ifndef SEMU_BLEND_H
#define SEMU_BLEND_H

#include "semu/types.h"

/*
 * RGB565 and A2LE blend math (ticket 511).
 * Pure integer functions for evidence-proven straight-alpha
 * tint-over-RGB565 blend.  No raster loop, texture decoder, or
 * global state.
 *
 * Evidence: E-NEMA-A2LE-001 (missing — endpoint coverage only).
 * Intermediate-coverage rounding is unverified; only alpha 0/255
 * is accepted for golden comparison.
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
 * Accepted alpha values: 0 (result = dst) and 255 (result = src).
 * Intermediate alpha (85, 170) is refused because rounding is
 * unverified (E-NEMA-A2LE-001 missing).
 *
 * Returns SEMU_OK with packed RGB565LE in *out, or
 * SEMU_ERR_UNSUPPORTED for unverified alpha or unsupported mode.
 */
semu_status blend_simple(uint32_t mode, rgb8 src, rgb8 dst,
                          uint8_t alpha, uint16_t *out,
                          semu_error *error);

#endif
