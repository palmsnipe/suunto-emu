#include "../../src/display/nema_completion.h"
#include "test.h"
#include <string.h>

static void equal_snapshot(semu_test_context *context, nema_completion *completion,
    const semu_snapshot_writer *before)
{
    semu_error error; semu_snapshot_writer after;
    semu_snapshot_writer_init(&after);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_snapshot_write(completion, &after, &error));
    SEMU_TEST_EQ_U64(context, before->size, after.size);
    SEMU_TEST_ASSERT(context, !memcmp(before->data, after.data, before->size));
    semu_snapshot_writer_destroy(&after);
}

static void test_completion_batch_failure_preserves_queue(semu_test_context *context)
{
    for (unsigned mode = 0u; mode < 4u; ++mode) {
        semu_error error;
        semu_scheduler *scheduler = semu_scheduler_create(&error);
        nema_completion *completion = NULL; semu_snapshot_writer before;
        const uint32_t ids[] = {123u, 124u};
        uint64_t next_id, next_sequence; size_t queued;
        SEMU_TEST_ASSERT(context, scheduler != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_create(&completion, &error));
        for (unsigned i = 0u; i < (mode == 3u ? 63u : 1u); ++i)
            SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule(completion,
                scheduler, i, NULL, NULL, NULL, NULL, &error));
        if (mode == 0u) scheduler->next_id = UINT64_MAX - 1u;
        if (mode == 1u) scheduler->next_sequence = UINT64_MAX - 1u;
        if (mode == 2u) scheduler->now_ns = UINT64_MAX - NEMA_COMPLETION_DELAY_NS + 1u;
        next_id = scheduler->next_id; next_sequence = scheduler->next_sequence;
        queued = semu_scheduler_event_count(scheduler);
        semu_scheduled_event saved[64];
        memcpy(saved, scheduler->events, queued * sizeof(saved[0]));
        semu_snapshot_writer_init(&before);
        SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_snapshot_write(completion, &before, &error));
        SEMU_TEST_ASSERT(context, nema_completion_schedule_batch(completion, scheduler,
            ids, 2u, NULL, NULL, NULL, NULL, &error) != SEMU_OK);
        SEMU_TEST_ASSERT(context, error.code != SEMU_OK && error.text[0] != '\0');
        SEMU_TEST_EQ_U64(context, next_id, scheduler->next_id);
        SEMU_TEST_EQ_U64(context, next_sequence, scheduler->next_sequence);
        SEMU_TEST_EQ_U64(context, queued, semu_scheduler_event_count(scheduler));
        SEMU_TEST_ASSERT(context, !memcmp(saved, scheduler->events, queued * sizeof(saved[0])));
        equal_snapshot(context, completion, &before);
        semu_snapshot_writer_destroy(&before);
        nema_completion_destroy(completion); semu_scheduler_destroy(scheduler);
    }
}

typedef struct { uint32_t values[9]; size_t count; } capture;
static void register_write(void *context, uint32_t offset, uint32_t value)
{
    capture *result = context;
    if (result->count < 9u) result->values[result->count++] = offset == NEMA_REG_CLID ? value : 100u;
}
static void irq(void *context, unsigned line, int asserted)
{
    capture *result = context;
    if (result->count < 9u) result->values[result->count++] = asserted ? line : 0u;
}

static void unrelated_event(void *context, uint64_t now)
{ unsigned *calls = context; (void)now; ++*calls; }

static void test_completion_cancel_does_not_claim_reused_id(semu_test_context *context)
{
    semu_error error; unsigned calls = 0u;
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    nema_completion *completion = NULL; semu_event_id id;
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_create(&completion, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule(completion, scheduler,
        7u, NULL, NULL, NULL, NULL, &error));
    semu_scheduler_reset(scheduler);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_schedule(scheduler, 1u,
        unrelated_event, &calls, &id, &error));
    SEMU_TEST_EQ_U64(context, 1u, id);
    nema_completion_cancel(completion);
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_count(scheduler));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(scheduler, 1u, &error));
    SEMU_TEST_EQ_U64(context, 1u, calls);
    nema_completion_destroy(completion); semu_scheduler_destroy(scheduler);
}

