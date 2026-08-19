#ifndef SEMU_APOLLO4_CLOCK_H
#define SEMU_APOLLO4_CLOCK_H

#include <stdint.h>

#include "semu/bus.h"
#include "semu/scheduler.h"
#include "../../core/snapshot_io.h"

#define SEMU_APOLLO4_CLOCK_BASE 0x40004000u
#define SEMU_APOLLO4_CLOCK_SIZE 0x800u

typedef struct semu_apollo4_clock semu_apollo4_clock;

semu_apollo4_clock *semu_apollo4_clock_create(semu_bus *bus,
                                              semu_scheduler *scheduler,
                                              semu_error *error);
void semu_apollo4_clock_destroy(semu_apollo4_clock *clock);
void semu_apollo4_clock_reset(void *context);

semu_status semu_apollo4_clock_read(void *context, uint32_t offset,
                                    unsigned width, uint32_t *value,
                                    semu_error *error);
semu_status semu_apollo4_clock_write(void *context, uint32_t offset,
                                     unsigned width, uint32_t value,
                                     semu_error *error);
const semu_bus_device_ops *semu_apollo4_clock_bus_ops(void);
semu_status semu_apollo4_clock_snapshot_write(
    const semu_apollo4_clock *clock, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_apollo4_clock_snapshot_read(
    semu_apollo4_clock *clock, semu_snapshot_reader *reader,
    semu_error *error);

#endif
