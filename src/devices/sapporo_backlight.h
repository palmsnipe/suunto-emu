#ifndef SEMU_SAPPORO_BACKLIGHT_H
#define SEMU_SAPPORO_BACKLIGHT_H

#include <stdint.h>

#include "semu/types.h"

/*
 * Sapporo 2.22 backlight (ticket 404).
 * Evidence: E-SAP-BACKLIGHT-001 (verified).
 * CTIMER9: OUTCFG104=0x12, control=0xA44, compare at 0x328/0x32c.
 * Observes timer configuration and reports on/off/level state.
 */

typedef struct semu_sapporo_backlight semu_sapporo_backlight;

semu_sapporo_backlight *semu_sapporo_backlight_create(semu_error *error);
void semu_sapporo_backlight_destroy(semu_sapporo_backlight *backlight);
void semu_sapporo_backlight_reset(semu_sapporo_backlight *backlight);

/*
 * Called when the timer module writes to a backlight-relevant register.
 * The offset is relative to the CTIMER base.  Recognized offsets:
 *   0x320: OUTCFG (output configuration)
 *   0x328: compare A
 *   0x32c: compare B
 *   0x330: compare C
 */
void semu_sapporo_backlight_on_timer_write(
    semu_sapporo_backlight *backlight, uint32_t offset, uint32_t value);

int semu_sapporo_backlight_is_on(const semu_sapporo_backlight *backlight);
uint32_t semu_sapporo_backlight_level(
    const semu_sapporo_backlight *backlight);

#endif
