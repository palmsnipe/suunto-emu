#include "sdl_button_hold.h"

#include <limits.h>
#include <string.h>

static uint64_t release_deadline(uint64_t now_ns)
{
    if (now_ns > UINT64_MAX - SEMU_SDL_BUTTON_HOLD_NS) {
        return UINT64_MAX;
    }
    return now_ns + SEMU_SDL_BUTTON_HOLD_NS;
}

int semu_sdl_button_hold_press_allowed(
    const semu_sdl_button_hold *hold, const semu_normalized_key *key,
    uint64_t now_ns)
{
    uint32_t button;
    if (hold == NULL || key == NULL || !key->down || key->repeat) {
        return 0;
    }
    for (button = 0u; button < SEMU_SDL_BUTTON_COUNT; ++button) {
        if (hold->active_key[button].key == key->key &&
            (hold->pending[button] ||
             now_ns < hold->stable_deadline[button])) {
            return 0;
        }
    }
    return 1;
}

void semu_sdl_button_hold_init(semu_sdl_button_hold *hold)
{
    if (hold != NULL) {
        memset(hold, 0, sizeof(*hold));
    }
}

void semu_sdl_button_hold_reset(semu_sdl_button_hold *hold)
{
    semu_sdl_button_hold_init(hold);
}

void semu_sdl_button_hold_note_press(semu_sdl_button_hold *hold,
    const semu_input_event *input, const semu_normalized_key *key)
{
    uint32_t button;
    if (hold == NULL || input == NULL || key == NULL ||
        input->kind != SEMU_INPUT_BUTTON || input->value != 0 ||
        input->code >= SEMU_SDL_BUTTON_COUNT) {
        return;
    }
    button = input->code;
    if (!hold->active[button]) {
        hold->active_key[button] = *key;
        hold->active[button] = 1u;
    }
}

int semu_sdl_button_hold_note_release(semu_sdl_button_hold *hold,
    const semu_normalized_key *key, uint64_t now_ns)
{
    uint32_t button;
    if (hold == NULL || key == NULL || key->down || key->repeat) {
        return 0;
    }
    for (button = 0u; button < SEMU_SDL_BUTTON_COUNT; ++button) {
        if (hold->active[button] &&
            hold->active_key[button].key == key->key) {
            if (!hold->pending[button]) {
                hold->pending_release[button] = *key;
                hold->pending_release[button].down = 0;
                hold->release_deadline[button] = release_deadline(now_ns);
                hold->pending[button] = 1u;
            }
            return 1;
        }
    }
    return 0;
}

uint32_t semu_sdl_button_hold_flush(semu_sdl_button_hold *hold,
    uint64_t now_ns, semu_normalized_key *out_keys, uint32_t max_keys)
{
    uint32_t button;
    uint32_t due_count = 0u;
    uint32_t count = 0u;
    if (hold == NULL || out_keys == NULL) {
        return 0u;
    }
    for (button = 0u; button < SEMU_SDL_BUTTON_COUNT; ++button) {
        if (hold->pending[button] &&
            now_ns >= hold->release_deadline[button]) {
            ++due_count;
        }
    }
    if (due_count > max_keys) {
        return 0u;
    }
    for (button = 0u; button < SEMU_SDL_BUTTON_COUNT; ++button) {
        if (hold->pending[button] &&
            now_ns >= hold->release_deadline[button]) {
            if (count < max_keys) {
                out_keys[count++] = hold->pending_release[button];
            }
            hold->pending[button] = 0u;
            hold->active[button] = 0u;
            hold->stable_deadline[button] = release_deadline(now_ns);
        }
    }
    return count;
}

int semu_sdl_button_hold_waiting(const semu_sdl_button_hold *hold,
    uint64_t now_ns)
{
    uint32_t button;
    if (hold == NULL) {
        return 0;
    }
    for (button = 0u; button < SEMU_SDL_BUTTON_COUNT; ++button) {
        if (hold->pending[button] &&
            now_ns < hold->release_deadline[button]) {
            return 1;
        }
    }
    return 0;
}
