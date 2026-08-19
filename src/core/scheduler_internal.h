#ifndef SEMU_SCHEDULER_INTERNAL_H
#define SEMU_SCHEDULER_INTERNAL_H

#include "semu/scheduler.h"

typedef struct semu_scheduled_event_state {
    uint64_t due_ns;
    uint64_t sequence;
    semu_event_id id;
    uint32_t kind;
    uint32_t subject;
} semu_scheduled_event_state;

typedef struct semu_scheduled_event {
    semu_scheduled_event_state state;
    semu_event_callback callback;
    void *context;
} semu_scheduled_event;

struct semu_scheduler {
    semu_scheduled_event *events;
    size_t count;
    size_t capacity;
    uint64_t now_ns;
    uint64_t next_sequence;
    semu_event_id next_id;
};

#define SEMU_SCHED_EVENT_NONE 0u
#define SEMU_SCHED_EVENT_SYSTICK 1u
#define SEMU_SCHED_EVENT_CTIMER 2u
#define SEMU_SCHED_EVENT_STIMER 3u
#define SEMU_SCHED_EVENT_UART_RX 4u
#define SEMU_SCHED_EVENT_UART_TX 5u
#define SEMU_SCHED_EVENT_CXD_RX 6u
#define SEMU_SCHED_EVENT_CXD_AWAKE 7u
#define SEMU_SCHED_EVENT_NEMA_COMPLETION 8u

semu_status semu_scheduler_schedule_tagged(semu_scheduler *scheduler,
    uint64_t delay_ns, uint32_t kind, uint32_t subject,
    semu_event_callback callback, void *context, semu_event_id *event_id,
    semu_error *error);
size_t semu_scheduler_event_count(const semu_scheduler *scheduler);
const semu_scheduled_event_state *semu_scheduler_event_get(
    const semu_scheduler *scheduler, size_t index);
semu_status semu_scheduler_restore_begin(semu_scheduler *scheduler,
    uint64_t now_ns, uint64_t next_sequence, semu_event_id next_id,
    semu_error *error);
semu_status semu_scheduler_restore_event(semu_scheduler *scheduler,
    const semu_scheduled_event_state *state, semu_event_callback callback,
    void *context, semu_error *error);

#endif
