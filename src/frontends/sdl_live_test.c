#include "sdl_live_test.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    /* The "Continue the setup on your phone" handoff frame settles as step
     * 12.  From that step onward the walk drives the bottom (LOWER) button to
     * skip phone pairing and continue through the remaining setup screens.
     * The post-handoff button sequence is overridable via
     * SEMU_SDL_SETUP_WALK_POST (a run of 'u'/'m'/'l' letters, one per step). */
    SEMU_SDL_SETUP_WALK_PHONE_PAIR_PHASE = 12u,
    SEMU_SDL_SETUP_WALK_QUIT_PHASE = 32u,
    /* Virtual-time spacing between same-frame repeat presses. Mirrors the
     * ~350 ms live-frame settle window so a re-press lands only after the
     * previous press's transition has had a full settle window to produce a
     * new frame. */
    SEMU_SDL_SETUP_WALK_REPEAT_INTERVAL_NS = 400000000u
};

static float timeline_button_y(unsigned index, uint32_t viewport_height)
{
    if (index == 0u) {
        return (float)viewport_height * 0.25f;
    }
    if (index == 1u) {
        return (float)viewport_height * 0.5f;
    }
    return (float)viewport_height * 0.75f;
}

int semu_sdl_live_test_set_timeline(semu_sdl_live_test *test,
    const char *spec, semu_error *error)
{
    unsigned count = 0u;
    unsigned i, j;
    const char *p;
    if (test == NULL || spec == NULL || error == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "SDL live test timeline has no argument");
        return SEMU_ERR_ARGUMENT;
    }
    p = spec;
    while (*p != '\0') {
        char *end;
        unsigned long ms;
        char letter;
        const char *next;
        if (count >= SEMU_SDL_SETUP_WALK_TIMELINE_MAX) {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                           "SDL live test timeline has too many entries "
                           "(max 64)");
            return SEMU_ERR_ARGUMENT;
        }
        ms = strtoul(p, &end, 10);
        if (end == p || *end != ':') {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                           "SDL live test timeline entry must be ms:letter");
            return SEMU_ERR_ARGUMENT;
        }
        letter = end[1];
        if (letter != 'u' && letter != 'm' && letter != 'l') {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                           "SDL live test timeline button letter must be "
                           "u, m, or l");
            return SEMU_ERR_ARGUMENT;
        }
        /* The character right after the letter must end the entry (either
         * the string or the next-entry separator); anything else is
         * trailing garbage. */
        next = end + 2;
        if (*next != '\0' && *next != ',') {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                           "SDL live test timeline entry must be ms:letter");
            return SEMU_ERR_ARGUMENT;
        }
        test->timeline_ns[count] = (uint64_t)ms * 1000000uL;
        test->timeline_index[count] =
            (letter == 'u') ? 0u : (letter == 'm') ? 1u : 2u;
        test->timeline_fired[count] = 0u;
        ++count;
        p = (*next == ',') ? next + 1 : next;
    }
    for (i = 1u; i < count; ++i) {
        for (j = i; j > 0u; --j) {
            if (test->timeline_ns[j] >= test->timeline_ns[j - 1u]) {
                break;
            }
            {
                uint64_t tns = test->timeline_ns[j];
                unsigned ti = test->timeline_index[j];
                unsigned tf = test->timeline_fired[j];
                test->timeline_ns[j] = test->timeline_ns[j - 1u];
                test->timeline_index[j] = test->timeline_index[j - 1u];
                test->timeline_fired[j] = test->timeline_fired[j - 1u];
                test->timeline_ns[j - 1u] = tns;
                test->timeline_index[j - 1u] = ti;
                test->timeline_fired[j - 1u] = tf;
            }
        }
    }
    test->timeline_count = count;
    return SEMU_OK;
}

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
    if (!test->enabled) {
        return 1;
    }
    memset(events, 0, sizeof(events));
    /* Absolute-virtual-time presses (opt-in). Fires the first due, unfired
     * timeline entry at its exact virtual time, independent of the settle
     * window `waiting`, so it can drive screens that animate continuously
     * (the `w-ltim` "Searching for GPS" ring never reports a settled frame).
     * Entries are parsed in non-decreasing time order. */
    if (test->setup_walk && test->timeline_count > 0u) {
        for (index = 0u; index < test->timeline_count; ++index) {
            if (test->timeline_fired[index]) {
                continue;
            }
            if (virtual_time_ns < test->timeline_ns[index]) {
                break;
            }
            if (viewport_height == 0u) {
                semu_error_set(error, SEMU_ERR_STATE,
                               "SDL live test has no validated viewport");
                return 0;
            }
            events[0].type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            events[0].button.button = SDL_BUTTON_LEFT;
            events[0].button.down = 1;
            events[0].button.y =
                timeline_button_y(test->timeline_index[index],
                                  viewport_height);
            events[1].type = SDL_EVENT_MOUSE_BUTTON_UP;
            events[1].button.button = SDL_BUTTON_LEFT;
            events[1].button.down = 0;
            events[1].button.y =
                timeline_button_y(test->timeline_index[index],
                                  viewport_height);
            count = 2u;
            test->timeline_fired[index] = 1u;
            fprintf(stderr,
                    "SDL live test timeline press index=%u virtual_ns=%llu\n",
                    index, (unsigned long long)virtual_time_ns);
            for (index = 0u; index < count; ++index) {
                if (!SDL_PushEvent(&events[index])) {
                    semu_error_set(error, SEMU_ERR_IO,
                                   "SDL live test event injection failed: %s",
                                   SDL_GetError());
                    return 0;
                }
            }
            return 1;
        }
    }
    if (!waiting) {
        return 1;
    }
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
               test->phase < SEMU_SDL_SETUP_WALK_QUIT_PHASE) {
        if (checkpoint_ready) {
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
                /* Track the press so the same-frame re-press path can re-issue
                 * the identical button when the frame does not settle to a new
                 * distinct frame (viewset / spinner panels). */
                test->last_button_y = y;
                test->pressed_crc = last_frame_crc;
                test->same_frame_presses = 0u;
                test->last_press_time_ns = virtual_time_ns;
            }
        } else if (test->repeat_max > 0u &&
                   test->last_press_time_ns != 0u &&
                   test->same_frame_presses < test->repeat_max &&
                   virtual_time_ns >= test->last_press_time_ns &&
                   virtual_time_ns - test->last_press_time_ns >=
                       SEMU_SDL_SETUP_WALK_REPEAT_INTERVAL_NS &&
                   last_frame_crc == test->pressed_crc &&
                   viewport_height != 0u) {
            /* Re-issue the same button on the unchanged frame. */
            events[0].type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            events[0].button.button = SDL_BUTTON_LEFT;
            events[0].button.down = 1;
            events[0].button.y = test->last_button_y;
            events[1].type = SDL_EVENT_MOUSE_BUTTON_UP;
            events[1].button.button = SDL_BUTTON_LEFT;
            events[1].button.down = 0;
            events[1].button.y = test->last_button_y;
            count = 2u;
            ++test->same_frame_presses;
            test->last_press_time_ns = virtual_time_ns;
        } else {
            return 1;
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
