#ifndef SEMU_SAPPORO_HSPPAD143_H
#define SEMU_SAPPORO_HSPPAD143_H

#include "semu/peripheral.h"

typedef struct semu_sapporo_hsppad143 semu_sapporo_hsppad143;

semu_sapporo_hsppad143 *semu_sapporo_hsppad143_create(uint8_t address,
                                                      semu_error *error);
void semu_sapporo_hsppad143_destroy(semu_sapporo_hsppad143 *sensor);
void semu_sapporo_hsppad143_reset(void *context);
semu_serial_endpoint semu_sapporo_hsppad143_endpoint(
    semu_sapporo_hsppad143 *sensor);

#endif
