#include "test.h"
#include "../../src/devices/sapporo_cxd5610.h"
#include "../../src/compat/sapporo_222.h"
#include <stdio.h>
#include <string.h>

typedef struct fixture {
    semu_scheduler *scheduler;
    semu_sapporo_cxd5610 *gps;
    semu_transaction_result reply;
    unsigned tx, rx, signals, highs;
} fixture;

static void awake(void *context, unsigned line, int level)
{
    fixture *f = context;
    (void)line;
    ++f->signals;
    if (level) ++f->highs;
}

static void tail_event(void *context, uint64_t now)
{
    (void)now;
    ++*(unsigned *)context;
}

static void receive(void *context, uint8_t byte, uint64_t now)
{
    fixture *f = context;
    (void)byte; (void)now;
    ++f->rx;
}

static void trace(void *context, semu_sapporo_cxd5610_trace_direction dir,
                  uint8_t byte, uint64_t now)
{
    fixture *f = context;
    (void)byte; (void)now;
    if (dir == SEMU_SAPPORO_CXD5610_TX) ++f->tx;
}

static semu_transaction_result exchange(void *context, const uint8_t *bytes,
    size_t count, semu_sapporo_cxd5610 *gps, semu_error *error)
{
    fixture *f = context;
    (void)gps;
    if (count != 6u || memcmp(bytes, "@VER\r\n", 6u) != 0 ||
        f->reply != SEMU_TRANSACTION_OK) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "synthetic refusal/wait");
        return f->reply == SEMU_TRANSACTION_WAIT ? f->reply : SEMU_TRANSACTION_REFUSE;
    }
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

static void init(fixture *f, semu_error *error)
{
    memset(f, 0, sizeof(*f));
    f->scheduler = semu_scheduler_create(error);
    f->gps = semu_sapporo_cxd5610_create(f->scheduler, awake, f,
        receive, f, trace, f, error);
    semu_sapporo_cxd5610_set_exchange(f->gps, exchange, f);
}

static void destroy(fixture *f)
{
    semu_sapporo_cxd5610_destroy(f->gps);
    semu_scheduler_destroy(f->scheduler);
}

static semu_transaction_result send(fixture *f, const char *bytes,
                                    semu_error *error)
{
    semu_serial_endpoint ep = semu_sapporo_cxd5610_endpoint(f->gps);
    semu_serial_transaction tx = {0u, 0u, (const uint8_t *)bytes,
                                  strlen(bytes), NULL, 0u};
    return ep.transfer(ep.context, &tx, error);
}

static void unchanged(semu_test_context *context, fixture *f,
                      const semu_snapshot_writer *before)
{
    semu_snapshot_writer after;
    semu_error error;
    semu_snapshot_writer_init(&after);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_cxd5610_snapshot_write(f->gps, &after, &error));
    SEMU_TEST_EQ_U64(context, before->size, after.size);
    SEMU_TEST_ASSERT(context, before->size == after.size &&
        memcmp(before->data, after.data, before->size) == 0);
    semu_snapshot_writer_destroy(&after);
}

static void test_cxd5610_command_commit_after_acceptance(semu_test_context *context)
{
    fixture f;
    semu_error error;
    semu_snapshot_writer before;
    init(&f, &error);
    SEMU_TEST_ASSERT(context, f.gps != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, send(&f, "@VER\r", &error));
    semu_snapshot_writer_init(&before);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_cxd5610_snapshot_write(f.gps, &before, &error));
    f.reply = SEMU_TRANSACTION_REFUSE;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, send(&f, "\n", &error));
    unchanged(context, &f, &before);
    SEMU_TEST_EQ_U64(context, 5u, f.tx);
    f.reply = SEMU_TRANSACTION_WAIT;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_WAIT, send(&f, "\n", &error));
    unchanged(context, &f, &before);
    SEMU_TEST_EQ_U64(context, 5u, f.tx);
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(f.scheduler));
    f.reply = SEMU_TRANSACTION_OK;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, send(&f, "\n", &error));
    SEMU_TEST_EQ_U64(context, 6u, f.tx);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, send(&f, "@VER\r\n", &error));
    SEMU_TEST_EQ_U64(context, 12u, f.tx);
    semu_snapshot_writer_destroy(&before);
    destroy(&f);
}

