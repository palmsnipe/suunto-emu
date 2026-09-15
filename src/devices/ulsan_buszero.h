#ifndef SEMU_DEVICES_ULSAN_BUSZERO_H
#define SEMU_DEVICES_ULSAN_BUSZERO_H

#include "semu/bus.h"

/*
 * Ulsan 2.35.36 lane-recorded bus-zero accesses (ticket 730,
 * E-ULS-0013/0016/0038).
 *
 * The reference-lane sysbus answers accesses below the peripheral
 * registry with its non-existing-peripheral fallback: reads log
 * "Read<T> from non existing peripheral at 0x..." and return the
 * untagged default 0; writes log "Write<T> ... value 0x..." and are
 * discarded. Each block window of this device lists exactly the
 * (offset, width[, write value]) tuples with reproduced lane log
 * lines - per-tuple, never whole-range zeros - answers tabled reads
 * with 0, accepts only the tabled write values (discarding them like
 * the lane), and refuses everything else, including reads of
 * write-only tuples. Byte and halfword tuples match the engine's own
 * Byte/Word access report.
 */

semu_status semu_ulsan_buszero_map(semu_bus *bus, semu_error *error);

#endif
