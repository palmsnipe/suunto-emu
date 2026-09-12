#ifndef SEMU_DEVICES_ULSAN_TIMER0_H
#define SEMU_DEVICES_ULSAN_TIMER0_H

#include "semu/bus.h"

/*
 * Ulsan 2.35.36 TIMER block at 0x40008000 (ticket 730, E-ULS-0021): the
 * seven registers boot touches, with the lane-silent framework store
 * semantics (writes store, reads answer the store for the three read
 * registers, reset to 0). All other TIMER addresses, widths, and reads
 * of write-only-observed registers refuse; see src/devices/
 * ulsan_timer0.c for the observed transaction sequence.
 */

semu_status semu_ulsan_timer0_map(semu_bus *bus, semu_error *error);

#endif
