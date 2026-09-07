/*
 * A2LE texture decoder (ticket 504).
 * 4 two-bit alpha samples per byte, least-significant sample first.
 * Endpoint values: 0, 85, 170, 255 (level * 85).
 */

#include "nema_texture.h"

static const uint8_t A2LE_LUT[4u] = { 0u, 85u, 170u, 255u };

semu_status nema_a2le_sample(semu_bus *bus, uint32_t base, uint32_t stride,
                              uint32_t width, uint32_t height,
                              uint32_t x, uint32_t y,
                              uint8_t *alpha, semu_error *error)
{
    uint32_t byte_offset, sample_idx, addr;
    uint8_t byte_val;
    semu_status st;

    if (bus == NULL || alpha == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "a2le: null argument");
        return SEMU_ERR_ARGUMENT;
    }
    if (stride == 0u || width == 0u || height == 0u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "a2le: zero stride/width/height");
        return SEMU_ERR_ARGUMENT;
    }
    if (x >= width || y >= height) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "a2le: coordinate (%u,%u) out of range %ux%u",
                       x, y, width, height);
        return SEMU_ERR_UNSUPPORTED;
    }
    /* Check the multiplication before comparing stride to the width. */
    if (stride > 0xFFFFFFFFu / 4u || stride * 4u < width) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "a2le: stride %u too small for width %u",
                       stride, width);
        return SEMU_ERR_UNSUPPORTED;
    }

    if (y > 0xFFFFFFFFu / stride) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "a2le: row offset overflow y %u * stride %u",
                       y, stride);
        return SEMU_ERR_UNSUPPORTED;
    }
    byte_offset = y * stride;
    if ((x / 4u) > 0xFFFFFFFFu - byte_offset) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "a2le: byte offset overflow row 0x%08x + %u",
                       byte_offset, x / 4u);
        return SEMU_ERR_UNSUPPORTED;
    }
    byte_offset += x / 4u;
    sample_idx = x & 3u;

    if (base > 0xFFFFFFFFu - byte_offset) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "a2le: address overflow base 0x%08x + %u",
                       base, byte_offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    addr = base + byte_offset;

    st = semu_bus_copy_out(bus, addr, &byte_val, 1u, error);
    if (st != SEMU_OK) return st;

    *alpha = A2LE_LUT[(byte_val >> (sample_idx * 2u)) & 0x03u];
    return SEMU_OK;
}

static uint8_t lerp_alpha(uint8_t a, uint8_t b, uint32_t fraction)
{
    uint32_t inverse = 256u - fraction;
    return (uint8_t)(((uint32_t)a * inverse +
                      (uint32_t)b * fraction + 128u) >> 8u);
}

semu_status nema_a2le_sample_bilinear(semu_bus *bus, uint32_t base,
                                      uint32_t stride, uint32_t width,
                                      uint32_t height, uint32_t x_fp8,
                                      uint32_t y_fp8, uint8_t *alpha,
                                      semu_error *error)
{
    uint32_t x0 = x_fp8 >> 8u;
    uint32_t y0 = y_fp8 >> 8u;
    uint32_t x1, y1;
    uint8_t a00, a10, a01, a11;
    uint8_t top, bottom;
    semu_status st;

    if (alpha == NULL || width == 0u || height == 0u ||
        x0 >= width || y0 >= height) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "a2le: bilinear coordinate out of range");
        return SEMU_ERR_UNSUPPORTED;
    }
    x1 = x0 + (x0 + 1u < width ? 1u : 0u);
    y1 = y0 + (y0 + 1u < height ? 1u : 0u);
    st = nema_a2le_sample(bus, base, stride, width, height,
                          x0, y0, &a00, error);
    if (st != SEMU_OK) return st;
    st = nema_a2le_sample(bus, base, stride, width, height,
                          x1, y0, &a10, error);
    if (st != SEMU_OK) return st;
    st = nema_a2le_sample(bus, base, stride, width, height,
                          x0, y1, &a01, error);
    if (st != SEMU_OK) return st;
    st = nema_a2le_sample(bus, base, stride, width, height,
                          x1, y1, &a11, error);
    if (st != SEMU_OK) return st;
    top = lerp_alpha(a00, a10, x_fp8 & 0xFFu);
    bottom = lerp_alpha(a01, a11, x_fp8 & 0xFFu);
    *alpha = lerp_alpha(top, bottom, y_fp8 & 0xFFu);
    return SEMU_OK;
}
