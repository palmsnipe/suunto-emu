#ifndef SEMU_DEVICES_ULSAN_CLKGEN_H
#define SEMU_DEVICES_ULSAN_CLKGEN_H

#include "semu/bus.h"

/*
 * Ulsan 2.35.36 clock generator at 0x40004000 (ticket 730, E-ULS-0010).
 *
 * The reference lane models this block as a sparse-offset dictionary
 * peripheral (Python.PythonPeripheral in ulsan-platform.repl: reads
 * return the stored value for an offset or 0, writes store the 32-bit
 * value under the offset), size 0x800. Boot's observed sequence on
 * offset 0x44 is read 0, store 0x00FC0000, read back, store
 * 0x00FC0040; the platform comment additionally records +0x84
 * read/write traffic during display-clock bring-up. This device
 * reproduces the lane script exactly over a bounded slot table.
 */

semu_status semu_ulsan_clkgen_map(semu_bus *bus, semu_error *error);

#define SEMU_ULSAN_CLKGEN_SLOTS 64u

typedef struct semu_ulsan_clkgen {
    uint32_t stored[SEMU_ULSAN_CLKGEN_SLOTS];
    uint32_t value[SEMU_ULSAN_CLKGEN_SLOTS];
    unsigned used;
} semu_ulsan_clkgen;

/* The caller owns state for the lifetime of the bus. */
semu_status semu_ulsan_clkgen_map_state(semu_bus *bus,
                                       semu_ulsan_clkgen *state,
                                       semu_error *error);

#endif
