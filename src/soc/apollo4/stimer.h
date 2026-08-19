#ifndef SEMU_APOLLO4_STIMER_H
#define SEMU_APOLLO4_STIMER_H
#include <stdint.h>
#include "semu/bus.h"
#include "semu/scheduler.h"
#include "../../core/snapshot_io.h"
#define SEMU_APOLLO4_STIMER_BASE 0x40008800u
#define SEMU_APOLLO4_STIMER_SIZE 0x200u
typedef struct semu_apollo4_stimer semu_apollo4_stimer;
typedef enum semu_apollo4_stimer_irq {
    SEMU_APOLLO4_STIMER_IRQ_A = 32u,
    SEMU_APOLLO4_STIMER_IRQ_B = 33u,
    SEMU_APOLLO4_STIMER_IRQ_C = 34u,
    SEMU_APOLLO4_STIMER_IRQ_D = 35u,
    SEMU_APOLLO4_STIMER_IRQ_E = 36u,
    SEMU_APOLLO4_STIMER_IRQ_F = 37u,
    SEMU_APOLLO4_STIMER_IRQ_G = 38u,
    SEMU_APOLLO4_STIMER_IRQ_H = 39u,
    SEMU_APOLLO4_STIMER_IRQ_I = 40u
} semu_apollo4_stimer_irq;
typedef void (*semu_apollo4_stimer_irq_fn)(void *context, unsigned irq,
                                           int level);
semu_apollo4_stimer *semu_apollo4_stimer_create(
    semu_bus *bus, semu_scheduler *scheduler,
    semu_apollo4_stimer_irq_fn irq, void *irq_context, semu_error *error);
void semu_apollo4_stimer_destroy(semu_apollo4_stimer *stimer);
void semu_apollo4_stimer_reset(void *context);
semu_status semu_apollo4_stimer_read(void *context, uint32_t offset,
                                     unsigned width, uint32_t *value,
                                     semu_error *error);
semu_status semu_apollo4_stimer_write(void *context, uint32_t offset,
                                      unsigned width, uint32_t value,
                                      semu_error *error);
const semu_bus_device_ops *semu_apollo4_stimer_bus_ops(void);
semu_status semu_apollo4_stimer_snapshot_write(
    const semu_apollo4_stimer *stimer, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_apollo4_stimer_snapshot_read(
    semu_apollo4_stimer *stimer, semu_snapshot_reader *reader,
    semu_error *error);
semu_status semu_apollo4_stimer_snapshot_resolve_event(
    semu_apollo4_stimer *stimer, uint32_t subject,
    semu_event_callback *callback, void **context, semu_error *error);
void semu_apollo4_stimer_event(void *context, uint64_t now);
#endif
