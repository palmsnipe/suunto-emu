#ifndef SEMU_DEVICES_ULSAN_IOM2_H
#define SEMU_DEVICES_ULSAN_IOM2_H

#include "../soc/apollo4/dma.h"
#include "../soc/apollo4/iom.h"

typedef struct semu_ulsan_iom2 {
    semu_apollo4_iom *controller;
    semu_apollo4_dma *dma;
    semu_serial_endpoint endpoint;
    uint8_t aux_low, aux_high;
    unsigned aux_ready;
} semu_ulsan_iom2;

semu_status semu_ulsan_244_iom2_map(semu_bus *bus, semu_ulsan_iom2 *state,
    semu_scheduler *scheduler, semu_apollo4_iom_irq_fn irq, void *context,
    semu_error *error);
void semu_ulsan_244_iom2_reset(semu_ulsan_iom2 *state);
void semu_ulsan_244_iom2_destroy(semu_ulsan_iom2 *state);

#endif
