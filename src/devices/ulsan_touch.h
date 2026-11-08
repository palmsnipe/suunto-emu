#ifndef SEMU_DEVICES_ULSAN_TOUCH_H
#define SEMU_DEVICES_ULSAN_TOUCH_H

#include "semu/peripheral.h"
#include "../soc/apollo4/gpio.h"

typedef struct {
    semu_apollo4_gpio *gpio;
    uint8_t report[13];
    uint16_t timestamp;
    enum { SEMU_ULSAN_TOUCH_IDLE, SEMU_ULSAN_TOUCH_HEADER, SEMU_ULSAN_TOUCH_BODY } state;
} semu_ulsan_touch;

void semu_ulsan_touch_reset(semu_ulsan_touch *touch);
semu_status semu_ulsan_touch_point(semu_ulsan_touch *touch,
    int32_t x, int32_t y, int32_t pressure, semu_error *error);
semu_transaction_result semu_ulsan_touch_transfer(void *context,
    semu_serial_transaction *transaction, semu_error *error);

#endif
