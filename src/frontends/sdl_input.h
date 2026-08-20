#ifndef SEMU_FRONTENDS_SDL_INPUT_H
#define SEMU_FRONTENDS_SDL_INPUT_H

#include <SDL3/SDL.h>

#include "semantic_input.h"

/*
 * SDL3 input adapter.  Normalizes SDL events into semu_normalized_key
 * structs and quit signals.  The mapper (semantic_input.c) is SDL-free;
 * only this adapter touches SDL types.
 */
typedef struct semu_sdl_input_adapter semu_sdl_input_adapter;

semu_sdl_input_adapter *semu_sdl_input_create(semu_error *error);
void semu_sdl_input_destroy(semu_sdl_input_adapter *adapter);

/*
 * Set the current drawable height used to map a left click in the upper,
 * middle, or lower screen third to the corresponding watch button.  A zero
 * height disables mouse mapping until a validated frame has been presented.
 */
void semu_sdl_input_set_viewport_height(semu_sdl_input_adapter *adapter,
    uint32_t height);

/*
 * Process one SDL_Event.
 * Returns 1 if a normalized key/button event was produced (*out_key is valid).
 * Returns 0 if no normalized event was produced.
 * Sets *out_quit = 1 when the event is SDL_EVENT_QUIT.
 */
int semu_sdl_input_process(semu_sdl_input_adapter *adapter,
    const SDL_Event *event, semu_normalized_key *out_key,
    int *out_quit, semu_error *error);

#endif
