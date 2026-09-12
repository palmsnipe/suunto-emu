#ifndef SEMU_DEVICES_ULSAN_BUSZERO_H
#define SEMU_DEVICES_ULSAN_BUSZERO_H

#include "semu/bus.h"

/*
 * Ulsan 2.35.36 lane-recorded bus-zero reads (ticket 730, E-ULS-0013).
 *
 * The reference-lane sysbus log records individual reads of unmodelled,
 * tag-only ranges as "ReadDoubleWord from non existing peripheral at
 * 0x..., returning 0x00000000" - the byte truth for exactly those
 * offsets. Each entry of this device's table is one such logged line
 * (block base, offset, logged value); the device maps the enclosing
 * declared tag ranges, answers only the tabled offsets, and refuses
 * everything else, including all writes (no lane log line records a
 * write to these ranges).
 */

semu_status semu_ulsan_buszero_map(semu_bus *bus, semu_error *error);

#endif
