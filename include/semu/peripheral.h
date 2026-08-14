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

#endif
