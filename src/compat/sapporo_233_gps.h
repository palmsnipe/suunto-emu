#ifndef SEMU_SAPPORO_233_GPS_H
#define SEMU_SAPPORO_233_GPS_H

#include "semu/bus.h"
#include "semu/compat.h"
#include "semu/cpu.h"
#include "../devices/sapporo_cxd5610.h"

#define SEMU_SAPPORO_233_GPS_PC UINT32_C(0x0011d990)

/* E-SAP-0071's initial status/version window only. The integration owner
 * binds this per-machine context and transport, observes sticky refusal
 * before executing another instruction, and clears refusal/hits on reset
 * after canceling transport events. Registration/snapshots are not attached. */
typedef struct {
    semu_layer_state *state;
    semu_logger *logger;
    semu_error refusal;
} semu_sapporo_233_gps_context;

extern const semu_layer_descriptor semu_sapporo_233_gps_layer;
semu_status semu_sapporo_233_gps_startup(semu_sapporo_233_gps_context *context,
    semu_sapporo_cxd5610 *gps, semu_bus *bus, const semu_cpu_state *cpu,
    semu_error *error);
semu_transaction_result semu_sapporo_233_gps_exchange(void *context,
    const uint8_t *request, size_t count, semu_sapporo_cxd5610 *gps,
    semu_error *error);

#endif
