#include "test.h"

/* The SDL adapter is excluded from libsemu.a. */
#include "../../src/frontends/sdl_input.c"

/* The setup-walk driver is excluded from libsemu.a. as well. */
#include "../../src/frontends/sdl_live_test.c"

#include <stdio.h>
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

static void test_setup_walk_timeline_parse(semu_test_context *context)
{
    semu_sdl_live_test test;
    semu_error error;
    semu_error_clear(&error);
    memset(&test, 0, sizeof(test));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sdl_live_test_set_timeline(&test, "30000:l,100:m", &error));
    SEMU_TEST_EQ_U64(context, 2u, test.timeline_count);
    /* Entries are stored in non-decreasing virtual-time order. */
    SEMU_TEST_EQ_U64(context, 100000000ull, test.timeline_ns[0]);
    SEMU_TEST_EQ_U64(context, 1u, test.timeline_index[0]);
    SEMU_TEST_EQ_U64(context, 0u, test.timeline_fired[0]);
    SEMU_TEST_EQ_U64(context, 30000000000ull, test.timeline_ns[1]);
    SEMU_TEST_EQ_U64(context, 2u, test.timeline_index[1]);
    SEMU_TEST_EQ_U64(context, 0u, test.timeline_fired[1]);
    semu_error_clear(&error);
    memset(&test, 0, sizeof(test));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sdl_live_test_set_timeline(&test, "500:u", &error));
    SEMU_TEST_EQ_U64(context, 1u, test.timeline_count);
    SEMU_TEST_EQ_U64(context, 500000000ull, test.timeline_ns[0]);
    SEMU_TEST_EQ_U64(context, 0u, test.timeline_index[0]);
    semu_error_clear(&error);
    memset(&test, 0, sizeof(test));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sdl_live_test_set_timeline(&test, "", &error));
    SEMU_TEST_EQ_U64(context, 0u, test.timeline_count);
}

static void test_setup_walk_timeline_parse_fails_closed(semu_test_context *context)
{
    semu_sdl_live_test test;
    semu_error error;
    const char *invalid[] = {
        "30000",
        "30000:",
        "30000:x",
        "30000:ll",
        "30000:l,32000",
        "30000:l extra",
        ",30000:l",
        NULL
    };
    for (unsigned i = 0u; i < SEMU_ARRAY_LEN(invalid); ++i) {
        semu_error_clear(&error);
        memset(&test, 0, sizeof(test));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
            semu_sdl_live_test_set_timeline(&test, invalid[i], &error));
        SEMU_TEST_ASSERT(context, error.text[0] != '\0');
        SEMU_TEST_EQ_U64(context, 0u, test.timeline_count);
    }
    {
        char spec[512];
        size_t len = 0u;
        for (int i = 0; i < 65; ++i) {
            len += (size_t)sprintf(spec + len, "%d:m,", i);
        }
        spec[len - 1u] = '\0';
        semu_error_clear(&error);
        memset(&test, 0, sizeof(test));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
            semu_sdl_live_test_set_timeline(&test, spec, &error));
    }
}

int main(void)
{
    const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_keyboard_mapping),
        SEMU_TEST_CASE(test_mouse_thirds_and_release),
        SEMU_TEST_CASE(test_mouse_mapping_fails_closed),
        SEMU_TEST_CASE(test_setup_walk_timeline_parse),
        SEMU_TEST_CASE(test_setup_walk_timeline_parse_fails_closed)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
