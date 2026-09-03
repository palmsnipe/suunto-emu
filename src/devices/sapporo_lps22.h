#ifndef SEMU_SAPPORO_LPS22_H
#define SEMU_SAPPORO_LPS22_H

#include "semu/peripheral.h"
#include "../core/snapshot_io.h"

typedef struct semu_sapporo_lps22 semu_sapporo_lps22;

semu_sapporo_lps22 *semu_sapporo_lps22_create(uint8_t address,
                                               semu_error *error);
void semu_sapporo_lps22_destroy(semu_sapporo_lps22 *sensor);
void semu_sapporo_lps22_reset(void *context);
semu_serial_endpoint semu_sapporo_lps22_endpoint(
    semu_sapporo_lps22 *sensor);
semu_status semu_sapporo_lps22_snapshot_write(
    const semu_sapporo_lps22 *sensor, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_sapporo_lps22_snapshot_read(
    semu_sapporo_lps22 *sensor, semu_snapshot_reader *reader,
    semu_error *error);

#endif
