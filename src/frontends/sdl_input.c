#include "sdl_input.h"

#include <stdlib.h>
#include <string.h>

struct semu_sdl_input_adapter {
    uint32_t sequence;
    uint32_t viewport_height;
};

static const struct {
    SDL_Scancode scancode;
    uint32_t key;
} default_scan[] = {
    { SDL_SCANCODE_UP,     SEMU_INPUT_KEY_UPPER  },
    { SDL_SCANCODE_RETURN, SEMU_INPUT_KEY_MIDDLE },
    { SDL_SCANCODE_KP_ENTER, SEMU_INPUT_KEY_MIDDLE },
    { SDL_SCANCODE_RETURN2, SEMU_INPUT_KEY_MIDDLE },
    { SDL_SCANCODE_DOWN,   SEMU_INPUT_KEY_LOWER  },
};

semu_sdl_input_adapter *semu_sdl_input_create(semu_error *error)
{
    semu_sdl_input_adapter *a;
    a = (semu_sdl_input_adapter *)calloc(1u, sizeof(*a));
    if (a == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate sdl input adapter");
        return NULL;
    }
    return a;
}

void semu_sdl_input_destroy(semu_sdl_input_adapter *adapter)
{
    free(adapter);
}

void semu_sdl_input_set_viewport_height(semu_sdl_input_adapter *adapter,
    uint32_t height)
{
    if (adapter != NULL) {
        adapter->viewport_height = height;
    }
}

static int mouse_button_key(const semu_sdl_input_adapter *adapter,
    const SDL_Event *event, uint32_t *key)
{
    float third;
    float y;
    if ((event->type != SDL_EVENT_MOUSE_BUTTON_DOWN &&
         event->type != SDL_EVENT_MOUSE_BUTTON_UP) ||
        event->button.button != SDL_BUTTON_LEFT ||
        adapter->viewport_height == 0u) {
        return 0;
    }
    y = event->button.y;
    if (!(y >= 0.0f) ||
        !(y < (float)adapter->viewport_height)) {
        return 0;
    }
    third = (float)adapter->viewport_height / 3.0f;
    *key = y < third ? SEMU_INPUT_KEY_UPPER :
           y < third * 2.0f ? SEMU_INPUT_KEY_MIDDLE :
                              SEMU_INPUT_KEY_LOWER;
    return 1;
}

int semu_sdl_input_process(semu_sdl_input_adapter *adapter,
    const SDL_Event *event, semu_normalized_key *out_key,
    int *out_quit, semu_error *error)
{
    size_t i;
    uint32_t mouse_key;
    if (adapter == NULL || event == NULL || out_key == NULL ||
        out_quit == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "sdl_input_process: null argument");
        return 0;
    }
    *out_quit = 0;
    memset(out_key, 0, sizeof(*out_key));

    if (event->type == SDL_EVENT_QUIT) {
        *out_quit = 1;
        return 0;
    }

    if (mouse_button_key(adapter, event, &mouse_key)) {
        out_key->key = mouse_key;
        out_key->down = event->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        out_key->sequence = adapter->sequence++;
        return 1;
    }

    if (event->type != SDL_EVENT_KEY_DOWN &&
        event->type != SDL_EVENT_KEY_UP) {
        return 0;
    }

    for (i = 0u; i < sizeof(default_scan) / sizeof(default_scan[0]); ++i) {
        if (event->key.scancode == default_scan[i].scancode) {
            out_key->key = default_scan[i].key;
            out_key->down = event->key.down ? 1 : 0;
            out_key->repeat = (int)event->key.repeat;
            out_key->sequence = adapter->sequence++;
            return 1;
        }
    }

    return 0;
}
