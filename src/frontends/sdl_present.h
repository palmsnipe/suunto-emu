#ifndef SEMU_FRONTENDS_SDL_PRESENT_H
#define SEMU_FRONTENDS_SDL_PRESENT_H

#include <SDL3/SDL.h>

#include "semu/frame.h"
#include "sdl_present_core.h"

/*
 * SDL3 frame presenter (ticket 515).
 * Owns window/renderer/texture, recreates only on dimension/scale
 * change, uses nearest scaling.  Validated by sdl_present_core;
 * event/input mapping remains in semantic_input (ticket 516).
 */

typedef struct sdl_presenter sdl_presenter;

sdl_presenter *sdl_presenter_create(uint32_t scale, semu_error *error);
void sdl_presenter_destroy(sdl_presenter *presenter);

/*
 * Present a frame.  Validates via sdl_present_core, recreates
 * texture on dimension/scale change, copies pixels using declared
 * stride, and presents.  Returns SEMU_OK or error.
 */
semu_status sdl_presenter_present(sdl_presenter *presenter,
    const semu_frame *frame, semu_error *error);

/*
 * Frontend usability: name the run state in the window title so a
 * stopped session explains itself on screen (the terminal keeps the
 * exact stop line with detail).
 */
void sdl_presenter_set_status(sdl_presenter *presenter, const char *status);

#endif
