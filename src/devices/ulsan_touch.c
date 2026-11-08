#include "ulsan_touch.h"

#include <string.h>

#define TOUCH_PIN 100u

static void put_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
}

/* E-ULS-0096. The firmware reads the length and the report as two IOM commands. */
void semu_ulsan_touch_reset(semu_ulsan_touch *touch)
{
    touch->state = SEMU_ULSAN_TOUCH_IDLE;
    touch->timestamp = 0u;
    if (touch->gpio != NULL)
        (void)semu_apollo4_gpio_set_input(touch->gpio, TOUCH_PIN, 1, NULL);
}

semu_status semu_ulsan_touch_point(semu_ulsan_touch *touch,
    int32_t x, int32_t y, int32_t pressure, semu_error *error)
{
    if (touch->gpio == NULL) {
        semu_error_set(error, SEMU_ERR_STATE, "Race S touch is disconnected");
        return SEMU_ERR_STATE;
    }
    if (x < 0 || x > 4095 || y < 0 || y > 4095 || pressure < 0 || pressure > 4095) {
        semu_error_set(error, SEMU_ERR_RANGE, "Race S touch point is out of range");
        return SEMU_ERR_RANGE;
    }
    if (touch->state != SEMU_ULSAN_TOUCH_IDLE) {
        semu_error_set(error, SEMU_ERR_STATE, "Race S touch report is pending");
        return SEMU_ERR_STATE;
    }
    touch->timestamp = (uint16_t)(touch->timestamp + 50u);
    memset(touch->report, 0, sizeof(touch->report));
    touch->report[2] = 1u;
    put_u16(touch->report + 3u, touch->timestamp);
    touch->report[5] = 1u;
    touch->report[8] = pressure > 0 ? 0x80u : 0u;
    put_u16(touch->report + 9u, (uint16_t)x);
    put_u16(touch->report + 11u, (uint16_t)y);
    touch->state = SEMU_ULSAN_TOUCH_HEADER;
    return semu_apollo4_gpio_set_input(touch->gpio, TOUCH_PIN, 0, error);
}

semu_transaction_result semu_ulsan_touch_transfer(void *context,
    semu_serial_transaction *transaction, semu_error *error)
{
    semu_ulsan_touch *touch = context;

    if (transaction->address == 0x24u && transaction->tx_size == 0u &&
        transaction->rx != NULL) {
        if (touch->state == SEMU_ULSAN_TOUCH_HEADER && transaction->rx_size == 2u) {
            transaction->rx[0] = (uint8_t)sizeof(touch->report);
            transaction->rx[1] = 0u;
            touch->state = SEMU_ULSAN_TOUCH_BODY;
            return SEMU_TRANSACTION_OK;
        }
        if (touch->state == SEMU_ULSAN_TOUCH_BODY &&
            transaction->rx_size == sizeof(touch->report)) {
            memcpy(transaction->rx, touch->report, sizeof(touch->report));
            touch->state = SEMU_ULSAN_TOUCH_IDLE;
            (void)semu_apollo4_gpio_set_input(touch->gpio, TOUCH_PIN, 1, NULL);
            return SEMU_TRANSACTION_OK;
        }
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "Race S IOM3 touch transaction refused");
    return SEMU_TRANSACTION_REFUSE;
}
