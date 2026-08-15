#ifndef SEMU_SAPPORO_BUTTONS_H
#define SEMU_SAPPORO_BUTTONS_H

#include "semu/input.h"
#include "semu/types.h"

/*
 * Sapporo 2.22 physical buttons (ticket 404).
 * Evidence: E-SAP-BUTTONS-001 (verified).
 * GPIO 57=upper, 58=middle, 59=lower.  Active-low: electrical 0 = pressed.
 * Board pins are constructor data; the module owns no GPIO hardware.
 */

typedef struct semu_sapporo_buttons semu_sapporo_buttons;

typedef void (*semu_sapporo_button_gpio_fn)(void *context, unsigned pin,
                                             int level);

semu_sapporo_buttons *semu_sapporo_buttons_create(
    unsigned upper_pin, unsigned middle_pin, unsigned lower_pin,
    int active_low,
    semu_sapporo_button_gpio_fn gpio_callback, void *gpio_context,
    semu_error *error);
void semu_sapporo_buttons_destroy(semu_sapporo_buttons *buttons);
void semu_sapporo_buttons_reset(semu_sapporo_buttons *buttons);

semu_status semu_sapporo_buttons_press(semu_sapporo_buttons *buttons,
    semu_button_id id, semu_error *error);
semu_status semu_sapporo_buttons_release(semu_sapporo_buttons *buttons,
    semu_button_id id, semu_error *error);
int semu_sapporo_buttons_is_pressed(const semu_sapporo_buttons *buttons,
    semu_button_id id);

#endif
