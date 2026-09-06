#ifndef SEMU_SAPPORO_239_GPS_REOPEN_H
#define SEMU_SAPPORO_239_GPS_REOPEN_H

#include "sapporo_239_gps.h"

#define SEMU_SAPPORO_239_GPS_REOPEN_PC UINT32_C(0x00128e8c)

typedef struct semu_sapporo_239_gps_reopen_context {
    semu_layer_state *state;
    semu_sapporo_239_gps_context *startup;
    semu_logger *logger;
    semu_layer_descriptor descriptor;
    semu_layer_intervention interventions[2];
} semu_sapporo_239_gps_reopen_context;

extern const semu_layer_descriptor semu_sapporo_239_gps_reopen_layer;
int semu_sapporo_239_gps_reopen_is_layer(const semu_layer_descriptor *d);
semu_status semu_sapporo_239_gps_reopen_bind(
    semu_sapporo_239_gps_reopen_context *c, semu_layer_state *state,
    semu_sapporo_239_gps_context *startup, semu_logger *logger, semu_error *error);
semu_status semu_sapporo_239_gps_reopen_start(
    semu_sapporo_239_gps_reopen_context *c, semu_sapporo_cxd5610 *gps,
    semu_bus *bus, const semu_cpu_state *cpu, semu_error *error);
semu_transaction_result semu_sapporo_239_gps_reopen_exchange(void *context,
    const uint8_t *request, size_t count, semu_sapporo_cxd5610 *gps,
    semu_error *error);

#endif
