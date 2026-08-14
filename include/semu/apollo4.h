#ifndef SEMU_APOLLO4_H
#define SEMU_APOLLO4_H

#include "semu/bus.h"

typedef struct semu_apollo4 semu_apollo4;

semu_apollo4 *semu_apollo4_create(semu_bus *bus, semu_error *error);
void semu_apollo4_destroy(semu_apollo4 *soc);
void semu_apollo4_reset(semu_apollo4 *soc);
semu_status semu_apollo4_set_gpio_input(semu_apollo4 *soc, unsigned pin,
                                        int level, semu_error *error);
int semu_apollo4_get_gpio_input(const semu_apollo4 *soc, unsigned pin);

#endif
