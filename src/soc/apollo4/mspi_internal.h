#ifndef SEMU_APOLLO4_MSPI_INTERNAL_H
#define SEMU_APOLLO4_MSPI_INTERNAL_H

#include "mspi.h"

struct semu_apollo4_mspi {
    semu_bus *bus;
    uint32_t base;
    unsigned irq_number;
    semu_apollo4_mspi_irq_fn irq;
    void *irq_context;
    semu_dma_request_sink_fn dma_sink;
    void *dma_context;
    semu_serial_endpoint endpoint;
    int endpoint_attached;
    uint32_t registers[SEMU_APOLLO4_MSPI_SIZE / 4u];
    uint32_t status;
    uint32_t dma_status;
    int irq_level;
    uint8_t dma_buffer[4100];
    semu_serial_transaction dma_transaction;
};

#endif
