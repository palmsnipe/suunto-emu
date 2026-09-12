#ifndef SEMU_DEVICES_ULSAN_WDT_H
#define SEMU_DEVICES_ULSAN_WDT_H

#include "semu/bus.h"

/*
 * Ulsan 2.35.36 watchdog control write at 0x40024000 (ticket 730,
 * E-ULS-0015).
 *
 * Boot configures the upstream AmbiqApollo4_Watchdog with one 32-bit
 * write of 0x033C3D06 to +0x0 per delta-table pass (scratch access trace
 * through instruction 200,000,000: the only WDT-window access; the lane
 * log records the same value verbatim, "Unhandled bits: [1] when writing
 * value 0x33C3D06"). The lane watchdog never logged an interrupt or reset
 * afterwards, so no expiry effect is evidenced; the value is stored for
 * read-back refusal symmetry but produces no behavior. Every read and
 * every other write refuses: the lane log's 0x200 InterruptEnable read
 * and write belong to a phase the observed boot cycles do not reach, and
 * a counter read would be dynamic lane state we cannot observe.
 */

semu_status semu_ulsan_wdt_map(semu_bus *bus, semu_error *error);

#endif
