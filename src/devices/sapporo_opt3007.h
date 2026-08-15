#ifndef SEMU_SAPPORO_OPT3007_H
#define SEMU_SAPPORO_OPT3007_H

#include "semu/peripheral.h"

typedef struct semu_sapporo_opt3007 semu_sapporo_opt3007;

semu_sapporo_opt3007 *semu_sapporo_opt3007_create(uint8_t address,
                                                   semu_error *error);
void semu_sapporo_opt3007_destroy(semu_sapporo_opt3007 *sensor);
void semu_sapporo_opt3007_reset(void *context);
semu_serial_endpoint semu_sapporo_opt3007_endpoint(
    semu_sapporo_opt3007 *sensor);

#endif
