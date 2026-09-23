#ifndef SEMU_APOLLO4_IOM_H
#define SEMU_APOLLO4_IOM_H

#include <stdint.h>

#include "semu/bus.h"
#include "semu/peripheral.h"
#include "semu/scheduler.h"
#include "../../core/snapshot_io.h"

#define SEMU_APOLLO4_IOM_SIZE 0x1000u

#define SEMU_APOLLO4_IOM0_BASE 0x40050000u
#define SEMU_APOLLO4_IOM0_IRQ 6u
#define SEMU_APOLLO4_IOM2_BASE 0x40052000u
#define SEMU_APOLLO4_IOM2_IRQ 8u
#define SEMU_APOLLO4_IOM3_BASE 0x40053000u
#define SEMU_APOLLO4_IOM3_IRQ 9u
#define SEMU_APOLLO4_IOM4_BASE 0x40054000u
#define SEMU_APOLLO4_IOM4_IRQ 10u
#define SEMU_APOLLO4_IOM6_BASE 0x40056000u
#define SEMU_APOLLO4_IOM6_IRQ 12u

typedef struct semu_apollo4_iom semu_apollo4_iom;
struct semu_sapporo_iom4;

typedef void (*semu_apollo4_iom_irq_fn)(void *context, unsigned irq,
                                         int level);

semu_apollo4_iom *semu_apollo4_iom_create(
    semu_bus *bus, uint32_t base, unsigned irq,
    semu_apollo4_iom_irq_fn irq_sink, void *irq_context,
    semu_dma_request_sink_fn dma_sink, void *dma_context,
    semu_scheduler *scheduler, semu_error *error);
void semu_apollo4_iom_destroy(semu_apollo4_iom *iom);
void semu_apollo4_iom_reset(void *context);
/* E-SAP-0036: profile selection routes the 0x40054000 window of the
 * sapporo-2.35.34 machine to the lane-mirror engine; NULL restores the
 * shared E-A4-IOM-001 law. */
void semu_apollo4_iom_set_live235(semu_apollo4_iom *iom,
                                  struct semu_sapporo_iom4 *live);
/* E-SAP-0039: exact negative pressure probes, gated to the 2.35 IOM2. */
void semu_apollo4_iom_set_pressure235(semu_apollo4_iom *iom, int enabled);

semu_status semu_apollo4_iom_attach_endpoint(
    semu_apollo4_iom *iom, const semu_serial_endpoint *endpoint,
    semu_error *error);

semu_status semu_apollo4_iom_read(void *context, uint32_t offset,
                                   unsigned width, uint32_t *value,
                                   semu_error *error);
semu_status semu_apollo4_iom_write(void *context, uint32_t offset,
                                    unsigned width, uint32_t value,
                                    semu_error *error);
const semu_bus_device_ops *semu_apollo4_iom_bus_ops(void);
semu_status semu_apollo4_iom_snapshot_write(
    const semu_apollo4_iom *iom, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_apollo4_iom_snapshot_read(
    semu_apollo4_iom *iom, semu_snapshot_reader *reader,
    semu_error *error);

#endif
