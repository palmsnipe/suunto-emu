/*
 * Bounded trace contract (ticket 600).
 * Allocation, append, bounded retrieval, and overflow policies.
 */

#include "semu/trace.h"

#include <stdlib.h>
#include <string.h>

struct semu_trace {
    semu_trace_record *records;
    uint32_t capacity;
    uint32_t head;
    uint32_t count;
    uint64_t next_sequence;
    int overflowed;
    semu_trace_overflow_policy policy;
};

semu_trace *semu_trace_create(uint32_t capacity,
    semu_trace_overflow_policy policy, semu_error *error)
{
    semu_trace *trace;
    if (capacity == 0u || capacity > SEMU_TRACE_MAX_RECORDS) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "trace: capacity %u out of range [1,%u]",
                       capacity, SEMU_TRACE_MAX_RECORDS);
        return NULL;
    }
    if (policy != SEMU_TRACE_OVERFLOW_STOP &&
        policy != SEMU_TRACE_OVERFLOW_TRUNCATE) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "trace: invalid overflow policy");
        return NULL;
    }
    trace = (semu_trace *)calloc(1u, sizeof(*trace));
    if (trace == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "trace: cannot allocate");
        return NULL;
    }
    trace->records = (semu_trace_record *)calloc(capacity,
                                                  sizeof(*trace->records));
    if (trace->records == NULL) {
        free(trace);
        semu_error_set(error, SEMU_ERR_NOMEM, "trace: cannot allocate records");
        return NULL;
    }
    trace->capacity = capacity;
    trace->policy = policy;
    trace->head = 0u;
    trace->count = 0u;
    trace->next_sequence = 0u;
    trace->overflowed = 0;
    return trace;
}

void semu_trace_destroy(semu_trace *trace)
{
    if (trace != NULL) {
        free(trace->records);
        free(trace);
    }
}

void semu_trace_reset(semu_trace *trace)
{
    if (trace == NULL) {
        return;
    }
    memset(trace->records, 0,
           (size_t)trace->capacity * sizeof(*trace->records));
    trace->head = 0u;
    trace->count = 0u;
    trace->next_sequence = 0u;
    trace->overflowed = 0;
}

semu_status semu_trace_append(semu_trace *trace,
    const semu_trace_record *record, semu_error *error)
{
    uint32_t idx;
    if (trace == NULL || record == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "trace: null argument");
        return SEMU_ERR_ARGUMENT;
    }
    if (trace->count >= trace->capacity) {
        if (trace->policy == SEMU_TRACE_OVERFLOW_STOP) {
            semu_error_set(error, SEMU_ERR_RANGE,
                           "trace: capacity %u exceeded (stop policy)",
                           trace->capacity);
            return SEMU_ERR_RANGE;
        }
        trace->overflowed = 1;
        idx = trace->head;
        trace->head = (trace->head + 1u) % trace->capacity;
    } else {
        idx = (trace->head + trace->count) % trace->capacity;
        ++trace->count;
    }
    trace->records[idx] = *record;
    trace->records[idx].schema_version = SEMU_TRACE_SCHEMA_VERSION;
    trace->records[idx].sequence = trace->next_sequence++;
    return SEMU_OK;
}

size_t semu_trace_count(const semu_trace *trace)
{
    return trace != NULL ? (size_t)trace->count : 0u;
}

int semu_trace_overflowed(const semu_trace *trace)
{
    return trace != NULL ? trace->overflowed : 0;
}

const semu_trace_record *semu_trace_get(const semu_trace *trace,
    size_t index)
{
    if (trace == NULL || index >= (size_t)trace->count) {
        return NULL;
    }
    return &trace->records[(trace->head + index) % trace->capacity];
}
