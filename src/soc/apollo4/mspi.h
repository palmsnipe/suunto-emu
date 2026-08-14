#ifndef SEMU_APOLLO4_MSPI_H
#define SEMU_APOLLO4_MSPI_H

#include <stdint.h>

#include "semu/bus.h"
#include "semu/peripheral.h"

#define SEMU_APOLLO4_MSPI1_BASE 0x40061000u
#define SEMU_APOLLO4_MSPI2_BASE 0x40062000u
#define SEMU_APOLLO4_MSPI_SIZE 0x1000u
#define SEMU_APOLLO4_MSPI1_IRQ 21u
#define SEMU_APOLLO4_MSPI2_IRQ 22u

#define SEMU_APOLLO4_MSPI_INTEN 0x200u
#define SEMU_APOLLO4_MSPI_INTSTAT 0x204u
#define SEMU_APOLLO4_MSPI_INTCLR 0x208u
#define SEMU_APOLLO4_MSPI_INTSET 0x20cu

#define SEMU_APOLLO4_MSPI1_CONTROL 0x2a0u
#define SEMU_APOLLO4_MSPI1_DESCRIPTOR 0x2a8u
#define SEMU_APOLLO4_MSPI1_QUEUE_CONTROL 0x100u
#define SEMU_APOLLO4_MSPI1_QUEUE_ADDRESS 0x108u
#define SEMU_APOLLO4_MSPI1_QUEUE_DEVICE 0x10cu
#define SEMU_APOLLO4_MSPI1_QUEUE_COUNT 0x110u

#define SEMU_APOLLO4_MSPI2_COMMAND 0x00u
#define SEMU_APOLLO4_MSPI2_ADDRESS 0x08u
#define SEMU_APOLLO4_MSPI2_DATA 0x0cu
#define SEMU_APOLLO4_MSPI2_INSTRUCTION 0x94u
#define SEMU_APOLLO4_MSPI2_DMA_CONFIG 0x100u
#define SEMU_APOLLO4_MSPI2_DMA_STATUS 0x104u
#define SEMU_APOLLO4_MSPI2_DMA_TARGET 0x108u
#define SEMU_APOLLO4_MSPI2_DMA_DEVICE 0x10cu
#define SEMU_APOLLO4_MSPI2_DMA_COUNT 0x110u

typedef struct semu_apollo4_mspi semu_apollo4_mspi;

typedef void (*semu_apollo4_mspi_irq_fn)(void *context, unsigned irq,
                                         int level);

semu_apollo4_mspi *semu_apollo4_mspi_create(
    semu_bus *bus, uint32_t base, unsigned irq,
    semu_apollo4_mspi_irq_fn irq_sink, void *irq_context,
    semu_dma_request_sink_fn dma_sink, void *dma_context, semu_error *error);
void semu_apollo4_mspi_destroy(semu_apollo4_mspi *mspi);
void semu_apollo4_mspi_reset(void *context);

semu_status semu_apollo4_mspi_read(void *context, uint32_t offset,
                                   unsigned width, uint32_t *value,
                                   semu_error *error);
semu_status semu_apollo4_mspi_write(void *context, uint32_t offset,
                                    unsigned width, uint32_t value,
                                    semu_error *error);
const semu_bus_device_ops *semu_apollo4_mspi_bus_ops(void);

semu_status semu_apollo4_mspi_attach_endpoint(
    semu_apollo4_mspi *mspi, const semu_serial_endpoint *endpoint,
    semu_error *error);

#endif
