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

/* Report a failure from the currently dispatched callback. Copies the first
 * non-OK error; returns that code even if later reports differ. NULL/OK/invalid
 * errors report ERR_ARGUMENT. Outside dispatch returns ERR_STATE (NULL
 * scheduler: ERR_ARGUMENT). run_next/advance return the copied failure and
 * leave remaining events queued at the failing deadline. No implicit rollback.
 * Recursive dispatch/advance refuses and reports ERR_STATE to the outer call.
 * Callbacks may schedule/cancel/reset; reset does not erase an active failure.
 * The report is transient, cleared after dispatch, and is not snapshot state.
 * Destroying the scheduler inside its callback is not supported. */
semu_status semu_scheduler_callback_fail(semu_scheduler *scheduler,
                                         const semu_error *failure);

#endif
