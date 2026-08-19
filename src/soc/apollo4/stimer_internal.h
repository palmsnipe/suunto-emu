#ifndef SEMU_APOLLO4_STIMER_INTERNAL_H
#define SEMU_APOLLO4_STIMER_INTERNAL_H

#include "stimer.h"

typedef struct stimer_compare {
    semu_apollo4_stimer *owner;
    unsigned number;
    uint32_t deadline;
    semu_event_id event;
    uint8_t enabled;
    uint8_t event_valid;
} stimer_compare;

struct semu_apollo4_stimer {
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4_stimer_irq_fn irq;
    void *irq_context;
    uint64_t counter_epoch;
    uint32_t counter_base;
    uint32_t configuration;
    uint32_t interrupt_enable;
    uint32_t pending;
    uint32_t nvram[4];
    stimer_compare compare[2];
};

#endif
