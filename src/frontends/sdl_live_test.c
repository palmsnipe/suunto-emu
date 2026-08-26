#include "sdl_live_test.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

enum {
    /* The "Continue the setup on your phone" handoff frame settles as step
     * 12.  From that step onward the walk drives the bottom (LOWER) button to
     * skip phone pairing and continue through the remaining setup screens.
     * The post-handoff button sequence is overridable via
     * SEMU_SDL_SETUP_WALK_POST (a run of 'u'/'m'/'l' letters, one per step). */
    SEMU_SDL_SETUP_WALK_PHONE_PAIR_PHASE = 12u,
    SEMU_SDL_SETUP_WALK_QUIT_PHASE = 32u
};

static float post_handoff_button_y(const semu_sdl_live_test *test,
    unsigned post_index, uint32_t viewport_height)
{
    const char *sequence = test->post_buttons;
    char letter = 'l';
    if (sequence != NULL && post_index < strlen(sequence)) {
        letter = sequence[post_index];
    }
    if (letter == 'u') {
        return (float)viewport_height * 0.25f;
    }
    if (letter == 'm') {
        return (float)viewport_height * 0.5f;
    }
    return (float)viewport_height * 0.75f;
}

int semu_sdl_live_test_queue(semu_sdl_live_test *test, int waiting,
    int checkpoint_ready, uint32_t viewport_height,
    uint64_t last_frame_generation, uint32_t last_frame_crc,
    uint64_t virtual_time_ns, semu_error *error)
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
    } else if (test->setup_walk && test->phase >= 2u &&
               test->phase < SEMU_SDL_SETUP_WALK_QUIT_PHASE &&
               checkpoint_ready) {
        if (test->phase >= SEMU_SDL_SETUP_WALK_QUIT_PHASE - 1u) {
            events[0].type = SDL_EVENT_QUIT;
            count = 1u;
        } else {
            float y;
            if (viewport_height == 0u) {
                semu_error_set(error, SEMU_ERR_STATE,
                               "SDL live test has no validated viewport");
                return 0;
            }
            if (test->phase >= SEMU_SDL_SETUP_WALK_PHONE_PAIR_PHASE) {
                y = post_handoff_button_y(
                        test,
                        test->phase - SEMU_SDL_SETUP_WALK_PHONE_PAIR_PHASE,
                        viewport_height);
            } else {
                y = (float)viewport_height / 2.0f;
            }
            events[0].type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            events[0].button.button = SDL_BUTTON_LEFT;
            events[0].button.down = 1;
            events[0].button.y = y;
            events[1].type = SDL_EVENT_MOUSE_BUTTON_UP;
            events[1].button.button = SDL_BUTTON_LEFT;
            events[1].button.down = 0;
            events[1].button.y = y;
            count = 2u;
        }
    } else if (!test->setup_walk &&
               (test->phase == 2u || test->phase == 3u) &&
               checkpoint_ready) {
        if (test->phase == 2u) {
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
    } else {
        return 1;
    }
    if (test->phase != 0u) {
        fprintf(stderr,
                "SDL live test settled step=%u generation=%llu crc32=%08x\n",
                test->phase, (unsigned long long)last_frame_generation,
                last_frame_crc);
        if (test->setup_walk) {
            fprintf(stderr,
                    "SDL live test setup-walk step=%u virtual_ns=%llu\n",
                    test->phase, (unsigned long long)virtual_time_ns);
        }
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
    } else if ((!test->setup_walk && test->phase == 4u) ||
               (test->setup_walk &&
                test->phase == SEMU_SDL_SETUP_WALK_QUIT_PHASE)) {
        if (test->setup_walk) {
            fprintf(stderr,
                    "SDL live test completed setup-navigation "
                    "last-step=%u\n", SEMU_SDL_SETUP_WALK_QUIT_PHASE - 1u);
        } else {
            fputs("SDL live test completed setup-navigation\n", stderr);
        }
    } else if (test->setup_walk) {
        fprintf(stderr, "SDL live test advanced setup step=%u\n", test->phase);
    }
    return 1;
}
