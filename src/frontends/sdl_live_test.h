#ifndef SEMU_FRONTENDS_SDL_LIVE_TEST_H
#define SEMU_FRONTENDS_SDL_LIVE_TEST_H

#include <stdint.h>

#include "semu/types.h"

#define SEMU_SDL_SETUP_WALK_TIMELINE_MAX 64u

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
    /* Optional absolute-virtual-time press schedule (opt-in via
     * SEMU_SDL_SETUP_WALK_TIMELINE). Lets the walk drive screens that never
     * report a settled frame - e.g. the `w-ltim` "Searching for GPS" ring
     * animates continuously, defeating the 350 ms settle window - by firing
     * a button at an exact virtual time. timeline_index is 0=upper,
     * 1=middle, 2=lower. All fields stay zero when unset. */
    unsigned timeline_count;
    uint64_t timeline_ns[SEMU_SDL_SETUP_WALK_TIMELINE_MAX];
    unsigned timeline_index[SEMU_SDL_SETUP_WALK_TIMELINE_MAX];
    unsigned timeline_fired[SEMU_SDL_SETUP_WALK_TIMELINE_MAX];
} semu_sdl_live_test;

int semu_sdl_live_test_queue(semu_sdl_live_test *test, int waiting,
    int checkpoint_ready, uint32_t viewport_height,
    uint64_t last_frame_generation, uint32_t last_frame_crc,
    uint64_t virtual_time_ns, semu_error *error);

/* Parse and store the optional time-scheduled press schedule
 * ("ms:letter[,ms:letter...]", letter one of u/m/l). Entries are stored in
 * non-decreasing time order. SEMU_OK on success, SEMU_ERR_ARGUMENT with
 * *error set on a malformed or over-long spec. An empty spec leaves the
 * test unchanged. */
int semu_sdl_live_test_set_timeline(semu_sdl_live_test *test,
    const char *spec, semu_error *error);

#endif
