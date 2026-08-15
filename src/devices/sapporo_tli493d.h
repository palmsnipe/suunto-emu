#ifndef SEMU_SAPPORO_TLI493D_H
#define SEMU_SAPPORO_TLI493D_H

#include "semu/peripheral.h"

typedef struct semu_sapporo_tli493d semu_sapporo_tli493d;

semu_sapporo_tli493d *semu_sapporo_tli493d_create(uint8_t address,
                                                   semu_error *error);
void semu_sapporo_tli493d_destroy(semu_sapporo_tli493d *sensor);
void semu_sapporo_tli493d_reset(void *context);
semu_serial_endpoint semu_sapporo_tli493d_endpoint(
    semu_sapporo_tli493d *sensor);

#endif
