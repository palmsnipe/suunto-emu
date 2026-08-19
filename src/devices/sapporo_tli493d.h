#ifndef SEMU_SAPPORO_TLI493D_H
#define SEMU_SAPPORO_TLI493D_H

#include "semu/peripheral.h"
#include "../core/snapshot_io.h"

typedef struct semu_sapporo_tli493d semu_sapporo_tli493d;

semu_sapporo_tli493d *semu_sapporo_tli493d_create(uint8_t address,
                                                   semu_error *error);
void semu_sapporo_tli493d_destroy(semu_sapporo_tli493d *sensor);
void semu_sapporo_tli493d_reset(void *context);
semu_serial_endpoint semu_sapporo_tli493d_endpoint(
    semu_sapporo_tli493d *sensor);
semu_status semu_sapporo_tli493d_snapshot_write(
    const semu_sapporo_tli493d *sensor, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_sapporo_tli493d_snapshot_read(
    semu_sapporo_tli493d *sensor, semu_snapshot_reader *reader,
    semu_error *error);

#endif
