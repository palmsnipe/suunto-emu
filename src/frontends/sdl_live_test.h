#ifndef SEMU_FRONTENDS_SDL_LIVE_TEST_H
#define SEMU_FRONTENDS_SDL_LIVE_TEST_H

#include <stdint.h>

#include "semu/types.h"

typedef struct semu_sdl_live_test {
    int enabled;
    unsigned phase;
} semu_sdl_live_test;

int semu_sdl_live_test_queue(semu_sdl_live_test *test, int waiting,
    int checkpoint_ready, uint32_t viewport_height,
    uint64_t last_frame_generation, uint32_t last_frame_crc,
    semu_error *error);

#endif
