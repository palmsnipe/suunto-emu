#ifndef SEMU_SAPPORO_GPS_COMPAT_H
#define SEMU_SAPPORO_GPS_COMPAT_H

#include "semu/bus.h"
#include "semu/compat.h"
#include "semu/cpu.h"
#include "../compat/sapporo_222.h"
#include "../soc/apollo4/uart.h"

/* Apply the bounded, version-pinned later GPS exchange used by the OTA UI
 * session.  Non-trigger PCs are no-ops; an observed trigger with an invalid
 * register shape or unavailable UART refuses. */
semu_status semu_sapporo_gps_compat_apply(
    semu_apollo4_uart *uart, semu_bus *bus, semu_cpu_state *cpu_state,
    semu_sapporo_cxd5610 *gps,
    semu_sapporo_222_fixture_context *fixture_context, semu_error *error);

#endif
