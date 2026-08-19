#ifndef SEMU_APOLLO4_POWER_H
#define SEMU_APOLLO4_POWER_H

#include <stdint.h>

#include "semu/bus.h"
#include "semu/scheduler.h"
#include "../../core/snapshot_io.h"

#define SEMU_APOLLO4_POWER_BASE 0x40021000u
#define SEMU_APOLLO4_POWER_SIZE 0x400u

typedef struct semu_apollo4_power semu_apollo4_power;

typedef enum semu_apollo4_power_gate {
    SEMU_APOLLO4_POWER_GATE_NEMA = 0,
    SEMU_APOLLO4_POWER_GATE_SHARED_SRAM,
    SEMU_APOLLO4_POWER_GATE_SIMO_BUCK
} semu_apollo4_power_gate;

typedef void (*semu_apollo4_power_state_callback)(
    void *context, semu_apollo4_power_gate gate, int enabled,
    uint64_t virtual_time_ns);

semu_apollo4_power *semu_apollo4_power_create(
    semu_bus *bus, semu_scheduler *scheduler,
    semu_apollo4_power_state_callback callback, void *callback_context,
    semu_error *error);
void semu_apollo4_power_destroy(semu_apollo4_power *power);
void semu_apollo4_power_reset(void *context);

semu_status semu_apollo4_power_read(void *context, uint32_t offset,
                                    unsigned width, uint32_t *value,
                                    semu_error *error);
semu_status semu_apollo4_power_write(void *context, uint32_t offset,
                                     unsigned width, uint32_t value,
                                     semu_error *error);
const semu_bus_device_ops *semu_apollo4_power_bus_ops(void);
const char *semu_apollo4_power_gate_name(semu_apollo4_power_gate gate);
semu_status semu_apollo4_power_snapshot_write(
    const semu_apollo4_power *power, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_apollo4_power_snapshot_read(
    semu_apollo4_power *power, semu_snapshot_reader *reader,
    semu_error *error);

#endif
