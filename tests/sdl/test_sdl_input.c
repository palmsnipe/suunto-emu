#include "test.h"

/* The SDL adapter is excluded from libsemu.a. */
#include "../../src/frontends/sdl_input.c"

#include <string.h>

static int process_mouse(semu_sdl_input_adapter *adapter, uint32_t type,
    uint8_t button, float y, semu_normalized_key *key)
{
    SDL_Event event;
    semu_error error;
    int quit = -1;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.button.button = button;
    event.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.y = y;
    semu_error_clear(&error);
    return semu_sdl_input_process(adapter, &event, key, &quit, &error);
}

static void test_keyboard_mapping(semu_test_context *context)
{
    semu_sdl_input_adapter *adapter;
    semu_normalized_key key;
    semu_error error;
    SDL_Event event;
    int quit = -1;
    semu_error_clear(&error);
    adapter = semu_sdl_input_create(&error);
    SEMU_TEST_ASSERT(context, adapter != NULL);
    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_RETURN;
    event.key.down = 1;
    SEMU_TEST_EQ_U64(context, 1u,
        semu_sdl_input_process(adapter, &event, &key, &quit, &error));
    SEMU_TEST_EQ_U64(context, SEMU_INPUT_KEY_MIDDLE, key.key);
    SEMU_TEST_EQ_U64(context, 1u, key.down);
    SEMU_TEST_EQ_U64(context, 0u, key.sequence);
    SEMU_TEST_EQ_U64(context, 0u, quit);
    semu_sdl_input_destroy(adapter);
}

static void test_mouse_thirds_and_release(semu_test_context *context)
{
    semu_sdl_input_adapter *adapter;
    semu_normalized_key key;
    semu_error error;
    semu_error_clear(&error);
    adapter = semu_sdl_input_create(&error);
    SEMU_TEST_ASSERT(context, adapter != NULL);
    semu_sdl_input_set_viewport_height(adapter, 90u);

    SEMU_TEST_EQ_U64(context, 1u,
        process_mouse(adapter, SDL_EVENT_MOUSE_BUTTON_DOWN,
                      SDL_BUTTON_LEFT, 0.0f, &key));
    SEMU_TEST_EQ_U64(context, SEMU_INPUT_KEY_UPPER, key.key);
    SEMU_TEST_EQ_U64(context, 1u, key.down);
    SEMU_TEST_EQ_U64(context, 1u,
        process_mouse(adapter, SDL_EVENT_MOUSE_BUTTON_DOWN,
                      SDL_BUTTON_LEFT, 30.0f, &key));
    SEMU_TEST_EQ_U64(context, SEMU_INPUT_KEY_MIDDLE, key.key);
    SEMU_TEST_EQ_U64(context, 1u,
        process_mouse(adapter, SDL_EVENT_MOUSE_BUTTON_DOWN,
                      SDL_BUTTON_LEFT, 60.0f, &key));
    SEMU_TEST_EQ_U64(context, SEMU_INPUT_KEY_LOWER, key.key);
    SEMU_TEST_EQ_U64(context, 1u,
        process_mouse(adapter, SDL_EVENT_MOUSE_BUTTON_UP,
                      SDL_BUTTON_LEFT, 89.0f, &key));
    SEMU_TEST_EQ_U64(context, SEMU_INPUT_KEY_LOWER, key.key);
    SEMU_TEST_EQ_U64(context, 0u, key.down);
    semu_sdl_input_destroy(adapter);
}

static void test_mouse_mapping_fails_closed(semu_test_context *context)
{
    semu_sdl_input_adapter *adapter;
    semu_normalized_key key;
    semu_error error;
    semu_error_clear(&error);
    adapter = semu_sdl_input_create(&error);
    SEMU_TEST_ASSERT(context, adapter != NULL);
    SEMU_TEST_EQ_U64(context, 0u,
        process_mouse(adapter, SDL_EVENT_MOUSE_BUTTON_DOWN,
                      SDL_BUTTON_LEFT, 1.0f, &key));
    semu_sdl_input_set_viewport_height(adapter, 90u);
    SEMU_TEST_EQ_U64(context, 0u,
        process_mouse(adapter, SDL_EVENT_MOUSE_BUTTON_DOWN,
                      SDL_BUTTON_RIGHT, 45.0f, &key));
    SEMU_TEST_EQ_U64(context, 0u,
        process_mouse(adapter, SDL_EVENT_MOUSE_BUTTON_DOWN,
                      SDL_BUTTON_LEFT, -1.0f, &key));
    SEMU_TEST_EQ_U64(context, 0u,
        process_mouse(adapter, SDL_EVENT_MOUSE_BUTTON_DOWN,
                      SDL_BUTTON_LEFT, 90.0f, &key));
    semu_sdl_input_destroy(adapter);
}

int main(void)
{
    const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_keyboard_mapping),
        SEMU_TEST_CASE(test_mouse_thirds_and_release),
        SEMU_TEST_CASE(test_mouse_mapping_fails_closed)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
