#ifndef SEMU_APOLLO4_TIMER_INTERNAL_H
#define SEMU_APOLLO4_TIMER_INTERNAL_H

#include "timer.h"

#define TIMER_CHANNEL_COUNT 16u

typedef struct timer_channel timer_channel;
struct timer_channel {
    struct semu_apollo4_timer *owner;
    uint32_t control;
    uint32_t base_value;
    uint32_t compare[2];
    uint32_t interrupt_enable;
    uint64_t epoch;
    semu_event_id event;
    uint8_t event_valid;
    uint8_t irq_level;
};

struct semu_apollo4_timer {
    semu_scheduler *scheduler;
    semu_apollo4_timer_irq_fn irq;
    void *irq_context;
    timer_channel channels[TIMER_CHANNEL_COUNT];
    uint32_t interrupt_mask;
    uint32_t pending;
    uint32_t status_value;
    uint8_t status_written;
    uint32_t output_control;
    uint32_t auxiliary;
    uint32_t pattern;
    uint32_t observed_d8;
};

void semu_apollo4_timer_event(void *context, uint64_t now);

#endif
