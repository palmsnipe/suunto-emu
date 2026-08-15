#ifndef SEMU_APOLLO4_DMA_H
#define SEMU_APOLLO4_DMA_H

#include <stdint.h>

#include "semu/bus.h"
#include "semu/peripheral.h"
#include "semu/scheduler.h"

typedef struct semu_apollo4_dma semu_apollo4_dma;

semu_apollo4_dma *semu_apollo4_dma_create(semu_bus *bus,
                                          semu_scheduler *scheduler,
                                          semu_error *error);
void semu_apollo4_dma_destroy(semu_apollo4_dma *dma);
void semu_apollo4_dma_reset(void *context);

semu_transaction_result semu_apollo4_dma_execute(
    void *context, const semu_dma_request *request, semu_error *error);

#endif
