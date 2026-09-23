#ifndef SEMU_SAPPORO_235_PRODUCTION_H
#define SEMU_SAPPORO_235_PRODUCTION_H

#include "semu/bus.h"
#include "semu/compat.h"

/* E-SAP-0038: one synthetic manufacturing sector, once per reset. */
extern const semu_layer_descriptor semu_sapporo_235_production_layer;
semu_status semu_sapporo_235_install_production(semu_bus *bus,
    semu_layer_state *state, semu_logger *logger, semu_error *error);

#endif
