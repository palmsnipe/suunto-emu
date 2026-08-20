/*
 * SDL3 frame presentation core (ticket 515).
 * Dependency-free validation for RGB565LE frame presentation.
 */

#include "sdl_present_core.h"

#include <limits.h>

semu_status sdl_present_core_validate(semu_pixel_format format,
    uint32_t width, uint32_t height,
    uint32_t stride, size_t size,
    uint32_t scale,
    sdl_present_descriptor *out,
    semu_error *error)
{
    uint64_t min_size;

    if (out == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "sdl_present_core: null descriptor");
        return SEMU_ERR_ARGUMENT;
    }
    if (format != SEMU_PIXEL_RGB565_LE) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "sdl_present_core: unsupported pixel format %u",
                       (unsigned)format);
        return SEMU_ERR_UNSUPPORTED;
    }
    if (width == 0u || height == 0u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "sdl_present_core: zero dimensions");
        return SEMU_ERR_ARGUMENT;
    }
    if (width > UINT32_MAX / 2u) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "sdl_present_core: width overflow");
        return SEMU_ERR_RANGE;
    }
    if (stride < width * 2u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "sdl_present_core: stride %u < width*2 %u",
                       stride, width * 2u);
        return SEMU_ERR_ARGUMENT;
    }
    if (scale < SDL_PRESENT_MIN_SCALE ||
        scale > SDL_PRESENT_MAX_SCALE) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "sdl_present_core: scale %u out of range [1,8]",
                       scale);
        return SEMU_ERR_ARGUMENT;
    }
    if ((uint64_t)width * (uint64_t)scale > (uint64_t)INT_MAX ||
        (uint64_t)height * (uint64_t)scale > (uint64_t)INT_MAX) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "sdl_present_core: scaled dimensions exceed int");
        return SEMU_ERR_RANGE;
    }
    if ((uint64_t)stride > (uint64_t)INT_MAX) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "sdl_present_core: stride exceeds int");
        return SEMU_ERR_RANGE;
    }
    min_size = (uint64_t)stride * (uint64_t)height;
    if (min_size > SIZE_MAX) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "sdl_present_core: size overflow");
        return SEMU_ERR_RANGE;
    }
    if (size < (size_t)min_size) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "sdl_present_core: size %zu < expected %llu",
                       size, (unsigned long long)min_size);
        return SEMU_ERR_RANGE;
    }
    out->width = width;
    out->height = height;
    out->stride = stride;
    out->scale = scale;
    out->expected_size = (size_t)min_size;
    return SEMU_OK;
}
