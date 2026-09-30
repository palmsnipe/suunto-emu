#include "scheduler_internal.h"

#include <limits.h>
#include <stdlib.h>

static int event_before(const semu_scheduled_event *left,
                        const semu_scheduled_event *right)
{
    return left->state.due_ns < right->state.due_ns ||
           (left->state.due_ns == right->state.due_ns &&
            left->state.sequence < right->state.sequence);
}

static semu_status reserve_event(semu_scheduler *scheduler, semu_error *error)
{
    semu_scheduled_event *replacement;
    size_t capacity;

    if (scheduler->count < scheduler->capacity) {
        return SEMU_OK;
    }
    capacity = scheduler->capacity == 0u ? 16u : scheduler->capacity * 2u;
    if (capacity < scheduler->capacity ||
        capacity > SIZE_MAX / sizeof(*scheduler->events)) {
        semu_error_set(error, SEMU_ERR_NOMEM, "scheduler capacity overflow");
        return SEMU_ERR_NOMEM;
    }
    replacement = (semu_scheduled_event *)realloc(
        scheduler->events, capacity * sizeof(*scheduler->events));
    if (replacement == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot grow scheduler");
        return SEMU_ERR_NOMEM;
    }
    scheduler->events = replacement;
    scheduler->capacity = capacity;
    return SEMU_OK;
}

semu_scheduler *semu_scheduler_create(semu_error *error)
{
    semu_scheduler *scheduler = (semu_scheduler *)calloc(1u, sizeof(*scheduler));
    if (scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate scheduler");
        return NULL;
    }
    scheduler->next_id = 1u;
    semu_error_clear(error);
    return scheduler;
}

void semu_scheduler_destroy(semu_scheduler *scheduler)
{
    if (scheduler != NULL) {
        free(scheduler->events);
        free(scheduler);
    }
}

void semu_scheduler_reset(semu_scheduler *scheduler)
{
    if (scheduler != NULL) {
        scheduler->count = 0u;
        scheduler->now_ns = 0u;
        scheduler->next_sequence = 0u;
        scheduler->next_id = 1u;
    }
}

uint64_t semu_scheduler_now(const semu_scheduler *scheduler)
{
    return scheduler != NULL ? scheduler->now_ns : 0u;
}

semu_status semu_scheduler_schedule(semu_scheduler *scheduler, uint64_t delay_ns,
                                    semu_event_callback callback, void *context,
                                    semu_event_id *event_id, semu_error *error)
{
    return semu_scheduler_schedule_tagged(scheduler, delay_ns,
                                           SEMU_SCHED_EVENT_NONE, 0u,
                                           callback, context, event_id, error);
}

semu_status semu_scheduler_schedule_tagged(semu_scheduler *scheduler,
                                           uint64_t delay_ns, uint32_t kind,
                                           uint32_t subject,
                                           semu_event_callback callback,
                                           void *context,
                                           semu_event_id *event_id,
                                           semu_error *error)
{
    const semu_event_request request = {delay_ns, kind, subject, callback, context};
    return semu_scheduler_schedule_batch(scheduler, &request, 1u, event_id, error);
}

int semu_scheduler_cancel(semu_scheduler *scheduler, semu_event_id event_id)
{
    size_t index;

    if (scheduler == NULL || event_id == 0u) {
        return 0;
    }
    for (index = 0u; index < scheduler->count; ++index) {
        if (scheduler->events[index].state.id == event_id) {
            size_t tail = scheduler->count - index - 1u;
            if (tail != 0u) {
                size_t item;
                for (item = 0u; item < tail; ++item) {
                    scheduler->events[index + item] =
                        scheduler->events[index + item + 1u];
                }
            }
            --scheduler->count;
            return 1;
        }
    }
    return 0;
}

int semu_scheduler_cancel_owned(semu_scheduler *scheduler, semu_event_id event_id,
    semu_event_callback callback, void *context)
{
    size_t index;
    if (scheduler == NULL || event_id == 0u || callback == NULL) return 0;
    for (index = 0u; index < scheduler->count; ++index) {
        const semu_scheduled_event *event = &scheduler->events[index];
        if (event->state.id == event_id) {
            if (event->callback != callback || event->context != context) return 0;
            return semu_scheduler_cancel(scheduler, event_id);
        }
    }
    return 0;
}

int semu_scheduler_has_events(const semu_scheduler *scheduler)
{
    return scheduler != NULL && scheduler->count != 0u;
}

semu_status semu_scheduler_callback_fail(semu_scheduler *scheduler,
                                         const semu_error *failure)
{
    if (scheduler == NULL) return SEMU_ERR_ARGUMENT;
    if (!scheduler->dispatching) return SEMU_ERR_STATE;
    if (scheduler->callback_error.code == SEMU_OK) {
        if (failure == NULL || failure->code <= SEMU_OK ||
            failure->code > SEMU_ERR_NOMEM) {
            semu_error_set(&scheduler->callback_error, SEMU_ERR_ARGUMENT,
                           "scheduler callback failure requires an error");
        } else {
            scheduler->callback_error = *failure;
            scheduler->callback_error.text[sizeof(failure->text) - 1u] = '\0';
        }
    }
    return scheduler->callback_error.code;
}

