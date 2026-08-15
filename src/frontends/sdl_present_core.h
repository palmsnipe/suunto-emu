#ifndef SEMU_FRONTENDS_SDL_PRESENT_CORE_H
#define SEMU_FRONTENDS_SDL_PRESENT_CORE_H

#include "semu/frame.h"
#include "semu/types.h"

#include <stddef.h>

/*
 * SDL3 frame presentation core (ticket 515).
 * Dependency-free validation and normalization for RGB565LE frame
 * presentation.  No SDL types; the SDL presenter (sdl_present.c)
 * wraps this core.
 */

#define SDL_PRESENT_MIN_SCALE 1u
#define SDL_PRESENT_MAX_SCALE 8u

/* Normalized presentation descriptor (no SDL types). */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t scale;
    size_t expected_size;
} sdl_present_descriptor;

/*
 * Validate frame parameters for presentation.
 * Checks: RGB565LE format, nonzero dimensions, stride >= width*2,
 * size >= stride*height (overflow-safe), scale 1-8.
 * On success, fills *out with normalized descriptor.
 */
semu_status sdl_present_core_validate(semu_pixel_format format,
    uint32_t width, uint32_t height,
    uint32_t stride, size_t size,
    uint32_t scale,
    sdl_present_descriptor *out,
    semu_error *error);

#endif
