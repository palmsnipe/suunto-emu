#ifndef SEMU_DEVICES_ULSAN_PWRCTRL_H
#define SEMU_DEVICES_ULSAN_PWRCTRL_H

#include "semu/bus.h"

/*
 * Ulsan 2.35.36 power-control block (ticket 730, E-ULS-0008).
 *
 * Evidence-bounded attachment of the 0x40021000 block for the Ulsan board.
 * The reference lane resolves this block with
 * Miscellaneous.AmbiqApollo4_PowerController at 0x40021000; every boot-phase
 * write the lane logged was reported entirely as unhandled bits (a state
 * no-op), and the one read consumed during boot is the DEVICE_POWER_STATUS
 * bit-20 sample. Every other offset refuses. Stateless by design, so one
 * mapping is safe for every machine instance on its own bus.
 */

semu_status semu_ulsan_pwrctrl_map(semu_bus *bus, semu_error *error);

#endif
