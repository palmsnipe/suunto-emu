#ifndef SEMU_APOLLO4_RSTGEN_H
#define SEMU_APOLLO4_RSTGEN_H

#include <stdint.h>

#include "semu/bus.h"
#include "../../core/snapshot_io.h"

#define SEMU_APOLLO4_RSTGEN_BASE UINT32_C(0x40000000)
#define SEMU_APOLLO4_RSTGEN_SIZE UINT32_C(0x400)

typedef struct semu_apollo4_rstgen semu_apollo4_rstgen;

semu_apollo4_rstgen *semu_apollo4_rstgen_create(
    semu_bus *bus, semu_error *error);
void semu_apollo4_rstgen_destroy(semu_apollo4_rstgen *rstgen);
void semu_apollo4_rstgen_reset(void *context);

semu_status semu_apollo4_rstgen_read(
    void *context, uint32_t offset, unsigned width, uint32_t *value,
    semu_error *error);
semu_status semu_apollo4_rstgen_write(
    void *context, uint32_t offset, unsigned width, uint32_t value,
    semu_error *error);
semu_status semu_apollo4_rstgen_snapshot_write(
    const semu_apollo4_rstgen *rstgen, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_apollo4_rstgen_snapshot_read(
    semu_apollo4_rstgen *rstgen, semu_snapshot_reader *reader,
    semu_error *error);

#endif