static void test_cxd5610_rx_schedule_failure_atomic(semu_test_context *context)
{
    fixture f;
    semu_error error;
    semu_snapshot_writer before;
    unsigned mode;
    static const uint8_t bytes[] = {0x31u, 0x32u};
    init(&f, &error);
    SEMU_TEST_ASSERT(context, f.gps != NULL);
    for (mode = 0u; mode < 3u; ++mode) {
        semu_sapporo_cxd5610_reset(f.gps);
        semu_scheduler_reset(f.scheduler);
        /* Exhaust time, event ID, and sequence independently, without allocation. */
        if (mode == 0u) f.scheduler->now_ns = UINT64_MAX;
        if (mode == 1u) f.scheduler->next_id = UINT64_MAX;
        if (mode == 2u) f.scheduler->next_sequence = UINT64_MAX;
        semu_snapshot_writer_init(&before);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_snapshot_write(f.gps, &before, &error));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
            semu_sapporo_cxd5610_inject_rx_after(f.gps, bytes, 2u, 1u, &error));
        unchanged(context, &f, &before);
        SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(f.scheduler));
        SEMU_TEST_EQ_U64(context, 0u, f.rx);
        semu_snapshot_writer_destroy(&before);
    }
    semu_scheduler_reset(f.scheduler);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_cxd5610_inject_rx_after(f.gps, bytes, 2u, 10u, &error));
    semu_snapshot_writer_init(&before);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_cxd5610_snapshot_write(f.gps, &before, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        semu_sapporo_cxd5610_inject_rx_after(f.gps, bytes, 1u, 0u, &error));
    unchanged(context, &f, &before);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(f.scheduler, 9u, &error));
    SEMU_TEST_EQ_U64(context, 0u, f.rx);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(f.scheduler, 1u, &error));
    SEMU_TEST_EQ_U64(context, 2u, f.rx);
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(f.scheduler));
    semu_snapshot_writer_destroy(&before);
    destroy(&f);
}

static void test_cxd5610_legacy_provider_refusal_keeps_hit(semu_test_context *context)
{
    fixture f;
    semu_error error;
    semu_logger logger;
    semu_layer_state state;
    semu_sapporo_222_fixture_context provider;
    semu_snapshot_writer before;
    FILE *stream = tmpfile();
    init(&f, &error);
    SEMU_TEST_ASSERT(context, f.gps != NULL && stream != NULL);
    semu_log_init(&logger, stream, SEMU_LOG_DEBUG);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_layer_enable(&state,
        &semu_sapporo_222_no_device_layer, "sapporo-2.22.60", &error));
    provider = (semu_sapporo_222_fixture_context){&state, &logger, 0};
    semu_sapporo_cxd5610_set_exchange(f.gps, semu_sapporo_222_gps_exchange, &provider);
    f.scheduler->now_ns = UINT64_MAX;
    semu_snapshot_writer_init(&before);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_cxd5610_snapshot_write(f.gps, &before, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, send(&f, "@VER\r\n", &error));
    unchanged(context, &f, &before);
    SEMU_TEST_EQ_U64(context, 0u, state.hits);
    SEMU_TEST_EQ_U64(context, 0u, state.descriptor->interventions[SEMU_SAPPORO_222_IV_GPS_STARTUP].hits);
    SEMU_TEST_EQ_U64(context, 0u, ftell(stream));
    SEMU_TEST_EQ_U64(context, 0u, f.tx);
    semu_scheduler_reset(f.scheduler);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, send(&f, "@VER\r\n", &error));
    SEMU_TEST_EQ_U64(context, 1u, state.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_scheduler_advance(f.scheduler, UINT64_C(10000000), &error));
    SEMU_TEST_EQ_U64(context, 10u, f.rx);
    semu_snapshot_writer_destroy(&before);
    destroy(&f); (void)fclose(stream);
}

