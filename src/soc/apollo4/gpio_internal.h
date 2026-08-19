#ifndef SEMU_APOLLO4_GPIO_INTERNAL_H
#define SEMU_APOLLO4_GPIO_INTERNAL_H

#include "gpio.h"

struct semu_apollo4_gpio {
    semu_bus *bus;
    uint32_t pin_configuration[SEMU_APOLLO4_GPIO_COUNT];
    uint8_t input[SEMU_APOLLO4_GPIO_COUNT];
    uint8_t direction[SEMU_APOLLO4_GPIO_COUNT];
    uint8_t edge[SEMU_APOLLO4_GPIO_COUNT];
    uint32_t output[SEMU_APOLLO4_GPIO_IRQ_BANKS];
    uint32_t output_set[SEMU_APOLLO4_GPIO_IRQ_BANKS];
    uint32_t output_clear[SEMU_APOLLO4_GPIO_IRQ_BANKS];
    uint32_t interrupt_enable[SEMU_APOLLO4_GPIO_IRQ_BANKS];
    uint32_t interrupt_status[SEMU_APOLLO4_GPIO_IRQ_BANKS];
    uint32_t pad_key;
    semu_apollo4_gpio_irq_fn irq;
    void *irq_context;
    semu_apollo4_gpio_output_fn output_observer;
    void *output_context;
};

#endif
