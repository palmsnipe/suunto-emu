#ifndef SEMU_DEVICES_ULSAN_IOM4_H
#define SEMU_DEVICES_ULSAN_IOM4_H

#include "semu/bus.h"

/* Map the Ulsan 2.35.36 IOM4 block (0x40054000, 4 KiB window). See the
 * source for the boot-observed register set (ticket 730, E-ULS-0023,
 * E-ULS-0025). */
semu_status semu_ulsan_iom4_map(semu_bus *bus, semu_error *error);

#endif /* SEMU_DEVICES_ULSAN_IOM4_H */
