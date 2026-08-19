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
    uint32_t observed_registers[SEMU_APOLLO4_IOM_OBSERVED_REGISTER_COUNT];
};

#endif
