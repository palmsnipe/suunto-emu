#ifndef SEMU_DEVICES_ULSAN_WDT_H
#define SEMU_DEVICES_ULSAN_WDT_H

#include "semu/bus.h"

/*
 * Ulsan 2.35.36 watchdog control write at 0x40024000 (ticket 730,
 * E-ULS-0015).
 *
 * Boot configures the upstream AmbiqApollo4_Watchdog per delta-table
 * pass (scratch access trace through instruction 200,000,000): the 32-bit
 * write of 0x033C3D06 to +0x0 (lane log records it verbatim, "Unhandled
 * bits: [1] when writing value 0x33C3D06"), then a control read and a
 * (read | 1) store (E-ULS-0017), the 0xB2 reload store to +0x4, and the
 * lane-logged InterruptEnable read/write pair at +0x200. Control and
 * InterruptEnable read back their stores. Restart is write-only in this
 * model. Expiry and watchdog interrupts are outside the startup law.
 * E-ULS-0060 records the same sequence on 2.44.52.
 */

typedef struct semu_ulsan_wdt {
    uint32_t interrupt_enable;
    uint32_t reload;
    uint32_t control;
} semu_ulsan_wdt;

semu_status semu_ulsan_wdt_map_state(semu_bus *bus, semu_ulsan_wdt *state,
                                     semu_error *error);
semu_status semu_ulsan_wdt_map(semu_bus *bus, semu_error *error);

#endif
