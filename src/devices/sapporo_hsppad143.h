#ifndef SEMU_SAPPORO_HSPPAD143_H
#define SEMU_SAPPORO_HSPPAD143_H

#include "semu/peripheral.h"
#include "../core/snapshot_io.h"

typedef struct semu_sapporo_hsppad143 semu_sapporo_hsppad143;

semu_sapporo_hsppad143 *semu_sapporo_hsppad143_create(uint8_t address,
                                                      semu_error *error);
void semu_sapporo_hsppad143_destroy(semu_sapporo_hsppad143 *sensor);
void semu_sapporo_hsppad143_reset(void *context);
semu_serial_endpoint semu_sapporo_hsppad143_endpoint(
    semu_sapporo_hsppad143 *sensor);
semu_status semu_sapporo_hsppad143_snapshot_write(
    const semu_sapporo_hsppad143 *sensor, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_sapporo_hsppad143_snapshot_read(
    semu_sapporo_hsppad143 *sensor, semu_snapshot_reader *reader,
    semu_error *error);

#endif
