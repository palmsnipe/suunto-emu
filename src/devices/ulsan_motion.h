#ifndef SEMU_DEVICES_ULSAN_MOTION_H
#define SEMU_DEVICES_ULSAN_MOTION_H

#include "../soc/apollo4/dma.h"
#include "../soc/apollo4/iom.h"

typedef struct semu_ulsan_motion {
    semu_apollo4_iom *controller;
    semu_serial_endpoint endpoint;
    uint8_t registers[128];
    unsigned hub_setup;
    int hub_consumed;
} semu_ulsan_motion;

semu_status semu_ulsan_244_motion_map(semu_bus *bus, semu_ulsan_motion *state,
    semu_apollo4_dma *dma, semu_scheduler *scheduler, semu_apollo4_iom_irq_fn irq,
    void *context, semu_error *error);
void semu_ulsan_244_motion_reset(semu_ulsan_motion *state);

#endif
