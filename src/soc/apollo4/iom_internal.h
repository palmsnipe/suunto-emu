#ifndef SEMU_APOLLO4_IOM_INTERNAL_H
#define SEMU_APOLLO4_IOM_INTERNAL_H

#include "iom.h"

#define SEMU_APOLLO4_IOM_OBSERVED_REGISTER_COUNT 14u

struct semu_apollo4_iom {
    semu_bus *bus;
    unsigned irq;
    semu_apollo4_iom_irq_fn irq_sink;
    void *irq_context;
    semu_dma_request_sink_fn dma_sink;
    void *dma_context;
    semu_scheduler *scheduler;
    const semu_serial_endpoint *endpoint;
    int endpoint_attached;
    uint32_t inten;
    uint32_t intstat;
    uint32_t dma_trig_en;
    uint32_t dma_trig_stat;
    uint32_t dma_config;
    uint32_t dma_count;
    uint32_t dma_target;
    uint32_t dma_status;
    uint32_t device_config;
    int irq_level;
    struct semu_sapporo_iom4 *live235;
    uint32_t observed_registers[SEMU_APOLLO4_IOM_OBSERVED_REGISTER_COUNT];
};

/*
 * E-SAP-0036 lane-mirror seam, defined in iom_live235.c.  Both entry
 * points are inert while live235 is NULL, which is every profile other
 * than sapporo-2.35.34, so the shared E-A4-IOM-001 law keeps its
 * byte-for-byte behaviour there.
 */
int semu_apollo4_iom_live_owns(const semu_apollo4_iom *iom);
void semu_apollo4_iom_live_reset(semu_apollo4_iom *iom);

#endif
