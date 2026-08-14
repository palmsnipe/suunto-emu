#include "semu/scheduler.h"

#include <limits.h>
#include <stdlib.h>

typedef struct scheduled_event {
    uint64_t due_ns;
    uint64_t sequence;
    semu_event_id id;
    semu_event_callback callback;
    void *context;
} scheduled_event;

struct semu_scheduler {
    scheduled_event *events;
    size_t count;
    size_t capacity;
    uint64_t now_ns;
    uint64_t next_sequence;
    semu_event_id next_id;
};

static int event_before(const scheduled_event *left, const scheduled_event *right)
{
    return left->due_ns < right->due_ns ||
           (left->due_ns == right->due_ns && left->sequence < right->sequence);
}

static semu_status reserve_event(semu_scheduler *scheduler, semu_error *error)
{
    scheduled_event *replacement;
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
    replacement = (scheduled_event *)realloc(
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
    scheduled_event event;
    size_t position;
    semu_status status;

    if (scheduler == NULL || callback == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "scheduler and callback required");
        return SEMU_ERR_ARGUMENT;
    }
    if (UINT64_MAX - scheduler->now_ns < delay_ns ||
        scheduler->next_id == 0u || scheduler->next_sequence == UINT64_MAX) {
        semu_error_set(error, SEMU_ERR_RANGE, "scheduler time or id overflow");
        return SEMU_ERR_RANGE;
    }
    status = reserve_event(scheduler, error);
    if (status != SEMU_OK) {
        return status;
    }
    event.due_ns = scheduler->now_ns + delay_ns;
    event.sequence = scheduler->next_sequence++;
    event.id = scheduler->next_id++;
    event.callback = callback;
    event.context = context;
    position = scheduler->count;
    while (position > 0u && event_before(&event, &scheduler->events[position - 1u])) {
        scheduler->events[position] = scheduler->events[position - 1u];
        --position;
    }
    scheduler->events[position] = event;
    ++scheduler->count;
    if (event_id != NULL) {
        *event_id = event.id;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

int semu_scheduler_cancel(semu_scheduler *scheduler, semu_event_id event_id)
{
    size_t index;

    if (scheduler == NULL || event_id == 0u) {
        return 0;
    }
    for (index = 0u; index < scheduler->count; ++index) {
        if (scheduler->events[index].id == event_id) {
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

int semu_scheduler_has_events(const semu_scheduler *scheduler)
{
    return scheduler != NULL && scheduler->count != 0u;
}

semu_status semu_scheduler_run_next(semu_scheduler *scheduler, semu_error *error)
{
    scheduled_event event;
    size_t index;

    if (scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "scheduler required");
        return SEMU_ERR_ARGUMENT;
    }
    if (scheduler->count == 0u) {
        semu_error_set(error, SEMU_ERR_STATE, "scheduler has no events");
        return SEMU_ERR_STATE;
    }
    event = scheduler->events[0];
    for (index = 1u; index < scheduler->count; ++index) {
        scheduler->events[index - 1u] = scheduler->events[index];
    }
    --scheduler->count;
    scheduler->now_ns = event.due_ns;
    event.callback(event.context, scheduler->now_ns);
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_scheduler_advance(semu_scheduler *scheduler, uint64_t delta_ns,
                                   semu_error *error)
{
    uint64_t target;

    if (scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "scheduler required");
        return SEMU_ERR_ARGUMENT;
    }
    if (UINT64_MAX - scheduler->now_ns < delta_ns) {
        semu_error_set(error, SEMU_ERR_RANGE, "scheduler time overflow");
        return SEMU_ERR_RANGE;
    }
    target = scheduler->now_ns + delta_ns;
    while (scheduler->count != 0u && scheduler->events[0].due_ns <= target) {
        semu_status status = semu_scheduler_run_next(scheduler, error);
        if (status != SEMU_OK) {
            return status;
        }
    }
    scheduler->now_ns = target;
    semu_error_clear(error);
    return SEMU_OK;
}
