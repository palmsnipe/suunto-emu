/*
 * RGB565 and A2LE blend math (ticket 511).
 * Pure integer functions for straight-alpha tint-over-RGB565 blend.
 */

#include "blend.h"

void blend_unpack_rgb565(uint16_t pixel, rgb8 *out)
{
    uint8_t r5, g6, b5;
    r5 = (uint8_t)((pixel >> 11) & 0x1Fu);
    g6 = (uint8_t)((pixel >> 5) & 0x3Fu);
    b5 = (uint8_t)(pixel & 0x1Fu);
    out->r = (uint8_t)((r5 << 3) | (r5 >> 2));
    out->g = (uint8_t)((g6 << 2) | (g6 >> 4));
    out->b = (uint8_t)((b5 << 3) | (b5 >> 2));
}

uint16_t blend_pack_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t r5 = (uint16_t)(r >> 3);
    uint16_t g6 = (uint16_t)(g >> 2);
    uint16_t b5 = (uint16_t)(b >> 3);
    return (uint16_t)((r5 << 11) | (g6 << 5) | b5);
}

rgb8 blend_tint_from_tex_color(uint32_t tex_color)
{
    rgb8 c;
    c.r = (uint8_t)(tex_color & 0xFFu);
    c.g = (uint8_t)((tex_color >> 8) & 0xFFu);
    c.b = (uint8_t)((tex_color >> 16) & 0xFFu);
    return c;
}

semu_status blend_simple(uint32_t mode, rgb8 src, rgb8 dst,
                          uint8_t alpha, uint16_t *out,
                          semu_error *error)
{
    rgb8 result;

    if (out == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "blend: null output");
        return SEMU_ERR_ARGUMENT;
    }

    if (mode != NEMA_BL_SIMPLE) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "blend: unsupported mode 0x%x", mode);
        return SEMU_ERR_UNSUPPORTED;
    }

    /* Only endpoint coverage is verified (E-NEMA-A2LE-001 missing). */
    if (alpha == 0u) {
        result = dst;
    } else if (alpha == 255u) {
        result = src;
    } else {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "blend: intermediate alpha %u unverified",
                       alpha);
        return SEMU_ERR_UNSUPPORTED;
    }

    *out = blend_pack_rgb565(result.r, result.g, result.b);
    return SEMU_OK;
}
