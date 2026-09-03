#ifndef SEMU_APOLLO4_MCU_CONTROL_H
#define SEMU_APOLLO4_MCU_CONTROL_H

#include <stdint.h>

#include "semu/bus.h"
#include "semu/scheduler.h"

#define SEMU_APOLLO4_MCU_CONTROL_BASE 0x40020000u
#define SEMU_APOLLO4_MCU_CONTROL_SIZE 0x1000u

#define SEMU_APOLLO4_CHIPID0_OFFSET 0x04u
#define SEMU_APOLLO4_CHIPID1_OFFSET 0x08u
#define SEMU_APOLLO4_CHIPREV 0x21u

typedef struct semu_apollo4_mcu_control semu_apollo4_mcu_control;

semu_apollo4_mcu_control *semu_apollo4_mcu_control_create(
    semu_bus *bus, semu_scheduler *scheduler, semu_error *error);
void semu_apollo4_mcu_control_destroy(semu_apollo4_mcu_control *mcu);
void semu_apollo4_mcu_control_reset(void *context);

semu_status semu_apollo4_mcu_control_read(void *context, uint32_t offset,
                                          unsigned width, uint32_t *value,
                                          semu_error *error);
semu_status semu_apollo4_mcu_control_write(void *context, uint32_t offset,
                                           unsigned width, uint32_t value,
                                           semu_error *error);
const semu_bus_device_ops *semu_apollo4_mcu_control_bus_ops(void);

#endif
