#ifndef SEMU_DEVICES_ULSAN_RTC_H
#define SEMU_DEVICES_ULSAN_RTC_H

#include "semu/bus.h"

/*
 * Ulsan 2.35.36 RTC block at 0x40004800 (ticket 730, E-ULS-0019): the
 * four registers boot touches, with the lane-silent framework store
 * semantics (writes store, reads answer the store, reset to 0). All
 * other RTC addresses, widths, and writes-to-unobserved registers
 * refuse; see src/devices/ulsan_rtc.c for the observed transaction
 * sequence.
 */

semu_status semu_ulsan_rtc_map(semu_bus *bus, semu_error *error);

#endif
