#include "../../src/core/scheduler_internal.h"
#include "test.h"
#include <stdlib.h>
#include <string.h>

/* Deterministic allocator failure in the production admission implementation. */
static void *refuse_realloc(void *pointer, size_t size)
{ (void)pointer; (void)size; return NULL; }
#define realloc refuse_realloc
#define semu_scheduler_schedule_batch fault_schedule_batch
#include "../../src/core/scheduler_batch.c"
#undef semu_scheduler_schedule_batch
#undef realloc

typedef struct { unsigned values[8]; unsigned count; } transcript;
typedef struct { transcript *trace; unsigned value; } event_context;
static void record(void *context, uint64_t now)
{
    event_context *event = context;
    (void)now;
    if (event->trace->count < SEMU_ARRAY_LEN(event->trace->values))
        event->trace->values[event->trace->count++] = event->value;
}

static void test_scheduler_batch_matches_single(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *batch = semu_scheduler_create(&error);
    semu_scheduler *single = semu_scheduler_create(&error);
    semu_event_id ids[4], singles[4];
    transcript bt = {{0}, 0}, st = {{0}, 0};
    event_context bc[4], sc[4];
    semu_event_request requests[4];
    const uint64_t delay[] = {9u, 3u, 9u, 1u};
    SEMU_TEST_ASSERT(context, batch != NULL && single != NULL);
    for (unsigned i = 0; i < 4u; ++i) {
        bc[i] = (event_context){&bt, i}; sc[i] = (event_context){&st, i};
        requests[i] = (semu_event_request){delay[i], SEMU_SCHED_EVENT_NEMA_COMPLETION,
            i, record, &bc[i]};
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_schedule_tagged(single,
            delay[i], SEMU_SCHED_EVENT_NEMA_COMPLETION, i, record, &sc[i], &singles[i], &error));
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_schedule_batch(batch, requests, 4u, ids, &error));
    SEMU_TEST_ASSERT(context, !memcmp(ids, singles, sizeof(ids)));
    for (size_t i = 0; i < 4u; ++i) {
        const semu_scheduled_event_state *a = semu_scheduler_event_get(batch, i);
        const semu_scheduled_event_state *b = semu_scheduler_event_get(single, i);
        SEMU_TEST_EQ_U64(context, a->id, b->id);
        SEMU_TEST_EQ_U64(context, a->sequence, b->sequence);
        SEMU_TEST_EQ_U64(context, a->due_ns, b->due_ns);
        SEMU_TEST_EQ_U64(context, a->subject, b->subject);
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(batch, 9u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(single, 9u, &error));
    SEMU_TEST_ASSERT(context, !memcmp(&bt, &st, sizeof(bt)));
    SEMU_TEST_EQ_U64(context, 3u, bt.values[0]);
    SEMU_TEST_EQ_U64(context, 1u, bt.values[1]);
    SEMU_TEST_EQ_U64(context, 0u, bt.values[2]);
    SEMU_TEST_EQ_U64(context, 2u, bt.values[3]);
    semu_scheduler_destroy(batch); semu_scheduler_destroy(single);
}

static void test_scheduler_batch_refusal_is_atomic(semu_test_context *context)
{
    for (unsigned mode = 0; mode < 6u; ++mode) {
        semu_error error;
        semu_scheduler *scheduler = semu_scheduler_create(&error);
        semu_event_id ids[2] = {55u, 66u};
        semu_event_request requests[2] = {{1u, 0u, 0u, record, NULL}, {2u, 0u, 0u, record, NULL}};
        semu_scheduler before;
        SEMU_TEST_ASSERT(context, scheduler != NULL);
        if (mode == 0u) scheduler->next_id = UINT64_MAX - 1u;
        if (mode == 1u) scheduler->next_sequence = UINT64_MAX - 1u;
        if (mode == 2u) scheduler->now_ns = UINT64_MAX - 1u;
        if (mode == 3u) requests[1].callback = NULL;
        before = *scheduler;
        SEMU_TEST_ASSERT(context, semu_scheduler_schedule_batch(scheduler,
            mode == 4u ? NULL : requests, mode == 5u ? SEMU_SCHEDULER_MAX_BATCH + 1u : 2u,
            ids, &error) != SEMU_OK);
        SEMU_TEST_ASSERT(context, !memcmp(&before, scheduler, sizeof(before)));
        SEMU_TEST_EQ_U64(context, 55u, ids[0]); SEMU_TEST_EQ_U64(context, 66u, ids[1]);
        SEMU_TEST_ASSERT(context, error.code != SEMU_OK && error.text[0] != '\0');
        semu_scheduler_destroy(scheduler);
    }
}

static void test_scheduler_batch_allocation_refusal(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    semu_event_request requests[17]; semu_event_id ids[17];
    semu_scheduler before; semu_scheduled_event event_before;
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_schedule(scheduler, 9u, record, NULL, NULL, &error));
    before = *scheduler; event_before = scheduler->events[0];
    for (unsigned i = 0; i < 17u; ++i) {
        requests[i] = (semu_event_request){1u, 0u, 0u, record, NULL}; ids[i] = 55u;
    }
    SEMU_TEST_EQ_U64(context, SEMU_ERR_NOMEM, fault_schedule_batch(scheduler, requests, 17u, ids, &error));
    SEMU_TEST_ASSERT(context, !memcmp(&before, scheduler, sizeof(before)));
    SEMU_TEST_ASSERT(context, !memcmp(&event_before, scheduler->events, sizeof(event_before)));
    for (unsigned i = 0; i < 17u; ++i) SEMU_TEST_EQ_U64(context, 55u, ids[i]);
    /* The injected allocator is not called when the full batch fits. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, fault_schedule_batch(scheduler, requests, 2u, ids, NULL));
    SEMU_TEST_EQ_U64(context, 2u, ids[0]); SEMU_TEST_EQ_U64(context, 3u, ids[1]);
    semu_scheduler_destroy(scheduler);
}

static void test_scheduler_batch_empty_reset_and_existing_fifo(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    transcript trace = {{0}, 0}; event_context contexts[3];
    semu_event_request requests[2]; semu_event_id ids[2] = {7u, 8u};
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT, semu_scheduler_schedule_batch(NULL, NULL, 0u, NULL, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_schedule_batch(scheduler, NULL, 0u, ids, &error));
    SEMU_TEST_EQ_U64(context, 7u, ids[0]);
    for (unsigned i = 0; i < 3u; ++i) contexts[i] = (event_context){&trace, i};
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_schedule(scheduler, 5u, record, contexts, NULL, &error));
    for (unsigned i = 0; i < 2u; ++i) requests[i] = (semu_event_request){5u, 0u, 0u, record, &contexts[i + 1u]};
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_schedule_batch(scheduler, requests, 2u, ids, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(scheduler, 5u, &error));
    for (unsigned i = 0; i < 3u; ++i) SEMU_TEST_EQ_U64(context, i, trace.values[i]);
    semu_scheduler_reset(scheduler);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_schedule_batch(scheduler, requests, 2u, ids, &error));
    SEMU_TEST_EQ_U64(context, 1u, ids[0]); SEMU_TEST_EQ_U64(context, 2u, ids[1]);
    semu_scheduler_destroy(scheduler);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_scheduler_batch_matches_single),
        SEMU_TEST_CASE(test_scheduler_batch_refusal_is_atomic),
        SEMU_TEST_CASE(test_scheduler_batch_allocation_refusal),
        SEMU_TEST_CASE(test_scheduler_batch_empty_reset_and_existing_fifo)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
