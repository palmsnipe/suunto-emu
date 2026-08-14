#ifndef SEMU_SAPPORO_222_COMPAT_H
#define SEMU_SAPPORO_222_COMPAT_H

#include "semu/bus.h"
#include "semu/compat.h"

extern const semu_layer_descriptor semu_sapporo_222_no_device_layer;

semu_status semu_sapporo_222_install_no_device(
    semu_bus *bus, semu_layer_state *state, semu_logger *logger,
    semu_error *error);

#endif
