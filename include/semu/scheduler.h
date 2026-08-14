#ifndef SEMU_SCHEDULER_H
#define SEMU_SCHEDULER_H

#include "semu/types.h"

typedef struct semu_scheduler semu_scheduler;
typedef uint64_t semu_event_id;
typedef void (*semu_event_callback)(void *context, uint64_t now_ns);

semu_scheduler *semu_scheduler_create(semu_error *error);
void semu_scheduler_destroy(semu_scheduler *scheduler);
void semu_scheduler_reset(semu_scheduler *scheduler);
uint64_t semu_scheduler_now(const semu_scheduler *scheduler);
semu_status semu_scheduler_advance(semu_scheduler *scheduler, uint64_t delta_ns,
                                   semu_error *error);
semu_status semu_scheduler_schedule(semu_scheduler *scheduler, uint64_t delay_ns,
                                    semu_event_callback callback, void *context,
                                    semu_event_id *event_id, semu_error *error);
int semu_scheduler_cancel(semu_scheduler *scheduler, semu_event_id event_id);
int semu_scheduler_has_events(const semu_scheduler *scheduler);
semu_status semu_scheduler_run_next(semu_scheduler *scheduler, semu_error *error);

#endif
