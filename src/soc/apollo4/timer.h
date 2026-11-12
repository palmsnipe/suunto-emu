#ifndef SEMU_APOLLO4_TIMER_H
#define SEMU_APOLLO4_TIMER_H

#include "semu/scheduler.h"
#include "../../core/snapshot_io.h"

typedef struct semu_apollo4_timer semu_apollo4_timer;

typedef void (*semu_apollo4_timer_irq_fn)(void *context, unsigned channel,
                                          int level);

semu_apollo4_timer *semu_apollo4_timer_create(
    semu_scheduler *scheduler, semu_apollo4_timer_irq_fn irq,
    void *irq_context, semu_error *error);
void semu_apollo4_timer_destroy(semu_apollo4_timer *timer);
void semu_apollo4_timer_reset(semu_apollo4_timer *timer);
/* Wismar Timer14 edge-mode law (ticket 760, E-WIS-0001): admits exactly
 * the channel-14 Edge(1) selector, mapped to Upcount(2); the shared
 * profiles keep refusing it. */
void semu_apollo4_timer_enable_wismar_timer14_edge(semu_apollo4_timer *timer);
semu_status semu_apollo4_timer_read(semu_apollo4_timer *timer,
                                    uint32_t offset, unsigned width,
                                    uint32_t *value, semu_error *error);
semu_status semu_apollo4_timer_write(semu_apollo4_timer *timer,
                                     uint32_t offset, unsigned width,
                                     uint32_t value, semu_error *error);
semu_status semu_apollo4_timer_snapshot_write(
    const semu_apollo4_timer *timer, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_apollo4_timer_snapshot_read(
    semu_apollo4_timer *timer, semu_snapshot_reader *reader,
    semu_error *error);
semu_status semu_apollo4_timer_snapshot_resolve_event(
    semu_apollo4_timer *timer, uint32_t subject,
    semu_event_callback *callback, void **context, semu_error *error);

#endif
