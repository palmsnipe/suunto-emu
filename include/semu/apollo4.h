#ifndef SEMU_APOLLO4_H
#define SEMU_APOLLO4_H

#include "semu/bus.h"
#include "semu/scheduler.h"

typedef struct semu_apollo4 semu_apollo4;

typedef void (*semu_apollo4_irq_fn)(void *context, unsigned irq, int level);

semu_apollo4 *semu_apollo4_create(semu_bus *bus, semu_error *error);
void semu_apollo4_destroy(semu_apollo4 *soc);
void semu_apollo4_reset(semu_apollo4 *soc);

semu_status semu_apollo4_init(semu_apollo4 *soc, semu_scheduler *scheduler,
                               semu_apollo4_irq_fn irq_sink,
                               void *irq_context, semu_error *error);

/* Profile selection seam (ticket 710, E-SAP-0032): the board map calls
 * this with the validated board profile id after init. Selecting
 * sapporo-2.35.34 enables the live one-second RTC alarm block; every
 * other verified profile keeps the auxiliary register stub. */
semu_status semu_apollo4_select_profile(semu_apollo4 *soc,
                                        const char *profile_id,
                                        semu_error *error);

semu_status semu_apollo4_set_gpio_input(semu_apollo4 *soc, unsigned pin,
                                        int level, semu_error *error);
int semu_apollo4_get_gpio_input(const semu_apollo4 *soc, unsigned pin);

#endif
