#ifndef SEMU_FRONTENDS_SDL_LIVE_TEST_H
#define SEMU_FRONTENDS_SDL_LIVE_TEST_H

#include <stdint.h>

#include "semu/types.h"

typedef struct semu_sdl_live_test {
    int enabled;
    int setup_walk;
    unsigned phase;
    const char *post_buttons;
    /* Optional same-frame repeat-press (opt-in via
     * SEMU_SDL_SETUP_WALK_REPEAT). The onboarding `w-tida` viewset and the
     * time-entry spinners need several presses on the SAME settled frame,
     * which the frame-stepped walk otherwise cannot drive. All fields below
     * stay zero when the feature is off, keeping the default walk byte-
     * identical. */
    float last_button_y;
    uint32_t pressed_crc;
    uint64_t last_press_time_ns;
    unsigned same_frame_presses;
    unsigned repeat_max;
} semu_sdl_live_test;

int semu_sdl_live_test_queue(semu_sdl_live_test *test, int waiting,
    int checkpoint_ready, uint32_t viewport_height,
    uint64_t last_frame_generation, uint32_t last_frame_crc,
    uint64_t virtual_time_ns, semu_error *error);

#endif
