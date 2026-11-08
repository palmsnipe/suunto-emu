#include "ulsan_als.h"

#include <string.h>

/* E-ULS-0089. The lane supplies a deterministic zero-light FIFO. */
semu_transaction_result semu_ulsan_als_transfer(void *context,
    semu_serial_transaction *transaction, semu_error *error)
{
    static const uint8_t writes[][2] = {
        {0x00u, 0x00u}, {0x00u, 0x02u}, {0x02u, 0x00u}, {0x03u, 0x20u},
        {0x05u, 0x00u}, {0x06u, 0x00u}, {0x17u, 0xe0u}, {0x3du, 0x01u},
        {0x60u, 0x20u}, {0x61u, 0x00u}, {0x68u, 0x01u}, {0x69u, 0x00u},
        {0x69u, 0x30u}, {0x6au, 0x80u}, {0x6bu, 0x00u}, {0x6cu, 0x0cu},
        {0x6du, 0x0au}, {0x80u, 0x99u}, {0xa4u, 0x07u}
    };
    size_t i;
    (void)context;

    if (transaction->address == 0x48u && transaction->tx != NULL) {
        if (transaction->tx_size == 2u && transaction->rx_size == 0u) {
            for (i = 0u; i < sizeof(writes) / sizeof(writes[0]); ++i) {
                if (memcmp(transaction->tx, writes[i], sizeof(writes[i])) == 0)
                    return SEMU_TRANSACTION_OK;
            }
        }
        if (transaction->tx_size == 1u && transaction->rx != NULL) {
            if (transaction->tx[0] == 0x64u && transaction->rx_size == 2u) {
                transaction->rx[0] = 0x80u;
                transaction->rx[1] = 0u;
                return SEMU_TRANSACTION_OK;
            }
            if (transaction->tx[0] == 0x66u && transaction->rx_size == 64u) {
                memset(transaction->rx, 0, transaction->rx_size);
                return SEMU_TRANSACTION_OK;
            }
        }
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Race S IOM3 ALS transaction refused (address=0x%02x tx=%zu rx=%zu)",
                   (unsigned)transaction->address, transaction->tx_size, transaction->rx_size);
    return SEMU_TRANSACTION_REFUSE;
}
