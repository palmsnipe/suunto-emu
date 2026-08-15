/*
 * Nema texture descriptor validation and decode (ticket 504).
 * Validates RGB565 and A2LE descriptors, decodes bounded samples.
 */

#include "nema_texture.h"

static uint32_t bytes_per_row(uint32_t format, uint32_t width)
{
    switch (format) {
    case NEMA_TEX_FMT_RGB565: return width * 2u;
    case NEMA_TEX_FMT_A2LE:    return (width + 3u) / 4u;
    default: return 0u;
    }
}

static semu_status validate_format_dims(const nema_texture_desc *d,
                                         semu_error *error)
{
    if (d->format != NEMA_TEX_FMT_RGB565 &&
        d->format != NEMA_TEX_FMT_A2LE) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "texture: unsupported format 0x%02x", d->format);
        return SEMU_ERR_UNSUPPORTED;
    }
    if (d->width == 0u || d->height == 0u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "texture: zero dimensions");
        return SEMU_ERR_ARGUMENT;
    }
    if (d->width > NEMA_TEX_MAX_DIM || d->height > NEMA_TEX_MAX_DIM) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "texture: dimensions %ux%u exceed max %u",
                       d->width, d->height, NEMA_TEX_MAX_DIM);
        return SEMU_ERR_UNSUPPORTED;
    }
    return SEMU_OK;
}

semu_status nema_texture_validate(semu_bus *bus,
                                   const nema_texture_desc *desc,
                                   semu_error *error)
{
    uint32_t min_bpr, total;
    uint32_t last_addr;
    semu_status st;
    uint32_t dummy;

    if (bus == NULL || desc == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "texture: null argument");
        return SEMU_ERR_ARGUMENT;
    }
    st = validate_format_dims(desc, error);
    if (st != SEMU_OK) return st;

    min_bpr = bytes_per_row(desc->format, desc->width);
    if (desc->stride < min_bpr) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "texture: stride %u < min %u for width %u",
                       desc->stride, min_bpr, desc->width);
        return SEMU_ERR_UNSUPPORTED;
    }

    /* Check total size overflow */
    if (desc->height > 0xFFFFFFFFu / desc->stride) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "texture: size overflow stride %u * height %u",
                       desc->stride, desc->height);
        return SEMU_ERR_UNSUPPORTED;
    }
    total = desc->stride * desc->height;
    if (desc->base > 0xFFFFFFFFu - total) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "texture: range overflow base 0x%08x + %u",
                       desc->base, total);
        return SEMU_ERR_UNSUPPORTED;
    }
    last_addr = desc->base + total - 1u;

    /* Confirm bus accessibility of the last byte */
    st = semu_bus_read(bus, last_addr, 1u, &dummy, error);
    if (st != SEMU_OK) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "texture: bus range 0x%08x..0x%08x not accessible",
                       desc->base, last_addr);
        return SEMU_ERR_UNSUPPORTED;
    }
    return SEMU_OK;
}

static semu_status sample_rgb565(semu_bus *bus,
                                  const nema_texture_desc *d,
                                  uint32_t x, uint32_t y,
                                  nema_texel *out, semu_error *error)
{
    uint32_t addr, lo, hi, pixel;
    uint32_t r5, g6, b5;
    semu_status st;

    if (d->stride < d->width * 2u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "rgb565: stride %u too small", d->stride);
        return SEMU_ERR_UNSUPPORTED;
    }
    addr = d->base + y * d->stride + x * 2u;
    st = semu_bus_read(bus, addr, 1u, &lo, error);
    if (st != SEMU_OK) return st;
    st = semu_bus_read(bus, addr + 1u, 1u, &hi, error);
    if (st != SEMU_OK) return st;

    pixel = (lo & 0xFFu) | ((hi & 0xFFu) << 8);
    r5 = (pixel >> 11) & 0x1Fu;
    g6 = (pixel >> 5) & 0x3Fu;
    b5 = pixel & 0x1Fu;
    out->r = (uint8_t)((r5 << 3) | (r5 >> 2));
    out->g = (uint8_t)((g6 << 2) | (g6 >> 4));
    out->b = (uint8_t)((b5 << 3) | (b5 >> 2));
    out->a = 255u;
    return SEMU_OK;
}

semu_status nema_texture_sample(semu_bus *bus,
                                 const nema_texture_desc *desc,
                                 uint32_t x, uint32_t y,
                                 nema_texel *out, semu_error *error)
{
    if (bus == NULL || desc == NULL || out == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "texture: null argument");
        return SEMU_ERR_ARGUMENT;
    }
    if (x >= desc->width || y >= desc->height) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "texture: (%u,%u) out of range %ux%u",
                       x, y, desc->width, desc->height);
        return SEMU_ERR_UNSUPPORTED;
    }
    switch (desc->format) {
    case NEMA_TEX_FMT_RGB565:
        return sample_rgb565(bus, desc, x, y, out, error);
    case NEMA_TEX_FMT_A2LE:
        out->r = 0u; out->g = 0u; out->b = 0u;
        return nema_a2le_sample(bus, desc->base, desc->stride,
                                 desc->width, desc->height,
                                 x, y, &out->a, error);
    default:
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "texture: unsupported format 0x%02x", desc->format);
        return SEMU_ERR_UNSUPPORTED;
    }
}
