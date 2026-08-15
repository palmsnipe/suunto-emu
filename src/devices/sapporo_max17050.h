#ifndef SEMU_SAPPORO_MAX17050_H
#define SEMU_SAPPORO_MAX17050_H

#include "semu/peripheral.h"

typedef struct semu_sapporo_max17050 semu_sapporo_max17050;

semu_sapporo_max17050 *semu_sapporo_max17050_create(uint8_t address,
                                                     semu_error *error);
void semu_sapporo_max17050_destroy(semu_sapporo_max17050 *sensor);
void semu_sapporo_max17050_reset(void *context);
semu_serial_endpoint semu_sapporo_max17050_endpoint(
    semu_sapporo_max17050 *sensor);

#endif
