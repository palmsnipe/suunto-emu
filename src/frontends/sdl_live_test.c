#include "sdl_live_test.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

int semu_sdl_live_test_queue(semu_sdl_live_test *test, int waiting,
    int checkpoint_ready, uint32_t viewport_height,
    uint64_t last_frame_generation, uint32_t last_frame_crc,
    semu_error *error)
{
    SDL_Event events[2];
    unsigned count;
    unsigned index;

    if (test == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "SDL live test has no state");
        return 0;
    }
    if (!test->enabled || !waiting) {
        return 1;
    }
    memset(events, 0, sizeof(events));
    if (test->phase == 0u) {
        events[0].type = SDL_EVENT_KEY_DOWN;
        events[0].key.scancode = SDL_SCANCODE_RETURN;
        events[0].key.down = 1;
        events[1].type = SDL_EVENT_KEY_UP;
        events[1].key.scancode = SDL_SCANCODE_RETURN;
        events[1].key.down = 0;
        count = 2u;
    } else if (test->phase == 1u) {
        if (!checkpoint_ready) {
            return 1;
        }
        if (viewport_height == 0u) {
            semu_error_set(error, SEMU_ERR_STATE,
                           "SDL live test has no validated viewport");
            return 0;
        }
        events[0].type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        events[0].button.button = SDL_BUTTON_LEFT;
        events[0].button.down = 1;
        events[0].button.y = (float)viewport_height / 2.0f;
        events[1].type = SDL_EVENT_MOUSE_BUTTON_UP;
        events[1].button.button = SDL_BUTTON_LEFT;
        events[1].button.down = 0;
        events[1].button.y = (float)viewport_height / 2.0f;
        count = 2u;
    } else if ((test->phase == 2u || test->phase == 3u) &&
               checkpoint_ready) {
        if (test->phase == 2u) {
            if (viewport_height == 0u) {
                semu_error_set(error, SEMU_ERR_STATE,
                               "SDL live test has no validated viewport");
                return 0;
            }
            events[0].type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            events[0].button.button = SDL_BUTTON_LEFT;
            events[0].button.down = 1;
            events[0].button.y = (float)viewport_height / 2.0f;
            events[1].type = SDL_EVENT_MOUSE_BUTTON_UP;
            events[1].button.button = SDL_BUTTON_LEFT;
            events[1].button.down = 0;
            events[1].button.y = (float)viewport_height / 2.0f;
            count = 2u;
        } else {
            events[0].type = SDL_EVENT_QUIT;
            count = 1u;
        }
    } else if (test->phase == 4u) {
        return 1;
    } else {
        return 1;
    }
    if (test->phase != 0u) {
        fprintf(stderr,
                "SDL live test settled step=%u generation=%llu crc32=%08x\n",
                test->phase, (unsigned long long)last_frame_generation,
                last_frame_crc);
    }
    for (index = 0u; index < count; ++index) {
        if (!SDL_PushEvent(&events[index])) {
            semu_error_set(error, SEMU_ERR_IO,
                           "SDL live test event injection failed: %s",
                           SDL_GetError());
            return 0;
        }
    }
    ++test->phase;
    if (test->phase == 1u) {
        fprintf(stderr, "SDL live test injected Return/Enter step=%u\n",
                test->phase);
    } else if (test->phase == 2u) {
        fprintf(stderr, "SDL live test injected middle click step=%u\n",
                test->phase);
    } else if (test->phase == 3u) {
        fprintf(stderr, "SDL live test injected middle click step=%u\n",
                test->phase);
    } else {
        fputs("SDL live test completed setup-navigation\n", stderr);
    }
    return 1;
}
