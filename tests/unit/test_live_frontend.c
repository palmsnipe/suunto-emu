#include "test.h"

#include "../../src/frontends/live_frame_gate.c"
#include "../../src/frontends/sdl_button_hold.c"

#include <string.h>

static void test_live_gate_requires_quiet_frame(semu_test_context *context)
{
    semu_live_frame_gate gate;
    semu_input_event event = {
        SEMU_INPUT_BUTTON, SEMU_BUTTON_MIDDLE, 0, 0, 0
    };
    uint8_t pixels[2] = {0x1Fu, 0u};
    semu_frame frame = {
        SEMU_PIXEL_RGB565_LE, 240u, 240u, 480u, 1u,
        pixels, sizeof(pixels)
    };

    semu_live_frame_gate_init(&gate, SEMU_BUTTON_MIDDLE);
    event.value = 1;
    semu_live_frame_gate_note_input(&gate, 4u, &event);
    event.value = 0;
    event.code = SEMU_BUTTON_UPPER;
    semu_live_frame_gate_note_input(&gate, 4u, &event);
    SEMU_TEST_EQ_U64(context, 0u, gate.input_seen);

    event.code = SEMU_BUTTON_MIDDLE;
    semu_live_frame_gate_note_input(&gate, 4u, &event);
    SEMU_TEST_EQ_U64(context, 0u,
        semu_live_frame_gate_observe(&gate, 5u, 100u, &frame));
    SEMU_TEST_EQ_U64(context, 1u,
        semu_live_frame_gate_settle(&gate, 5u, 100u +
            SEMU_LIVE_FRAME_SETTLE_NS));
    SEMU_TEST_EQ_U64(context, 1u, gate.ready);
}

static void test_live_gate_rejects_black_and_changed_frames(
    semu_test_context *context)
{
    semu_live_frame_gate gate;
    semu_input_event event = {
        SEMU_INPUT_BUTTON, SEMU_BUTTON_LOWER, 0, 0, 0
    };
    uint8_t pixels[2] = {0u, 0u};
    semu_frame frame = {
        SEMU_PIXEL_RGB565_LE, 240u, 240u, 480u, 1u,
        pixels, sizeof(pixels)
    };

    semu_live_frame_gate_init(&gate, SEMU_BUTTON_LOWER);
    semu_live_frame_gate_note_input(&gate, 10u, &event);
    SEMU_TEST_EQ_U64(context, 0u,
        semu_live_frame_gate_observe(&gate, 11u, 100u, &frame));
    pixels[0] = 0x1Fu;
    SEMU_TEST_EQ_U64(context, 0u,
        semu_live_frame_gate_observe(&gate, 12u, 200u, &frame));
    pixels[0] = 0xE0u;
    SEMU_TEST_EQ_U64(context, 0u,
        semu_live_frame_gate_observe(&gate, 13u, 300u, &frame));
    SEMU_TEST_EQ_U64(context, 0u,
        semu_live_frame_gate_settle(&gate, 13u, 300u +
            SEMU_LIVE_FRAME_SETTLE_NS - 1u));
    SEMU_TEST_EQ_U64(context, 0u, gate.ready);
}

static void test_live_gate_consumes_next_button_edge(
    semu_test_context *context)
{
    semu_live_frame_gate gate;
    semu_input_event event = {
        SEMU_INPUT_BUTTON, SEMU_BUTTON_MIDDLE, 0, 0, 0
    };
    uint8_t pixels[2] = {0x1Fu, 0u};
    semu_frame frame = {
        SEMU_PIXEL_RGB565_LE, 240u, 240u, 480u, 1u,
        pixels, sizeof(pixels)
    };

    semu_live_frame_gate_init(&gate, SEMU_BUTTON_MIDDLE);
    semu_live_frame_gate_note_input(&gate, 1u, &event);
    semu_live_frame_gate_observe(&gate, 2u, 100u, &frame);
    SEMU_TEST_EQ_U64(context, 1u,
        semu_live_frame_gate_settle(&gate, 2u,
            100u + SEMU_LIVE_FRAME_SETTLE_NS));
    SEMU_TEST_EQ_U64(context, 1u, gate.ready);

    semu_live_frame_gate_consume(&gate);
    SEMU_TEST_EQ_U64(context, 0u, gate.ready);
    SEMU_TEST_EQ_U64(context, 1u, gate.consumed);
    event.code = SEMU_BUTTON_LOWER;
    semu_live_frame_gate_note_input(&gate, 3u, &event);
    SEMU_TEST_EQ_U64(context, 1u, gate.consumed);
    SEMU_TEST_EQ_U64(context, 1u, gate.input_seen);
}

static void test_sdl_button_hold_is_bounded_and_atomic(
    semu_test_context *context)
{
    semu_sdl_button_hold hold;
    semu_input_event press = {
        SEMU_INPUT_BUTTON, SEMU_BUTTON_MIDDLE, 0, 0, 0
    };
    semu_normalized_key down = {SEMU_INPUT_KEY_MIDDLE, 1, 0, 1};
    semu_normalized_key wrong_release = {SEMU_INPUT_KEY_UPPER, 0, 0, 2};
    semu_normalized_key release = {SEMU_INPUT_KEY_MIDDLE, 0, 0, 3};
    semu_normalized_key output[SEMU_SDL_BUTTON_COUNT];

    semu_sdl_button_hold_init(&hold);
    semu_sdl_button_hold_note_press(&hold, &press, &down);
    SEMU_TEST_EQ_U64(context, 0u,
        semu_sdl_button_hold_note_release(&hold, &wrong_release, 100u));
    SEMU_TEST_EQ_U64(context, 1u,
        semu_sdl_button_hold_note_release(&hold, &release, 100u));
    SEMU_TEST_EQ_U64(context, 1u,
        semu_sdl_button_hold_waiting(&hold, 100u));
    SEMU_TEST_EQ_U64(context, 0u,
        semu_sdl_button_hold_flush(&hold, 100u +
            SEMU_SDL_BUTTON_HOLD_NS - 1u, output, SEMU_SDL_BUTTON_COUNT));
    SEMU_TEST_EQ_U64(context, 1u,
        semu_sdl_button_hold_flush(&hold, 100u +
            SEMU_SDL_BUTTON_HOLD_NS, output, SEMU_SDL_BUTTON_COUNT));
    SEMU_TEST_EQ_U64(context, SEMU_INPUT_KEY_MIDDLE, output[0].key);
    SEMU_TEST_EQ_U64(context, 0u, output[0].down);
    SEMU_TEST_EQ_U64(context, 0u,
        semu_sdl_button_hold_flush(&hold, UINT64_MAX, output,
                                   SEMU_SDL_BUTTON_COUNT));
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_live_gate_requires_quiet_frame),
        SEMU_TEST_CASE(test_live_gate_rejects_black_and_changed_frames),
        SEMU_TEST_CASE(test_live_gate_consumes_next_button_edge),
        SEMU_TEST_CASE(test_sdl_button_hold_is_bounded_and_atomic)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
