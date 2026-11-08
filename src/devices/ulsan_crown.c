#include "ulsan_crown.h"

#include <string.h>

/* E-ULS-0091. PAT9126 configuration and readback with the idle crown. */
void semu_ulsan_crown_reset(semu_ulsan_crown *crown)
{
    memset(crown->registers, 0, sizeof(crown->registers));
    crown->registers[0] = 0x31u;
}

semu_transaction_result semu_ulsan_crown_transfer(void *context,
    semu_serial_transaction *transaction, semu_error *error)
{
    static const uint8_t writes[][2] = {
        {0x05u, 0xb8u}, {0x06u, 0x17u}, {0x06u, 0x97u}, {0x09u, 0x00u},
        {0x09u, 0x5au}, {0x0du, 0x80u}, {0x0eu, 0xffu}, {0x19u, 0x04u},
        {0x2bu, 0x6du}, {0x2du, 0x00u}, {0x4bu, 0x04u}, {0x7cu, 0x82u},
        {0x7fu, 0x00u}
    };
    static const uint8_t reads[] = {
        0x00u, 0x05u, 0x09u, 0x0du, 0x0eu, 0x19u, 0x2bu, 0x2du, 0x4bu, 0x7cu
    };
    semu_ulsan_crown *crown = context;
    size_t i;

    if (transaction->address == 0x75u && transaction->tx != NULL) {
        if (transaction->tx_size == 2u && transaction->rx_size == 0u) {
            for (i = 0u; i < sizeof(writes) / sizeof(writes[0]); ++i) {
                if (memcmp(transaction->tx, writes[i], sizeof(writes[i])) == 0) {
                    crown->registers[writes[i][0]] = writes[i][1];
                    return SEMU_TRANSACTION_OK;
                }
            }
        }
        if (transaction->tx_size == 1u && transaction->rx_size == 1u &&
            transaction->rx != NULL &&
            memchr(reads, transaction->tx[0], sizeof(reads)) != NULL) {
            transaction->rx[0] = crown->registers[transaction->tx[0]];
            return SEMU_TRANSACTION_OK;
        }
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Race S IOM6 crown transaction refused (address=0x%02x tx=%zu rx=%zu)",
                   (unsigned)transaction->address, transaction->tx_size, transaction->rx_size);
    return SEMU_TRANSACTION_REFUSE;
}
