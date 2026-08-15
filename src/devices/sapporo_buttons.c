/*
 * Sapporo 2.22 physical buttons (ticket 404).
 * Evidence: E-SAP-BUTTONS-001 (verified).
 * GPIO 57/58/59, active-low.  Translates semantic press/release to
 * electrical GPIO levels via the constructor-provided callback.
 */

#include "sapporo_buttons.h"

#include <stdlib.h>
#include <string.h>

#define BUTTON_COUNT 3u

struct semu_sapporo_buttons {
    unsigned pins[BUTTON_COUNT];
    int active_low;
    int pressed[BUTTON_COUNT];
    semu_sapporo_button_gpio_fn gpio_cb;
    void *gpio_ctx;
};

static unsigned id_to_index(semu_button_id id)
{
    return (unsigned)id;
}

static int pressed_level(const semu_sapporo_buttons *buttons)
{
    return buttons->active_low ? 0 : 1;
}

static int released_level(const semu_sapporo_buttons *buttons)
{
    return buttons->active_low ? 1 : 0;
}

static void emit_gpio(const semu_sapporo_buttons *buttons, unsigned index,
                      int level)
{
    if (buttons->gpio_cb != NULL) {
        buttons->gpio_cb(buttons->gpio_ctx, buttons->pins[index], level);
    }
}

semu_sapporo_buttons *semu_sapporo_buttons_create(
    unsigned upper_pin, unsigned middle_pin, unsigned lower_pin,
    int active_low,
    semu_sapporo_button_gpio_fn gpio_callback, void *gpio_context,
    semu_error *error)
{
    semu_sapporo_buttons *buttons;
    (void)error;
    buttons = (semu_sapporo_buttons *)calloc(1u, sizeof(*buttons));
    if (buttons == NULL) {
        return NULL;
    }
    buttons->pins[SEMU_BUTTON_UPPER] = upper_pin;
    buttons->pins[SEMU_BUTTON_MIDDLE] = middle_pin;
    buttons->pins[SEMU_BUTTON_LOWER] = lower_pin;
    buttons->active_low = active_low;
    buttons->gpio_cb = gpio_callback;
    buttons->gpio_ctx = gpio_context;
    return buttons;
}

void semu_sapporo_buttons_destroy(semu_sapporo_buttons *buttons)
{
    free(buttons);
}

void semu_sapporo_buttons_reset(semu_sapporo_buttons *buttons)
{
    unsigned i;
    if (buttons == NULL) {
        return;
    }
    for (i = 0u; i < BUTTON_COUNT; ++i) {
        if (buttons->pressed[i]) {
            buttons->pressed[i] = 0;
            emit_gpio(buttons, i, released_level(buttons));
        }
    }
}

semu_status semu_sapporo_buttons_press(semu_sapporo_buttons *buttons,
    semu_button_id id, semu_error *error)
{
    unsigned idx;
    if (buttons == NULL || id >= BUTTON_COUNT) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid button");
        return SEMU_ERR_ARGUMENT;
    }
    idx = id_to_index(id);
    if (buttons->pressed[idx]) {
        semu_error_set(error, SEMU_ERR_STATE, "button %u already pressed", idx);
        return SEMU_ERR_STATE;
    }
    buttons->pressed[idx] = 1;
    emit_gpio(buttons, idx, pressed_level(buttons));
    return SEMU_OK;
}

semu_status semu_sapporo_buttons_release(semu_sapporo_buttons *buttons,
    semu_button_id id, semu_error *error)
{
    unsigned idx;
    if (buttons == NULL || id >= BUTTON_COUNT) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid button");
        return SEMU_ERR_ARGUMENT;
    }
    idx = id_to_index(id);
    if (!buttons->pressed[idx]) {
        semu_error_set(error, SEMU_ERR_STATE, "button %u not pressed", idx);
        return SEMU_ERR_STATE;
    }
    buttons->pressed[idx] = 0;
    emit_gpio(buttons, idx, released_level(buttons));
    return SEMU_OK;
}

int semu_sapporo_buttons_is_pressed(const semu_sapporo_buttons *buttons,
    semu_button_id id)
{
    if (buttons == NULL || id >= BUTTON_COUNT) {
        return 0;
    }
    return buttons->pressed[id_to_index(id)];
}