static void test_completion_batch_success_matches_singles(semu_test_context *context)
{
    semu_error error; nema_completion *batch = NULL, *single = NULL;
    semu_scheduler *a = semu_scheduler_create(&error), *b = semu_scheduler_create(&error);
    const uint32_t ids[] = {7u, 8u, 7u, 9u}; capture ac = {{0}, 0}, bc = {{0}, 0};
    semu_snapshot_writer expected;
    SEMU_TEST_ASSERT(context, a != NULL && b != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_create(&batch, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_create(&single, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule_batch(batch, a, ids, 4u,
        register_write, &ac, irq, &ac, &error));
    for (unsigned i = 0u; i < 4u; ++i)
        SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule(single, b, ids[i],
            register_write, &bc, irq, &bc, &error));
    semu_snapshot_writer_init(&expected);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_snapshot_write(single, &expected, &error));
    equal_snapshot(context, batch, &expected);
    SEMU_TEST_EQ_U64(context, 3u, semu_scheduler_event_count(a));
    /* Repeated pending IDs and the empty batch consume no new identity. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule_batch(batch, a, ids, 4u,
        NULL, NULL, NULL, NULL, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule_batch(batch, a, NULL, 0u,
        NULL, NULL, NULL, NULL, NULL));
    equal_snapshot(context, batch, &expected);
    SEMU_TEST_EQ_U64(context, 4u, a->next_id);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(a, NEMA_COMPLETION_DELAY_NS, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(b, NEMA_COMPLETION_DELAY_NS, &error));
    SEMU_TEST_ASSERT(context, !memcmp(&ac, &bc, sizeof(ac)));
    for (unsigned i = 0u; i < 3u; ++i) {
        SEMU_TEST_EQ_U64(context, 7u + i, ac.values[3u * i]);
        SEMU_TEST_EQ_U64(context, 100u, ac.values[3u * i + 1u]);
        SEMU_TEST_EQ_U64(context, 28u, ac.values[3u * i + 2u]);
    }
    semu_snapshot_writer_destroy(&expected);
    nema_completion_destroy(batch); nema_completion_destroy(single);
    semu_scheduler_destroy(a); semu_scheduler_destroy(b);
}

static void test_completion_cancel_reset_destroy_and_ownership(semu_test_context *context)
{
    semu_error error; nema_completion *completion = NULL;
    semu_scheduler *a = semu_scheduler_create(&error), *b = semu_scheduler_create(&error);
    const uint32_t ids[] = {7u, 8u}; semu_snapshot_writer before;
    SEMU_TEST_ASSERT(context, a != NULL && b != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_create(&completion, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule_batch(completion, a, ids, 2u,
        NULL, NULL, NULL, NULL, &error));
    semu_snapshot_writer_init(&before);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_snapshot_write(completion, &before, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT, nema_completion_schedule_batch(completion, b, ids, 2u,
        NULL, NULL, NULL, NULL, &error));
    equal_snapshot(context, completion, &before);
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(b));
    nema_completion_cancel(completion);
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(a));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule_batch(completion, a, ids, 2u,
        NULL, NULL, NULL, NULL, &error));
    nema_completion_reset(completion);
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(a));
    SEMU_TEST_EQ_U64(context, 0u, nema_completion_count(completion));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule_batch(completion, b, ids, 2u,
        NULL, NULL, NULL, NULL, &error));
    nema_completion_destroy(completion);
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(b));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(a, NEMA_COMPLETION_DELAY_NS, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(b, NEMA_COMPLETION_DELAY_NS, &error));
    semu_snapshot_writer_destroy(&before); semu_scheduler_destroy(a); semu_scheduler_destroy(b);
}

static void test_completion_failed_admission_preserves_snapshot(semu_test_context *context)
{
    for (unsigned mode = 0u; mode < 3u; ++mode) {
        semu_error error;
        semu_scheduler *scheduler = semu_scheduler_create(&error);
        nema_completion *completion = NULL;
        semu_snapshot_writer before, after;
        SEMU_TEST_ASSERT(context, scheduler != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_create(&completion, &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_restore_begin(scheduler,
            mode == 0u ? UINT64_MAX : 0u, 0u, 1u, &error));
        if (mode == 1u) scheduler->next_id = UINT64_MAX;
        if (mode == 2u) scheduler->next_sequence = UINT64_MAX;
        semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
        SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_snapshot_write(completion, &before, &error));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, nema_completion_schedule(completion,
            scheduler, 123u, NULL, NULL, NULL, NULL, &error));
        SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(scheduler));
        SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_snapshot_write(completion, &after, &error));
        SEMU_TEST_EQ_U64(context, before.size, after.size);
        SEMU_TEST_ASSERT(context, memcmp(before.data, after.data, before.size) == 0);
        semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
        nema_completion_destroy(completion); semu_scheduler_destroy(scheduler);
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_completion_failed_admission_preserves_snapshot),
        SEMU_TEST_CASE(test_completion_batch_failure_preserves_queue),
        SEMU_TEST_CASE(test_completion_batch_success_matches_singles),
        SEMU_TEST_CASE(test_completion_cancel_does_not_claim_reused_id),
        SEMU_TEST_CASE(test_completion_cancel_reset_destroy_and_ownership)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
