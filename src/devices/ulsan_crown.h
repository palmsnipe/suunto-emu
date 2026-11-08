#ifndef SEMU_DEVICES_ULSAN_CROWN_H
#define SEMU_DEVICES_ULSAN_CROWN_H

#include "semu/peripheral.h"
#include "../soc/apollo4/gpio.h"

typedef struct {
    semu_apollo4_gpio *gpio;
    uint8_t registers[128];
} semu_ulsan_crown;

void semu_ulsan_crown_reset(semu_ulsan_crown *crown);
semu_status semu_ulsan_crown_rotate(semu_ulsan_crown *crown, int32_t delta,
    semu_error *error);
semu_transaction_result semu_ulsan_crown_transfer(void *context,
    semu_serial_transaction *transaction, semu_error *error);

#endif
