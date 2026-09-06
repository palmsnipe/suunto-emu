#include "scheduler_internal.h"
#include <stdlib.h>

/* E-EMU-NEMA-ATOMIC-001: reserve the complete operation before consuming any
 * event identity. The insertion loop cannot fail and invokes no callback. */
semu_status semu_scheduler_schedule_batch(semu_scheduler *scheduler,
    const semu_event_request *requests, size_t count, semu_event_id *event_ids,
    semu_error *error)
{
    size_t needed, capacity, i;
    semu_scheduled_event *replacement;
    if (scheduler == NULL || (count != 0u && requests == NULL)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "scheduler batch arguments required");
        return SEMU_ERR_ARGUMENT;
    }
    if (count > SEMU_SCHEDULER_MAX_BATCH) {
        semu_error_set(error, SEMU_ERR_RANGE, "scheduler batch exceeds bound");
        return SEMU_ERR_RANGE;
    }
    if (count == 0u) { semu_error_clear(error); return SEMU_OK; }
    if (scheduler->next_id == 0u || count > UINT64_MAX - scheduler->next_id ||
        count > UINT64_MAX - scheduler->next_sequence ||
        count > SIZE_MAX - scheduler->count) {
        semu_error_set(error, SEMU_ERR_RANGE, "scheduler time or id overflow");
        return SEMU_ERR_RANGE;
    }
    for (i = 0u; i < count; ++i) {
        if (requests[i].callback == NULL) {
            semu_error_set(error, SEMU_ERR_ARGUMENT, "scheduler and callback required");
            return SEMU_ERR_ARGUMENT;
        }
        if (requests[i].delay_ns > UINT64_MAX - scheduler->now_ns) {
            semu_error_set(error, SEMU_ERR_RANGE, "scheduler time or id overflow");
            return SEMU_ERR_RANGE;
        }
    }
    needed = scheduler->count + count;
    if (needed > scheduler->capacity) {
        capacity = scheduler->capacity == 0u ? 16u : scheduler->capacity;
        while (capacity < needed) {
            if (capacity > SIZE_MAX / 2u) { capacity = needed; break; }
            capacity *= 2u;
        }
        if (capacity > SIZE_MAX / sizeof(*replacement)) {
            semu_error_set(error, SEMU_ERR_NOMEM, "scheduler capacity overflow");
            return SEMU_ERR_NOMEM;
        }
        replacement = realloc(scheduler->events, capacity * sizeof(*replacement));
        if (replacement == NULL) {
            semu_error_set(error, SEMU_ERR_NOMEM, "cannot grow scheduler");
            return SEMU_ERR_NOMEM;
        }
        scheduler->events = replacement;
        scheduler->capacity = capacity;
    }
    for (i = 0u; i < count; ++i) {
        semu_scheduled_event event;
        size_t position = scheduler->count;
        event.state.due_ns = scheduler->now_ns + requests[i].delay_ns;
        event.state.sequence = scheduler->next_sequence++;
        event.state.id = scheduler->next_id++;
        event.state.kind = requests[i].kind;
        event.state.subject = requests[i].subject;
        event.callback = requests[i].callback;
        event.context = requests[i].context;
        while (position > 0u && scheduler->events[position - 1u].state.due_ns > event.state.due_ns) {
            scheduler->events[position] = scheduler->events[position - 1u];
            --position;
        }
        scheduler->events[position] = event;
        ++scheduler->count;
        if (event_ids != NULL) event_ids[i] = event.state.id;
    }
    semu_error_clear(error);
    return SEMU_OK;
}
