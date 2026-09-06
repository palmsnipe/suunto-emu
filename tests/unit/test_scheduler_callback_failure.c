#include "test.h"
#include "../../src/core/scheduler_internal.h"
#include <string.h>

typedef struct fixture {
    semu_scheduler *scheduler;
    semu_status reported, nested;
    unsigned calls, mode;
} fixture;

static void count_event(void *context, uint64_t now)
{
    fixture *f = context;
    (void)now;
    ++f->calls;
}

static void failing_event(void *context, uint64_t now)
{
    fixture *f = context;
    semu_error failure;
    (void)now;
    ++f->calls;
    semu_error_set(&failure, SEMU_ERR_NOMEM, "synthetic first callback failure");
    f->reported = semu_scheduler_callback_fail(f->scheduler, &failure);
    /* The caller's stack storage is not retained, and success cannot erase it. */
    memset(&failure, 0, sizeof(failure));
    if (f->mode == 1u) semu_scheduler_reset(f->scheduler);
    f->nested = semu_scheduler_callback_fail(f->scheduler, &failure);
    (void)semu_scheduler_schedule(f->scheduler, 0u, count_event, f, NULL, &failure);
    semu_error_set(&failure, SEMU_ERR_RANGE, "synthetic later failure");
    (void)semu_scheduler_callback_fail(f->scheduler, &failure);
}

static void test_scheduler_callback_failure_first_error_and_reuse(semu_test_context *context)
{
    for (unsigned mode = 0u; mode < 4u; ++mode) {
        semu_error error;
        fixture f = {0};
        f.scheduler = semu_scheduler_create(&error);
        SEMU_TEST_ASSERT(context, f.scheduler != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_schedule(
            f.scheduler, 10u, failing_event, &f, NULL, &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_schedule(
            f.scheduler, 10u, count_event, &f, NULL, &error));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_NOMEM, (mode & 1u) ?
            semu_scheduler_advance(f.scheduler, 100u, mode < 2u ? &error : NULL) :
            semu_scheduler_run_next(f.scheduler, mode < 2u ? &error : NULL));
        if (mode < 2u) {
            SEMU_TEST_EQ_U64(context, SEMU_ERR_NOMEM, error.code);
            SEMU_TEST_ASSERT(context, strcmp(error.text, "synthetic first callback failure") == 0);
        }
        SEMU_TEST_EQ_U64(context, SEMU_ERR_NOMEM, f.reported);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_NOMEM, f.nested);
        SEMU_TEST_EQ_U64(context, 1u, f.calls);
        SEMU_TEST_EQ_U64(context, 10u, semu_scheduler_now(f.scheduler));
        SEMU_TEST_EQ_U64(context, 2u, semu_scheduler_event_count(f.scheduler));
        SEMU_TEST_EQ_U64(context, 2u, semu_scheduler_event_get(f.scheduler, 0u)->id);
        SEMU_TEST_EQ_U64(context, 3u, semu_scheduler_event_get(f.scheduler, 1u)->id);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(f.scheduler, 0u, &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, error.code);
        SEMU_TEST_EQ_U64(context, 3u, f.calls);
        semu_scheduler_destroy(f.scheduler);
    }
}

static void test_scheduler_callback_failure_reset_retains_first_error(semu_test_context *context)
{
    semu_error error;
    fixture f = {0};
    f.mode = 1u;
    f.scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, f.scheduler != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_schedule(
        f.scheduler, 10u, failing_event, &f, NULL, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_NOMEM, semu_scheduler_advance(f.scheduler, 100u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_NOMEM, error.code);
    SEMU_TEST_ASSERT(context, strcmp(error.text, "synthetic first callback failure") == 0);
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_now(f.scheduler));
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_count(f.scheduler));
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_get(f.scheduler, 0u)->id);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_run_next(f.scheduler, &error));
    SEMU_TEST_EQ_U64(context, 2u, f.calls);
    semu_scheduler_reset(f.scheduler);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(f.scheduler, 1u, &error));
    semu_scheduler_destroy(f.scheduler);
}

static void recursive_event(void *context, uint64_t now)
{
    fixture *f = context;
    semu_error error;
    (void)now;
    ++f->calls;
    if (f->mode == 0u) f->nested = semu_scheduler_run_next(f->scheduler, &error);
    if (f->mode == 1u) f->nested = semu_scheduler_advance(f->scheduler, 0u, &error);
    if (f->mode == 2u) f->nested = semu_scheduler_advance_one(f->scheduler, NULL);
}

static void test_scheduler_callback_failure_reentrancy(semu_test_context *context)
{
    for (unsigned mode = 0u; mode < 3u; ++mode) {
        semu_error error;
        fixture f = {0};
        f.mode = mode;
        f.scheduler = semu_scheduler_create(&error);
        SEMU_TEST_ASSERT(context, f.scheduler != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_schedule(
            f.scheduler, 1u, recursive_event, &f, NULL, &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_schedule(
            f.scheduler, 2u, count_event, &f, NULL, &error));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, semu_scheduler_advance(f.scheduler, 10u, &error));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, f.nested);
        SEMU_TEST_ASSERT(context, strcmp(error.text, "recursive scheduler dispatch") == 0);
        SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_now(f.scheduler));
        SEMU_TEST_EQ_U64(context, 1u, f.calls);
        SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_count(f.scheduler));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_run_next(f.scheduler, &error));
        SEMU_TEST_EQ_U64(context, 2u, f.calls);
        semu_scheduler_destroy(f.scheduler);
    }
}

static void invalid_error_event(void *context, uint64_t now)
{
    fixture *f = context;
    semu_error failure;
    (void)now;
    memset(&failure, 'x', sizeof(failure));
    failure.code = f->mode == 0u ? SEMU_OK : f->mode == 1u ? (semu_status)99 : SEMU_ERR_IO;
    f->reported = semu_scheduler_callback_fail(f->scheduler, f->mode == 2u ? NULL : &failure);
}

static void test_scheduler_callback_failure_invalid_reports(semu_test_context *context)
{
    for (unsigned mode = 0u; mode < 4u; ++mode) {
        semu_error error;
        fixture f = {0};
        f.mode = mode;
        f.scheduler = semu_scheduler_create(&error);
        SEMU_TEST_ASSERT(context, f.scheduler != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT, semu_scheduler_callback_fail(NULL, NULL));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, semu_scheduler_callback_fail(f.scheduler, NULL));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_schedule(
            f.scheduler, 0u, invalid_error_event, &f, NULL, &error));
        SEMU_TEST_EQ_U64(context, mode == 3u ? SEMU_ERR_IO : SEMU_ERR_ARGUMENT,
            semu_scheduler_run_next(f.scheduler, &error));
        SEMU_TEST_EQ_U64(context, error.code, f.reported);
        if (mode == 3u) {
            SEMU_TEST_EQ_U64(context, sizeof(error.text) - 1u, strlen(error.text));
            SEMU_TEST_EQ_U64(context, 'x', error.text[0]);
        }
        semu_scheduler_destroy(f.scheduler);
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_scheduler_callback_failure_first_error_and_reuse),
        SEMU_TEST_CASE(test_scheduler_callback_failure_reset_retains_first_error),
        SEMU_TEST_CASE(test_scheduler_callback_failure_reentrancy),
        SEMU_TEST_CASE(test_scheduler_callback_failure_invalid_reports)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
