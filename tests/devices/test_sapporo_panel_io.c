#include "test.h"

#include <string.h>

#include "sapporo_backlight.h"
#include "sapporo_buttons.h"
#include "sapporo_panel_transport.h"
#include "semu/input.h"
#include "semu/peripheral.h"

typedef struct gpio_trace {
    unsigned pin;
    int level;
} gpio_trace;

static gpio_trace trace_buf[16];
static size_t trace_count;

static void gpio_cb(void *context, unsigned pin, int level)
{
    (void)context;
    if (trace_count < sizeof(trace_buf) / sizeof(trace_buf[0])) {
        trace_buf[trace_count].pin = pin;
        trace_buf[trace_count].level = level;
        ++trace_count;
    }
}

static void test_button_press_release(semu_test_context *context)
{
    semu_sapporo_buttons *buttons;
    semu_error error;
    semu_error_clear(&error);
    trace_count = 0;
    buttons = semu_sapporo_buttons_create(57u, 58u, 59u, 1, gpio_cb, NULL,
                                          &error);
    SEMU_TEST_ASSERT(context, buttons != NULL);

    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_buttons_press(buttons, SEMU_BUTTON_UPPER, &error));
    SEMU_TEST_EQ_U64(context, 57u, trace_buf[0].pin);
    SEMU_TEST_EQ_U64(context, 0, trace_buf[0].level);
    SEMU_TEST_ASSERT(context, semu_sapporo_buttons_is_pressed(buttons,
        SEMU_BUTTON_UPPER));

    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_buttons_release(buttons, SEMU_BUTTON_UPPER, &error));
    SEMU_TEST_EQ_U64(context, 57u, trace_buf[1].pin);
    SEMU_TEST_EQ_U64(context, 1, trace_buf[1].level);

    semu_sapporo_buttons_destroy(buttons);
}

static void test_button_all_three(semu_test_context *context)
{
    semu_sapporo_buttons *buttons;
    semu_error error;
    semu_error_clear(&error);
    trace_count = 0;
    buttons = semu_sapporo_buttons_create(57u, 58u, 59u, 1, gpio_cb, NULL,
                                          &error);
    SEMU_TEST_ASSERT(context, buttons != NULL);

    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_buttons_press(buttons, SEMU_BUTTON_UPPER, &error));
    SEMU_TEST_EQ_U64(context, 0, trace_buf[0].level);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_buttons_press(buttons, SEMU_BUTTON_MIDDLE, &error));
    SEMU_TEST_EQ_U64(context, 0, trace_buf[1].level);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_buttons_press(buttons, SEMU_BUTTON_LOWER, &error));
    SEMU_TEST_EQ_U64(context, 0, trace_buf[2].level);

    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_buttons_release(buttons, SEMU_BUTTON_MIDDLE, &error));
    SEMU_TEST_EQ_U64(context, 1, trace_buf[3].level);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_buttons_release(buttons, SEMU_BUTTON_LOWER, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_buttons_release(buttons, SEMU_BUTTON_UPPER, &error));

    semu_sapporo_buttons_destroy(buttons);
}

static void test_button_double_press_refuses(semu_test_context *context)
{
    semu_sapporo_buttons *buttons;
    semu_error error;
    semu_error_clear(&error);
    trace_count = 0;
    buttons = semu_sapporo_buttons_create(57u, 58u, 59u, 1, gpio_cb, NULL,
                                          &error);
    SEMU_TEST_ASSERT(context, buttons != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_buttons_press(buttons, SEMU_BUTTON_MIDDLE, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_sapporo_buttons_press(buttons, SEMU_BUTTON_MIDDLE, &error));
    semu_sapporo_buttons_destroy(buttons);
}

static void test_button_reset_releases(semu_test_context *context)
{
    semu_sapporo_buttons *buttons;
    semu_error error;
    semu_error_clear(&error);
    trace_count = 0;
    buttons = semu_sapporo_buttons_create(57u, 58u, 59u, 1, gpio_cb, NULL,
                                          &error);
    SEMU_TEST_ASSERT(context, buttons != NULL);
    semu_sapporo_buttons_press(buttons, SEMU_BUTTON_UPPER, &error);
    semu_sapporo_buttons_press(buttons, SEMU_BUTTON_LOWER, &error);
    trace_count = 0;
    semu_sapporo_buttons_reset(buttons);
    SEMU_TEST_ASSERT(context, !semu_sapporo_buttons_is_pressed(buttons,
        SEMU_BUTTON_UPPER));
    SEMU_TEST_ASSERT(context, !semu_sapporo_buttons_is_pressed(buttons,
        SEMU_BUTTON_LOWER));
    SEMU_TEST_EQ_U64(context, 2u, trace_count);
    semu_sapporo_buttons_destroy(buttons);
}

static void test_backlight_off_then_on(semu_test_context *context)
{
    semu_sapporo_backlight *bl;
    semu_error error;
    semu_error_clear(&error);
    bl = semu_sapporo_backlight_create(&error);
    SEMU_TEST_ASSERT(context, bl != NULL);
    SEMU_TEST_ASSERT(context, !semu_sapporo_backlight_is_on(bl));
    semu_sapporo_backlight_on_timer_write(bl, 0x320u, 0x12u);
    semu_sapporo_backlight_on_timer_write(bl, 0x328u, 0xA44u);
    SEMU_TEST_ASSERT(context, semu_sapporo_backlight_is_on(bl));
    SEMU_TEST_EQ_U64(context, 0xA44u, semu_sapporo_backlight_level(bl));
    semu_sapporo_backlight_destroy(bl);
}

static void test_backlight_reset(semu_test_context *context)
{
    semu_sapporo_backlight *bl;
    semu_error error;
    semu_error_clear(&error);
    bl = semu_sapporo_backlight_create(&error);
    semu_sapporo_backlight_on_timer_write(bl, 0x320u, 0x12u);
    semu_sapporo_backlight_on_timer_write(bl, 0x328u, 0x100u);
    SEMU_TEST_ASSERT(context, semu_sapporo_backlight_is_on(bl));
    semu_sapporo_backlight_reset(bl);
    SEMU_TEST_ASSERT(context, !semu_sapporo_backlight_is_on(bl));
    semu_sapporo_backlight_destroy(bl);
}

static void test_panel_refuses(semu_test_context *context)
{
    semu_sapporo_panel_transport *panel;
    semu_serial_endpoint ep;
    semu_serial_transaction txn;
    semu_error error;
    semu_transaction_result result;
    semu_error_clear(&error);
    panel = semu_sapporo_panel_transport_create(&error);
    SEMU_TEST_ASSERT(context, panel != NULL);
    ep = semu_sapporo_panel_transport_endpoint(panel);
    memset(&txn, 0, sizeof(txn));
    result = ep.transfer(ep.context, &txn, &error);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, result);
    semu_sapporo_panel_transport_destroy(panel);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_button_press_release),
        SEMU_TEST_CASE(test_button_all_three),
        SEMU_TEST_CASE(test_button_double_press_refuses),
        SEMU_TEST_CASE(test_button_reset_releases),
        SEMU_TEST_CASE(test_backlight_off_then_on),
        SEMU_TEST_CASE(test_backlight_reset),
        SEMU_TEST_CASE(test_panel_refuses)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
