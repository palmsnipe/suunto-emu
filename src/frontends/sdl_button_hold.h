#ifndef SEMU_FRONTENDS_SDL_BUTTON_HOLD_H
#define SEMU_FRONTENDS_SDL_BUTTON_HOLD_H

#include "semantic_input.h"

#define SEMU_SDL_BUTTON_HOLD_NS UINT64_C(70000000)
#define SEMU_SDL_BUTTON_COUNT 3u

typedef struct semu_sdl_button_hold {
    semu_normalized_key active_key[SEMU_SDL_BUTTON_COUNT];
    semu_normalized_key pending_release[SEMU_SDL_BUTTON_COUNT];
    uint64_t release_deadline[SEMU_SDL_BUTTON_COUNT];
    uint8_t active[SEMU_SDL_BUTTON_COUNT];
    uint8_t pending[SEMU_SDL_BUTTON_COUNT];
} semu_sdl_button_hold;

void semu_sdl_button_hold_init(semu_sdl_button_hold *hold);
void semu_sdl_button_hold_reset(semu_sdl_button_hold *hold);
void semu_sdl_button_hold_note_press(semu_sdl_button_hold *hold,
    const semu_input_event *input, const semu_normalized_key *key);
int semu_sdl_button_hold_note_release(semu_sdl_button_hold *hold,
    const semu_normalized_key *key, uint64_t now_ns);
uint32_t semu_sdl_button_hold_flush(semu_sdl_button_hold *hold,
    uint64_t now_ns, semu_normalized_key *out_keys, uint32_t max_keys);
int semu_sdl_button_hold_waiting(const semu_sdl_button_hold *hold,
    uint64_t now_ns);

#endif
