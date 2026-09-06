#ifndef SEMU_SCHEDULER_H
#define SEMU_SCHEDULER_H

#include "semu/types.h"

typedef struct semu_scheduler semu_scheduler;
typedef uint64_t semu_event_id;
typedef void (*semu_event_callback)(void *context, uint64_t now_ns);

#define SEMU_SCHEDULER_MAX_BATCH 64u
typedef struct semu_event_request {
    uint64_t delay_ns;
    uint32_t kind;
    uint32_t subject;
    semu_event_callback callback;
    void *context;
} semu_event_request;

/* Atomic admission of at most MAX_BATCH events. Requests and optional output
 * IDs are caller-owned, nonoverlapping arrays of count elements. No callbacks
 * run here. Refusal preserves queue/time/IDs/sequences and output IDs. Success
 * assigns IDs/sequences in request order, with the same deadline/FIFO behavior
 * as consecutive single admissions. Zero count is a no-op on a valid scheduler.
 * kind/subject are the existing snapshot event identity (kind 0: untagged).
 * May be called from a dispatch callback, like single-event scheduling. */
semu_status semu_scheduler_schedule_batch(semu_scheduler *scheduler,
    const semu_event_request *requests, size_t count, semu_event_id *event_ids,
    semu_error *error);

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
/* Cancel only when the ID still belongs to this callback/context. A scheduler
 * reset may recycle IDs; stale device bookkeeping must not cancel a new owner. */
int semu_scheduler_cancel_owned(semu_scheduler *scheduler, semu_event_id event_id,
    semu_event_callback callback, void *context);
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