static semu_status refuse_recursive_dispatch(semu_scheduler *scheduler,
                                             semu_error *error)
{
    semu_error failure;
    semu_error_set(&failure, SEMU_ERR_STATE, "recursive scheduler dispatch");
    (void)semu_scheduler_callback_fail(scheduler, &failure);
    if (error != NULL) *error = failure;
    return SEMU_ERR_STATE;
}

semu_status semu_scheduler_run_next(semu_scheduler *scheduler, semu_error *error)
{
    semu_scheduled_event event;
    size_t index;

    if (scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "scheduler required");
        return SEMU_ERR_ARGUMENT;
    }
    if (scheduler->dispatching) return refuse_recursive_dispatch(scheduler, error);
    if (scheduler->count == 0u) {
        semu_error_set(error, SEMU_ERR_STATE, "scheduler has no events");
        return SEMU_ERR_STATE;
    }
    event = scheduler->events[0];
    for (index = 1u; index < scheduler->count; ++index) {
        scheduler->events[index - 1u] = scheduler->events[index];
    }
    --scheduler->count;
    scheduler->now_ns = event.state.due_ns;
    semu_error_clear(&scheduler->callback_error);
    scheduler->dispatching = 1;
    event.callback(event.context, scheduler->now_ns);
    scheduler->dispatching = 0;
    semu_status status = scheduler->callback_error.code;
    if (error != NULL) *error = scheduler->callback_error;
    semu_error_clear(&scheduler->callback_error);
    return status;
}

semu_status semu_scheduler_advance(semu_scheduler *scheduler, uint64_t delta_ns,
                                   semu_error *error)
{
    uint64_t target;

    if (scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "scheduler required");
        return SEMU_ERR_ARGUMENT;
    }
    if (scheduler->dispatching) return refuse_recursive_dispatch(scheduler, error);
    if (UINT64_MAX - scheduler->now_ns < delta_ns) {
        semu_error_set(error, SEMU_ERR_RANGE, "scheduler time overflow");
        return SEMU_ERR_RANGE;
    }
    target = scheduler->now_ns + delta_ns;
    while (scheduler->count != 0u &&
           scheduler->events[0].state.due_ns <= target) {
        semu_status status = semu_scheduler_run_next(scheduler, error);
        if (status != SEMU_OK) {
            return status;
        }
    }
    scheduler->now_ns = target;
    semu_error_clear(error);
    return SEMU_OK;
}

size_t semu_scheduler_event_count(const semu_scheduler *scheduler)
{
    return scheduler != NULL ? scheduler->count : 0u;
}

const semu_scheduled_event_state *semu_scheduler_event_get(
    const semu_scheduler *scheduler, size_t index)
{
    if (scheduler == NULL || index >= scheduler->count) return NULL;
    return &scheduler->events[index].state;
}

semu_status semu_scheduler_restore_begin(semu_scheduler *scheduler,
                                         uint64_t now_ns,
                                         uint64_t next_sequence,
                                         semu_event_id next_id,
                                         semu_error *error)
{
    if (scheduler == NULL || next_id == 0u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "invalid scheduler snapshot state");
        return SEMU_ERR_ARGUMENT;
    }
    if (next_id == UINT64_MAX) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "scheduler snapshot id overflow");
        return SEMU_ERR_RANGE;
    }
    if (next_sequence == UINT64_MAX) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "scheduler snapshot sequence overflow");
        return SEMU_ERR_RANGE;
    }
    scheduler->count = 0u;
    scheduler->now_ns = now_ns;
    scheduler->next_sequence = next_sequence;
    scheduler->next_id = next_id;
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_scheduler_restore_event(semu_scheduler *scheduler,
    const semu_scheduled_event_state *state, semu_event_callback callback,
    void *context, semu_error *error)
{
    semu_scheduled_event event;
    size_t index;
    semu_status status;

    if (scheduler == NULL || state == NULL || callback == NULL ||
        state->id == 0u || state->sequence >= scheduler->next_sequence ||
        state->id >= scheduler->next_id || state->due_ns < scheduler->now_ns ||
        state->kind == SEMU_SCHED_EVENT_NONE ||
        state->kind > SEMU_SCHED_EVENT_SAP235_RTC_ALARM) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid scheduler snapshot event");
        return SEMU_ERR_FORMAT;
    }
    for (index = 0u; index < scheduler->count; ++index) {
        if (scheduler->events[index].state.id == state->id ||
            scheduler->events[index].state.sequence == state->sequence ||
            (scheduler->events[index].state.kind == state->kind &&
             scheduler->events[index].state.subject == state->subject)) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                           "duplicate scheduler snapshot event");
            return SEMU_ERR_FORMAT;
        }
    }
    status = reserve_event(scheduler, error);
    if (status != SEMU_OK) return status;
    event.state = *state;
    event.callback = callback;
    event.context = context;
    index = scheduler->count;
    while (index > 0u &&
           event_before(&event, &scheduler->events[index - 1u])) {
        scheduler->events[index] = scheduler->events[index - 1u];
        --index;
    }
    scheduler->events[index] = event;
    ++scheduler->count;
    semu_error_clear(error);
    return SEMU_OK;
}
