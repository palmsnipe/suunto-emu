#ifndef SEMU_DEVICES_SAPPORO_IOM4_H
#define SEMU_DEVICES_SAPPORO_IOM4_H

#include "semu/bus.h"
#include "semu/types.h"

/*
 * Sapporo-2.35.34 live IOM4 block at 0x40054000 (E-SAP-0036). The law
 * mirrors the lane oracle composition
 * SPI.SapporoApollo4Iom4 -> AmbiqApollo4_IOMaster (wrapper-owned DMA
 * register window + upstream command/FIFO/interrupt engine) for the
 * endpoint set the 2.35 machine registers: observed device 0x28
 * (deterministic zeros) and MAX17050 fuel gauge 0x36. Addresses beyond
 * that set fail the transaction the way an unregistered peripheral
 * does on the lane (IllegalCommand status). Backed by four
 * byte-identical lane probe pairs and the in-tree guest census
 * recorded in E-SAP-0036.
 *
 * Only semu_apollo4_select_profile("sapporo-2.35.34") attaches this
 * module (semu_apollo4_iom_set_live235); every other profile keeps the
 * E-A4-IOM-001 shared controller byte-for-byte.
 */

typedef struct semu_sapporo_iom4 semu_sapporo_iom4;

typedef void (*semu_sapporo_iom4_irq_fn)(void *context, unsigned irq,
                                         int level);

semu_sapporo_iom4 *semu_sapporo_iom4_create(
    semu_bus *bus, semu_sapporo_iom4_irq_fn irq_sink, void *irq_context,
    semu_error *error);
void semu_sapporo_iom4_destroy(semu_sapporo_iom4 *iom4);
void semu_sapporo_iom4_reset(semu_sapporo_iom4 *iom4);

semu_status semu_sapporo_iom4_read(semu_sapporo_iom4 *iom4,
                                   uint32_t offset, unsigned width,
                                   uint32_t *value, semu_error *error);
semu_status semu_sapporo_iom4_write(semu_sapporo_iom4 *iom4,
                                    uint32_t offset, unsigned width,
                                    uint32_t value, semu_error *error);

#endif
