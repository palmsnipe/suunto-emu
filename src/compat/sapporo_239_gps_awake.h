#ifndef SEMU_SAPPORO_239_GPS_AWAKE_H
#define SEMU_SAPPORO_239_GPS_AWAKE_H

#include "sapporo_239_gps_reopen.h"

#define SEMU_SAPPORO_239_GPS_AWAKE_PC UINT32_C(0x001291cc)

typedef struct semu_sapporo_239_gps_awake_context {
    semu_layer_state *state;
    const semu_layer_state *startup, *reopen;
    semu_logger *logger;
    semu_layer_descriptor descriptor;
    semu_layer_intervention intervention;
} semu_sapporo_239_gps_awake_context;

extern const semu_layer_descriptor semu_sapporo_239_gps_awake_layer;
int semu_sapporo_239_gps_awake_is_layer(const semu_layer_descriptor *d);
/* Preflight only; no counter, descriptor, callback or device mutation. */
semu_status semu_sapporo_239_gps_awake_validate(const semu_layer_state *state,
    const semu_layer_state *startup, const semu_layer_state *reopen,
    semu_error *error);
semu_status semu_sapporo_239_gps_awake_bind(
    semu_sapporo_239_gps_awake_context *c, semu_layer_state *state,
    const semu_layer_state *startup, const semu_layer_state *reopen,
    semu_logger *logger, semu_error *error);
semu_status semu_sapporo_239_gps_awake_poll(
    semu_sapporo_239_gps_awake_context *c, semu_sapporo_cxd5610 *gps,
    semu_bus *bus, const semu_cpu_state *cpu, semu_error *error);

#endif
