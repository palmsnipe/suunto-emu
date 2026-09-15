#ifndef SEMU_DEVICES_ULSAN_RTC_H
#define SEMU_DEVICES_ULSAN_RTC_H

#include "semu/bus.h"
#include "semu/scheduler.h"

/*
 * Ulsan 2.35.36 RTC block at 0x40004800 (ticket 730, E-ULS-0019,
 * E-ULS-0036): the four registers boot touches with lane-silent
 * framework store semantics (writes store, reads answer the store,
 * reset to 0), plus the sleep-path counter window: +0x20 answers BCD
 * hundredths of scheduler virtual time, +0x24 answers its store
 * (reset 0x14700101). All other RTC addresses and widths refuse, and
 * the counter words refuse writes; see src/devices/ulsan_rtc.c for
 * the observed transaction sequence and lane calibration.
 */

semu_status semu_ulsan_rtc_map(semu_bus *bus, semu_error *error);

/* Counter-window source (ticket 730, E-ULS-0036); device reset
 * preserves it, device maps reset detach. */
void semu_ulsan_rtc_attach(semu_scheduler *scheduler);

#endif
