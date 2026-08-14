#ifndef SEMU_APOLLO4_INTERNAL_H
#define SEMU_APOLLO4_INTERNAL_H

#include "semu/apollo4.h"

#define SEMU_APOLLO4_GPIO_COUNT 128u
#define SEMU_APOLLO4_REGBANK_WORDS 32u

typedef struct semu_regbank {
    const char *name;
    uint32_t allowed[SEMU_APOLLO4_REGBANK_WORDS];
    uint32_t values[SEMU_APOLLO4_REGBANK_WORDS];
    size_t count;
} semu_regbank;

struct semu_apollo4 {
    semu_bus *bus;
    uint8_t gpio_level[SEMU_APOLLO4_GPIO_COUNT];
    semu_regbank clock;
    semu_regbank power;
    semu_regbank mram;
    semu_regbank crypto;
    semu_regbank system;
};

semu_status semu_regbank_read(void *context, uint32_t offset, unsigned width,
                              uint32_t *value, semu_error *error);
semu_status semu_regbank_write(void *context, uint32_t offset, unsigned width,
                               uint32_t value, semu_error *error);
void semu_regbank_reset(void *context);

#endif
