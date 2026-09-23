#ifndef SEMU_SAPPORO_235_GPS_AWAKE_H
#define SEMU_SAPPORO_235_GPS_AWAKE_H

#include "sapporo_235_gps_reopen.h"

#define SEMU_SAPPORO_235_GPS_AWAKE_PC UINT32_C(0x001259fe)

typedef struct {
    semu_layer_state *state;
    semu_logger *logger;
    semu_error refusal;
    const semu_sapporo_235_gps_reopen_context *reopen;
} semu_sapporo_235_gps_awake_context;

extern const semu_layer_descriptor semu_sapporo_235_gps_awake_layer;
semu_status semu_sapporo_235_gps_awake_poll(semu_sapporo_235_gps_awake_context *context,
    semu_sapporo_cxd5610 *gps, semu_bus *bus, const semu_cpu_state *cpu,
    semu_error *error);

#endif