static void test_cxd5610_awake_admission_atomic(semu_test_context *context)
{
    for (unsigned mode = 0u; mode < 5u; ++mode) {
        fixture f;
        semu_error error;
        semu_snapshot_writer before;
        unsigned tail = 0u;
        init(&f, &error);
        SEMU_TEST_ASSERT(context, f.gps != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_scheduler_schedule(f.scheduler, 0u, tail_event, &tail, NULL, &error));
        if (mode == 0u) f.scheduler->now_ns = UINT64_MAX;
        if (mode == 1u) f.scheduler->next_id = UINT64_MAX;
        if (mode == 2u) f.scheduler->next_sequence = UINT64_MAX;
        if (mode == 3u) f.scheduler->now_ns = UINT64_MAX - 5u;
        if (mode == 4u) SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_pulse_awake_after(f.gps, 1u, &error));
        semu_scheduler scheduler_before;
        semu_scheduled_event queued;
        memcpy(&scheduler_before, f.scheduler, sizeof(scheduler_before));
        memcpy(&queued, &f.scheduler->events[0], sizeof(queued));
        unsigned signals = f.signals;
        semu_snapshot_writer_init(&before);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_snapshot_write(f.gps, &before, &error));
        SEMU_TEST_EQ_U64(context, mode == 4u ? SEMU_ERR_STATE : SEMU_ERR_RANGE,
            semu_sapporo_cxd5610_pulse_awake_after(f.gps, 1u, &error));
        SEMU_TEST_EQ_U64(context, mode == 4u ? SEMU_ERR_STATE : SEMU_ERR_RANGE,
            semu_sapporo_cxd5610_pulse_awake_after(f.gps, 1u, NULL));
        unchanged(context, &f, &before);
        SEMU_TEST_EQ_U64(context, signals, f.signals);
        SEMU_TEST_ASSERT(context, memcmp(&scheduler_before, f.scheduler,
                                        sizeof(scheduler_before)) == 0);
        SEMU_TEST_ASSERT(context, memcmp(&queued, &f.scheduler->events[0],
                                        sizeof(queued)) == 0);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
            semu_sapporo_cxd5610_pulse_awake_after(NULL, 1u, &error));
        unchanged(context, &f, &before);
        semu_snapshot_writer_destroy(&before);
        destroy(&f);
    }
}

static void test_cxd5610_awake_dispatch_failure(semu_test_context *context)
{
    for (unsigned mode = 0u; mode < 4u; ++mode) {
        fixture f;
        semu_error error;
        unsigned tail = 0u;
        init(&f, &error);
        SEMU_TEST_ASSERT(context, f.gps != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_pulse_awake_after(f.gps, 10u, &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_scheduler_schedule(f.scheduler, 10u, tail_event, &tail, NULL, &error));
        if ((mode & 1u) == 0u) f.scheduler->next_id = UINT64_MAX;
        else f.scheduler->next_sequence = UINT64_MAX;
        SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, mode < 2u ?
            semu_scheduler_run_next(f.scheduler, &error) :
            semu_scheduler_advance(f.scheduler, 100u, &error));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, error.code);
        SEMU_TEST_ASSERT(context, strcmp(error.text, "scheduler time or id overflow") == 0);
        SEMU_TEST_EQ_U64(context, 0u, f.highs);
        SEMU_TEST_EQ_U64(context, 2u, f.signals); /* create + accepted low */
        SEMU_TEST_EQ_U64(context, 0u, tail);
        SEMU_TEST_EQ_U64(context, 10u, semu_scheduler_now(f.scheduler));
        SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_count(f.scheduler));
        semu_sapporo_cxd5610_reset(f.gps);
        semu_scheduler_reset(f.scheduler);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_pulse_awake_after(f.gps, 10u, &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(f.scheduler,
            10u + SEMU_SAPPORO_CXD5610_AWAKE_PULSE_NS, &error));
        SEMU_TEST_EQ_U64(context, 1u, f.highs);
        SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(f.scheduler));
        destroy(&f);
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_cxd5610_command_commit_after_acceptance),
        SEMU_TEST_CASE(test_cxd5610_rx_schedule_failure_atomic),
        SEMU_TEST_CASE(test_cxd5610_legacy_provider_refusal_keeps_hit),
        SEMU_TEST_CASE(test_cxd5610_awake_admission_atomic),
        SEMU_TEST_CASE(test_cxd5610_awake_dispatch_failure)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
