#ifndef SEMU_SAPPORO_LSM6DSL_H
#define SEMU_SAPPORO_LSM6DSL_H

#include "semu/peripheral.h"
#include "../core/snapshot_io.h"

typedef struct semu_sapporo_lsm6dsl semu_sapporo_lsm6dsl;

semu_sapporo_lsm6dsl *semu_sapporo_lsm6dsl_create(uint8_t chip_select,
                                                   semu_error *error);
void semu_sapporo_lsm6dsl_destroy(semu_sapporo_lsm6dsl *sensor);
void semu_sapporo_lsm6dsl_reset(void *context);
semu_serial_endpoint semu_sapporo_lsm6dsl_endpoint(
    semu_sapporo_lsm6dsl *sensor);
semu_status semu_sapporo_lsm6dsl_snapshot_write(
    const semu_sapporo_lsm6dsl *sensor, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_sapporo_lsm6dsl_snapshot_read(
    semu_sapporo_lsm6dsl *sensor, semu_snapshot_reader *reader,
    semu_error *error);

#endif
