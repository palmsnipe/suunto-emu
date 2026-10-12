#ifndef SEMU_SAPPORO_233_PRODUCTION_H
#define SEMU_SAPPORO_233_PRODUCTION_H

#include "semu/bus.h"
#include "semu/compat.h"

/* The E-SAP-0038 recovered synthetic manufacturing sector for the 2.33.16
 * profile: the same ProductionData/ACCR/ACCC/MAGN/HLAT container the 2.35
 * layer installs, generated from the 2.33 application's own checksum table
 * (VA 0x1abb14, the unique copy of the pinned 16-word table). Installed
 * once per machine reset into an empty flash sector at 0x14fff000.
 * E-SAP-0057 names the missing production data as the 2.33 boot-stage
 * candidate; the staged lane boots with exactly these bytes. */
extern const semu_layer_descriptor semu_sapporo_233_production_layer;

semu_status semu_sapporo_233_install_production(semu_bus *bus,
    semu_layer_state *state, semu_logger *logger, semu_error *error);

#endif
