#ifndef SEMU_DEVICES_ULSAN_ALS_H
#define SEMU_DEVICES_ULSAN_ALS_H

#include "semu/peripheral.h"

semu_transaction_result semu_ulsan_als_transfer(void *context,
    semu_serial_transaction *transaction, semu_error *error);

#endif
