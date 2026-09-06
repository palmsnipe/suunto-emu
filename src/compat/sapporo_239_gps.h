#ifndef SEMU_SAPPORO_239_GPS_H
#define SEMU_SAPPORO_239_GPS_H

#include "semu/bus.h"
#include "semu/compat.h"
#include "semu/cpu.h"
#include "../devices/sapporo_cxd5610.h"

#define SEMU_SAPPORO_239_GPS_PC UINT32_C(0x00128d14)

/* Counters are owned by the device instance, serialized by the existing
 * machine layer codec. Borrowed binding pointers carry no lifecycle state. */
typedef struct semu_sapporo_239_gps_context {
    semu_layer_state *state;
    semu_logger *logger;
    semu_layer_descriptor descriptor;
    semu_layer_intervention interventions[2];
} semu_sapporo_239_gps_context;

extern const semu_layer_descriptor semu_sapporo_239_gps_layer;
int semu_sapporo_239_gps_is_layer(const semu_layer_descriptor *descriptor);
int semu_sapporo_239_gps_counts_valid(uint64_t total, uint64_t startup,
                                      uint64_t reply);
semu_status semu_sapporo_239_gps_bind(semu_sapporo_239_gps_context *context,
    semu_layer_state *state, semu_logger *logger, semu_error *error);
semu_status semu_sapporo_239_gps_startup(semu_sapporo_239_gps_context *context,
    semu_sapporo_cxd5610 *gps, semu_bus *bus, const semu_cpu_state *cpu,
    semu_error *error);
semu_transaction_result semu_sapporo_239_gps_exchange(void *context,
    const uint8_t *request, size_t count, semu_sapporo_cxd5610 *gps,
    semu_error *error);

#endif
