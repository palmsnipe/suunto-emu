#ifndef SEMU_PERIPHERAL_H
#define SEMU_PERIPHERAL_H

#include "semu/types.h"

typedef struct semu_serial_transaction {
    uint8_t address;
    uint8_t chip_select;
    const uint8_t *tx;
    size_t tx_size;
    uint8_t *rx;
    size_t rx_size;
} semu_serial_transaction;

typedef semu_transaction_result (*semu_serial_transfer_fn)(
    void *context, semu_serial_transaction *transaction, semu_error *error);

typedef struct semu_serial_endpoint {
    const char *name;
    semu_serial_transfer_fn transfer;
    void *context;
} semu_serial_endpoint;

/* Optional callbacks are supplied separately by device constructors. */
typedef void (*semu_peripheral_reset_fn)(void *context);
typedef void (*semu_peripheral_signal_fn)(void *context, unsigned signal,
                                          int level);

typedef enum semu_dma_direction {
    SEMU_DMA_TO_ENDPOINT = 0,
    SEMU_DMA_FROM_ENDPOINT
} semu_dma_direction;

typedef void (*semu_dma_completion_fn)(void *context,
                                       semu_transaction_result result);

typedef struct semu_dma_request {
    const char *controller_id;
    semu_dma_direction direction;
    uint32_t guest_address;
    uint32_t count;
    const semu_serial_endpoint *endpoint;
    uint8_t continuation;
    semu_dma_completion_fn completion;
    void *completion_context;
} semu_dma_request;

typedef semu_transaction_result (*semu_dma_request_sink_fn)(
    void *context, const semu_dma_request *request, semu_error *error);

#endif
