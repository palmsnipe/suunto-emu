#ifndef SEMU_APOLLO4_RESET_H
#define SEMU_APOLLO4_RESET_H

#include <stddef.h>
#include <stdint.h>

#include "semu/bus.h"
#include "semu/scheduler.h"

#define SEMU_APOLLO4_RESET_MAX_CALLBACKS 16u

typedef semu_status (*semu_apollo4_reset_callback)(void *context,
                                                    semu_error *error);

typedef struct semu_apollo4_reset_controller semu_apollo4_reset_controller;

semu_apollo4_reset_controller *semu_apollo4_reset_controller_create(
    semu_bus *bus, semu_scheduler *scheduler, semu_error *error);
void semu_apollo4_reset_controller_destroy(
    semu_apollo4_reset_controller *controller);
void semu_apollo4_reset_controller_reset(void *context);

semu_status semu_apollo4_reset_controller_register(
    semu_apollo4_reset_controller *controller,
    semu_apollo4_reset_callback callback, void *callback_context,
    semu_error *error);

semu_status semu_apollo4_reset_controller_read(void *context, uint32_t offset,
                                                unsigned width, uint32_t *value,
                                                semu_error *error);
semu_status semu_apollo4_reset_controller_write(void *context, uint32_t offset,
                                                 unsigned width, uint32_t value,
                                                 semu_error *error);
const semu_bus_device_ops *semu_apollo4_reset_controller_bus_ops(void);

#endif
