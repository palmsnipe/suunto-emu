#ifndef SEMU_SAPPORO_HAPTIC_H
#define SEMU_SAPPORO_HAPTIC_H

#include "semu/peripheral.h"

typedef struct semu_sapporo_haptic semu_sapporo_haptic;

semu_sapporo_haptic *semu_sapporo_haptic_create(uint8_t address,
                                                 semu_error *error);
void semu_sapporo_haptic_destroy(semu_sapporo_haptic *sensor);
void semu_sapporo_haptic_reset(void *context);
semu_serial_endpoint semu_sapporo_haptic_endpoint(
    semu_sapporo_haptic *sensor);

#endif
