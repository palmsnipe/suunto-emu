#ifndef SEMU_APOLLO4_MRAM_H
#define SEMU_APOLLO4_MRAM_H

#include <stdint.h>

#include "semu/bus.h"
#include "../../core/snapshot_io.h"

#define SEMU_APOLLO4_MRAM_BASE 0x40014000u
#define SEMU_APOLLO4_MRAM_SIZE 0x1000u

typedef struct semu_apollo4_mram semu_apollo4_mram;

semu_apollo4_mram *semu_apollo4_mram_create(semu_bus *bus,
                                             semu_error *error);
void semu_apollo4_mram_destroy(semu_apollo4_mram *mram);
void semu_apollo4_mram_reset(void *context);

semu_status semu_apollo4_mram_read(void *context, uint32_t offset,
                                   unsigned width, uint32_t *value,
                                   semu_error *error);
semu_status semu_apollo4_mram_write(void *context, uint32_t offset,
                                    unsigned width, uint32_t value,
                                    semu_error *error);
const semu_bus_device_ops *semu_apollo4_mram_bus_ops(void);
semu_status semu_apollo4_mram_snapshot_write(
    const semu_apollo4_mram *mram, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_apollo4_mram_snapshot_read(
    semu_apollo4_mram *mram, semu_snapshot_reader *reader,
    semu_error *error);

#endif
