#ifndef SEMU_DEVICES_ULSAN_DAXI_H
#define SEMU_DEVICES_ULSAN_DAXI_H

#include "semu/bus.h"

/*
 * Ulsan 2.35.36 cpu-complex DAXI block at 0x48000000 (ticket 730,
 * E-ULS-0011).
 *
 * The reference lane models this block with the one-line script published
 * in the upstream platform description (ambiq-apollo4.repl lines
 * 197-201, size 0x1000): reads return 0x4 at offset 0x54 ("DAXI Control =
 * 0x4 - DAXIREADY bit set" per its comment) and 0 everywhere else; the
 * script stores no write, so every write is accepted and dropped. Boot's
 * observed sequence (scratch access trace, lane probe 10): read 0x50 -> 0
 * (bit 2 test at 0x0009750c selects the continue path), read 0x54, store
 * 0x54. This device reproduces the script exactly.
 */

semu_status semu_ulsan_daxi_map(semu_bus *bus, semu_error *error);

#endif
