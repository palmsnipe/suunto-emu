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

static semu_status validate_sampling(const nema_texture_desc *d,
                                     semu_error *error)
{
    if (d->sampling != NEMA_TEX_SAMPLING_NEAREST &&
        d->sampling != NEMA_TEX_SAMPLING_BILINEAR) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "texture: sampling mode 0x%02x unsupported",
                       d->sampling);
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
    uint8_t dummy;

    if (bus == NULL || desc == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "texture: null argument");
        return SEMU_ERR_ARGUMENT;
    }
    st = validate_format_dims(desc, error);
    if (st != SEMU_OK) return st;
    st = validate_sampling(desc, error);
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

    /* A non-mutating boundary check; each sampled byte is checked separately. */
    return semu_bus_copy_out(bus, last_addr, &dummy, 1u, error);
}

static semu_status sample_rgb565(semu_bus *bus,
                                  const nema_texture_desc *d,
                                  uint32_t x, uint32_t y,
                                  nema_texel *out, semu_error *error)
{
    uint32_t addr, pixel;
    uint8_t lo, hi;
    uint32_t r5, g6, b5;
    semu_status st;

    if (d->stride < d->width * 2u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "rgb565: stride %u too small", d->stride);
        return SEMU_ERR_UNSUPPORTED;
    }
    addr = d->base + y * d->stride + x * 2u;
    /* Keep byte granularity: a two-byte copy could bypass a one-byte overlay. */
    st = semu_bus_copy_out(bus, addr, &lo, 1u, error);
    if (st != SEMU_OK) return st;
    st = semu_bus_copy_out(bus, addr + 1u, &hi, 1u, error);
    if (st != SEMU_OK) return st;

    pixel = (uint32_t)lo | (uint32_t)hi << 8u;
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
    case NEMA_TEX_FMT_A2LE: {
        uint8_t alpha;
        semu_status st = nema_a2le_sample(bus, desc->base, desc->stride,
            desc->width, desc->height, x, y, &alpha, error);
        if (st != SEMU_OK) return st;
        *out = (nema_texel){0u, 0u, 0u, alpha};
        return SEMU_OK;
    }
    default:
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "texture: unsupported format 0x%02x", desc->format);
        return SEMU_ERR_UNSUPPORTED;
    }
}
