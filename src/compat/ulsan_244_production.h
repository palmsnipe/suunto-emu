#ifndef SEMU_ULSAN_244_PRODUCTION_H
#define SEMU_ULSAN_244_PRODUCTION_H

#include "semu/bus.h"
#include "semu/compat.h"

extern const semu_layer_descriptor semu_ulsan_244_production_layer;
extern const semu_layer_descriptor semu_ulsan_244_emmc_layer;
semu_status semu_ulsan_244_install_production(semu_bus *bus,
    semu_layer_state *state, semu_logger *logger, semu_error *error);

#endif
