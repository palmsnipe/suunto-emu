#ifndef SEMU_DEVICES_ULSAN_PWRCTRL_H
#define SEMU_DEVICES_ULSAN_PWRCTRL_H

#include "semu/bus.h"

typedef struct semu_ulsan_pwrctrl {
    uint32_t device_power_enable;
    uint32_t display_mask;
} semu_ulsan_pwrctrl;

/* Existing 2.35.36 attachment (E-ULS-0008/0037). */
semu_status semu_ulsan_pwrctrl_map(semu_bus *bus, semu_error *error);

/* The 2.44.52 caller owns state for the lifetime of the bus (E-ULS-0053). */
semu_status semu_ulsan_244_pwrctrl_map(semu_bus *bus,
                                      semu_ulsan_pwrctrl *state,
                                      semu_error *error);

#endif
