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
    uint32_t byte_val;
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
    /* stride must cover width: stride * 4 >= width */
    if (stride * 4u < width) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "a2le: stride %u too small for width %u",
                       stride, width);
        return SEMU_ERR_UNSUPPORTED;
    }

    byte_offset = y * stride + (x / 4u);
    sample_idx = x & 3u;

    if (base > 0xFFFFFFFFu - byte_offset) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "a2le: address overflow base 0x%08x + %u",
                       base, byte_offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    addr = base + byte_offset;

    st = semu_bus_read(bus, addr, 1u, &byte_val, error);
    if (st != SEMU_OK) return st;

    *alpha = A2LE_LUT[(byte_val >> (sample_idx * 2u)) & 0x03u];
    return SEMU_OK;
}
