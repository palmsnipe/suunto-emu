#include "semu/display.h"

#include <stdlib.h>
#include <string.h>

struct semu_surface {
    semu_frame frame;
    uint8_t *pixels;
};

semu_surface *semu_surface_create(uint32_t width, uint32_t height,
                                  semu_error *error)
{
    semu_surface *surface;
    size_t size;
    if (width == 0u || height == 0u || width > UINT32_MAX / 2u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid surface dimensions");
        return NULL;
    }
    size = (size_t)width * (size_t)height * 2u;
    if (height != 0u && size / height / 2u != width) {
        semu_error_set(error, SEMU_ERR_RANGE, "surface size overflow");
        return NULL;
    }
    surface = (semu_surface *)calloc(1u, sizeof(*surface));
    if (surface == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate surface");
        return NULL;
    }
    surface->pixels = (uint8_t *)calloc(size, 1u);
    if (surface->pixels == NULL) {
        free(surface);
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate surface pixels");
        return NULL;
    }
    surface->frame.format = SEMU_PIXEL_RGB565_LE;
    surface->frame.width = width;
    surface->frame.height = height;
    surface->frame.stride = width * 2u;
    surface->frame.pixels = surface->pixels;
    surface->frame.size = size;
    return surface;
}

void semu_surface_destroy(semu_surface *surface)
{
    if (surface != NULL) {
        free(surface->pixels);
        free(surface);
    }
}

void semu_surface_clear(semu_surface *surface, uint16_t rgb565)
{
    size_t i;
    if (surface == NULL) {
        return;
    }
    for (i = 0u; i < surface->frame.size; i += 2u) {
        surface->pixels[i] = (uint8_t)rgb565;
        surface->pixels[i + 1u] = (uint8_t)(rgb565 >> 8u);
    }
    surface->frame.generation++;
}

semu_status semu_surface_write(semu_surface *surface, uint32_t x, uint32_t y,
                               uint32_t width, uint32_t height,
                               const uint8_t *rgb565_le, uint32_t stride,
                               semu_error *error)
{
    uint32_t row;
    if (surface == NULL || rgb565_le == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "surface write argument is null");
        return SEMU_ERR_ARGUMENT;
    }
    if (width == 0u || height == 0u || stride < width * 2u ||
        x > surface->frame.width || y > surface->frame.height ||
        width > surface->frame.width - x || height > surface->frame.height - y) {
        semu_error_set(error, SEMU_ERR_RANGE, "surface write is out of bounds");
        return SEMU_ERR_RANGE;
    }
    for (row = 0u; row < height; ++row) {
        size_t destination = (size_t)(y + row) * surface->frame.stride + x * 2u;
        memcpy(surface->pixels + destination,
               rgb565_le + (size_t)row * stride, (size_t)width * 2u);
    }
    surface->frame.generation++;
    return SEMU_OK;
}

const semu_frame *semu_surface_frame(semu_surface *surface)
{
    return surface != NULL ? &surface->frame : NULL;
}
