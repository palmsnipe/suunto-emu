#include "../../src/core/scheduler_internal.h"
#include "../../src/devices/sapporo_cxd5610.h"
#include "semu/scheduler.h"
#include "test.h"
#include <string.h>

static void sink(void *context, uint8_t value, uint64_t now_ns)
{
    (void)context;
    (void)value;
    (void)now_ns;
}

static semu_sapporo_cxd5610 *make_transport(semu_scheduler **scheduler,
                                            semu_error *error)
{
    *scheduler = semu_scheduler_create(error);
    if (*scheduler == NULL) return NULL;
    return semu_sapporo_cxd5610_create(*scheduler, NULL, NULL, sink, NULL,
                                       NULL, NULL, error);
}

static void put_u64le(uint8_t *data, uint64_t value)
{
    for (unsigned index = 0u; index < 8u; ++index)
        data[index] = (uint8_t)(value >> (index * 8u));
}

static void test_pending_rx_round_trip_and_refusals(
    semu_test_context *context)
{
    static const uint8_t byte = 0x42u;
    semu_error error;
    semu_error target_error;
    semu_scheduler *scheduler;
    semu_scheduler *target_scheduler;
    semu_sapporo_cxd5610 *transport = make_transport(&scheduler, &error);
    semu_sapporo_cxd5610 *target = make_transport(&target_scheduler,
                                                   &target_error);
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_event_callback callback = NULL;
    void *event_context = NULL;

    SEMU_TEST_ASSERT(context, transport != NULL && target != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_cxd5610_inject_rx_after(transport, &byte, 1u, 100u,
                                             &error));
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_count(scheduler));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_cxd5610_snapshot_write(transport, &writer, &error));

    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_cxd5610_snapshot_read(target, &reader, &target_error));
    SEMU_TEST_ASSERT(context, semu_snapshot_reader_done(&reader));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_cxd5610_snapshot_resolve_event(
            target, 0u, &callback, &event_context, &target_error));
    SEMU_TEST_ASSERT(context, callback != NULL && event_context != NULL);

    /* pending=0, rx_count=1, rx byte=1, rx event starts at byte 9. */
    put_u64le(writer.data + 9u, 0u);
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_sapporo_cxd5610_snapshot_read(transport, &reader, &error));
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_count(scheduler));

    /* Restore the event ID and corrupt its generation instead. */
    put_u64le(writer.data + 9u, 1u);
    put_u64le(writer.data + 18u, 0u);
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_sapporo_cxd5610_snapshot_read(transport, &reader, &error));
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_count(scheduler));

    semu_snapshot_writer_destroy(&writer);
    semu_sapporo_cxd5610_destroy(target);
    semu_scheduler_destroy(target_scheduler);
    semu_sapporo_cxd5610_destroy(transport);
    semu_scheduler_destroy(scheduler);
}

static void test_pending_awake_round_trip_and_links(semu_test_context *context)
{
    for (unsigned subject = 2u; subject <= 3u; ++subject) {
        semu_error error;
        semu_scheduler *scheduler, *target_scheduler;
        semu_sapporo_cxd5610 *transport = make_transport(&scheduler, &error);
        semu_sapporo_cxd5610 *target = make_transport(&target_scheduler, &error);
        semu_snapshot_writer writer, after;
        semu_snapshot_reader reader;
        semu_event_callback callback;
        void *event_context;
        SEMU_TEST_ASSERT(context, transport != NULL && target != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_pulse_awake_after(transport, 100u, &error));
        if (subject == 3u) SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_scheduler_advance(scheduler, 100u, &error));
        semu_scheduled_event_state event = *semu_scheduler_event_get(scheduler, 0u);
        SEMU_TEST_EQ_U64(context, subject, event.subject);
        SEMU_TEST_EQ_U64(context, subject - 1u, event.id);
        SEMU_TEST_EQ_U64(context, subject - 2u, event.sequence);
        semu_snapshot_writer_init(&writer);
        semu_snapshot_writer_init(&after);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_snapshot_write(transport, &writer, &error));
        semu_snapshot_reader_init(&reader, writer.data, writer.size);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_snapshot_read(target, &reader, &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_snapshot_resolve_event(target, subject,
                &callback, &event_context, &error));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
            semu_sapporo_cxd5610_snapshot_resolve_event(target, 5u - subject,
                &callback, &event_context, &error));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
            semu_sapporo_cxd5610_snapshot_event_links_match(target, NULL, 0u, &error));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
            semu_sapporo_cxd5610_snapshot_event_id_matches(target, subject, event.id + 1u, &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_snapshot_event_links_match(target, &event, 1u, &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_restore_begin(target_scheduler,
            scheduler->now_ns, scheduler->next_sequence, scheduler->next_id, &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_restore_event(target_scheduler,
            &event, callback, event_context, &error));
        /* Empty buffers: stage is byte 42. Refusal must preserve the target. */
        uint8_t stage = writer.data[42u];
        writer.data[42u] = 0u;
        semu_snapshot_reader_init(&reader, writer.data, writer.size);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
            semu_sapporo_cxd5610_snapshot_read(target, &reader, &error));
        writer.data[42u] = stage;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_snapshot_write(target, &after, &error));
        SEMU_TEST_ASSERT(context, writer.size == after.size &&
            memcmp(writer.data, after.data, writer.size) == 0);
        semu_snapshot_writer_destroy(&writer);
        semu_snapshot_writer_destroy(&after);
        uint64_t delta = 100u + SEMU_SAPPORO_CXD5610_AWAKE_PULSE_NS - scheduler->now_ns;
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(scheduler, delta, &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(target_scheduler, delta, &error));
        SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(target_scheduler));
        SEMU_TEST_EQ_U64(context, scheduler->next_id, target_scheduler->next_id);
        SEMU_TEST_EQ_U64(context, scheduler->next_sequence, target_scheduler->next_sequence);
        semu_snapshot_writer_init(&writer);
        semu_snapshot_writer_init(&after);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_snapshot_write(transport, &writer, &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_snapshot_write(target, &after, &error));
        SEMU_TEST_ASSERT(context, writer.size == after.size &&
            memcmp(writer.data, after.data, writer.size) == 0);
        semu_snapshot_writer_destroy(&writer);
        semu_snapshot_writer_destroy(&after);
        semu_sapporo_cxd5610_destroy(target);
        semu_sapporo_cxd5610_destroy(transport);
        semu_scheduler_destroy(target_scheduler);
        semu_scheduler_destroy(scheduler);
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_pending_rx_round_trip_and_refusals),
        SEMU_TEST_CASE(test_pending_awake_round_trip_and_links)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
