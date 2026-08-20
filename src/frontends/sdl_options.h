#ifndef SEMU_FRONTENDS_SDL_OPTIONS_H
#define SEMU_FRONTENDS_SDL_OPTIONS_H

#include "semu/types.h"

/* Parse the optional SDL presentation scale from a frontend argv. */
semu_status semu_sdl_parse_scale(int argc, char *const *argv,
    uint32_t *scale, semu_error *error);

#endif
