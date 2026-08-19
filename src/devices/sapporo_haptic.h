#ifndef SEMU_SAPPORO_HAPTIC_H
#define SEMU_SAPPORO_HAPTIC_H

#include "semu/peripheral.h"
#include "../core/snapshot_io.h"

typedef struct semu_sapporo_haptic semu_sapporo_haptic;

semu_sapporo_haptic *semu_sapporo_haptic_create(uint8_t address,
                                                 semu_error *error);
void semu_sapporo_haptic_destroy(semu_sapporo_haptic *sensor);
void semu_sapporo_haptic_reset(void *context);
semu_serial_endpoint semu_sapporo_haptic_endpoint(
    semu_sapporo_haptic *sensor);
semu_status semu_sapporo_haptic_snapshot_write(
    const semu_sapporo_haptic *sensor, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_sapporo_haptic_snapshot_read(
    semu_sapporo_haptic *sensor, semu_snapshot_reader *reader,
    semu_error *error);

#endif
