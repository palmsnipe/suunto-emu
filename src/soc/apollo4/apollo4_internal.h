#ifndef SEMU_APOLLO4_INTERNAL_H
#define SEMU_APOLLO4_INTERNAL_H

#include "semu/apollo4.h"

#include "auxiliary.h"
#include "clock.h"
#include "dma.h"
#include "gpio.h"
#include "mcu_control.h"
#include "mram.h"
#include "mspi.h"
#include "iom.h"
#include "power.h"
#include "reset.h"
#include "stimer.h"
#include "timer.h"
#include "uart.h"

#define SEMU_APOLLO4_GPIO_COUNT 128u

struct semu_apollo4 {
    semu_bus *bus;
    int initialized;
    semu_apollo4_irq_fn irq_sink;
    void *irq_context;
    uint8_t gpio_level[SEMU_APOLLO4_GPIO_COUNT];
    semu_apollo4_clock *clock;
    semu_apollo4_power *power;
    semu_apollo4_mcu_control *mcu_control;
    semu_apollo4_reset_controller *reset_ctrl;
    semu_apollo4_gpio *gpio;
    semu_apollo4_timer *timer;
    semu_apollo4_stimer *stimer;
    semu_apollo4_uart *uart;
    semu_apollo4_iom *iom0;
    semu_apollo4_iom *iom2;
    semu_apollo4_iom *iom3;
    semu_apollo4_iom *iom4;
    semu_apollo4_iom *iom6;
    semu_apollo4_dma *dma;
    semu_apollo4_mspi *mspi1;
    semu_apollo4_mspi *mspi2;
    semu_apollo4_mram *mram;
};

#endif
