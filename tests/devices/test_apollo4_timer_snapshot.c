#include "../../src/core/scheduler_internal.h"
#include "../../src/soc/apollo4/timer.h"
#include "semu/scheduler.h"
#include "test.h"

static void test_inconsistent_event_refuses(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_apollo4_timer *timer;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;

    semu_error_clear(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    timer = semu_apollo4_timer_create(scheduler, NULL, NULL, &error);
    SEMU_TEST_ASSERT(context, timer != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x10u, 4u, 1u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x210u, 4u, 0x100u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x208u, 4u, 5u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x200u, 4u, 1u,
                                              &error));
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_count(scheduler));

    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_snapshot_write(timer, &writer, &error));
    /* Header is 29 bytes; channel-0 event ID follows 28 bytes of state. */
    for (unsigned index = 57u; index < 65u; ++index) writer.data[index] = 0u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_apollo4_timer_snapshot_read(timer, &reader, &error));
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_count(scheduler));

    semu_snapshot_writer_destroy(&writer);
    semu_apollo4_timer_destroy(timer);
    semu_scheduler_destroy(scheduler);
}

static void test_invalid_control_state_refuses(semu_test_context *context)
{
    static const size_t offsets[] = { 1u, 6u, 8u, 13u, 17u, 21u, 25u, 45u };
    static const uint8_t values[] = { 0x10u, 1u, 3u, 2u, 1u, 2u, 1u, 2u };
    semu_error error;
    semu_scheduler *source_scheduler;
    semu_scheduler *target_scheduler;
    semu_apollo4_timer *source;
    semu_apollo4_timer *target;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    size_t index;

    semu_error_clear(&error);
    source_scheduler = semu_scheduler_create(&error);
    target_scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, source_scheduler != NULL);
    SEMU_TEST_ASSERT(context, target_scheduler != NULL);
    source = semu_apollo4_timer_create(source_scheduler, NULL, NULL, &error);
    target = semu_apollo4_timer_create(target_scheduler, NULL, NULL, &error);
    SEMU_TEST_ASSERT(context, source != NULL);
    SEMU_TEST_ASSERT(context, target != NULL);
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_snapshot_write(source, &writer, &error));
    for (index = 0u; index < SEMU_ARRAY_LEN(offsets); ++index) {
        uint8_t original = writer.data[offsets[index]];
        writer.data[offsets[index]] = values[index];
        semu_snapshot_reader_init(&reader, writer.data, writer.size);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
            semu_apollo4_timer_snapshot_read(target, &reader, &error));
        writer.data[offsets[index]] = original;
    }
    semu_snapshot_writer_destroy(&writer);
    semu_apollo4_timer_destroy(target);
    semu_apollo4_timer_destroy(source);
    semu_scheduler_destroy(target_scheduler);
    semu_scheduler_destroy(source_scheduler);
}

static void test_event_requires_schedulable_channel(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *source_scheduler;
    semu_scheduler *target_scheduler;
    semu_apollo4_timer *source;
    semu_apollo4_timer *target;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    unsigned index;

    semu_error_clear(&error);
    source_scheduler = semu_scheduler_create(&error);
    target_scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, source_scheduler != NULL);
    SEMU_TEST_ASSERT(context, target_scheduler != NULL);
    source = semu_apollo4_timer_create(source_scheduler, NULL, NULL, &error);
    target = semu_apollo4_timer_create(target_scheduler, NULL, NULL, &error);
    SEMU_TEST_ASSERT(context, source != NULL);
    SEMU_TEST_ASSERT(context, target != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(source, 0x10u, 4u, 1u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(source, 0x210u, 4u, 0x100u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(source, 0x208u, 4u, 5u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(source, 0x200u, 4u, 1u,
                                              &error));
    SEMU_TEST_EQ_U64(context, 1u,
                     semu_scheduler_event_count(source_scheduler));

    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_snapshot_write(source, &writer, &error));
    /* Channel-0 control starts after the 29-byte global header. */
    for (index = 29u; index < 33u; ++index) writer.data[index] = 0u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_apollo4_timer_snapshot_read(target, &reader, &error));
    SEMU_TEST_EQ_U64(context, 0u,
                     semu_scheduler_event_count(target_scheduler));

    semu_snapshot_writer_destroy(&writer);
    semu_apollo4_timer_destroy(target);
    semu_apollo4_timer_destroy(source);
    semu_scheduler_destroy(target_scheduler);
    semu_scheduler_destroy(source_scheduler);
}

static void test_later_values_round_trip(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *source_scheduler;
    semu_scheduler *target_scheduler;
    semu_apollo4_timer *source;
    semu_apollo4_timer *target;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    uint32_t value = 0u;

    semu_error_clear(&error);
    source_scheduler = semu_scheduler_create(&error);
    target_scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, source_scheduler != NULL);
    SEMU_TEST_ASSERT(context, target_scheduler != NULL);
    source = semu_apollo4_timer_create(source_scheduler, NULL, NULL, &error);
    target = semu_apollo4_timer_create(target_scheduler, NULL, NULL, &error);
    SEMU_TEST_ASSERT(context, source != NULL);
    SEMU_TEST_ASSERT(context, target != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_write(source, 0xe8u, 4u, 0x3fu, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_write(source, 0x104u, 4u, 0x12300u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_write(source, 0x68u, 4u, 0x4000u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_write(source, 0x60u, 4u, 0x4001u, &error));

    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_snapshot_write(source, &writer, &error));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_snapshot_read(target, &reader, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_read(target, 0xe8u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x3fu, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_read(target, 0x104u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x12300u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_read(target, 0x68u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x4000u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_read(target, 0x60u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x4001u, value);

    semu_snapshot_writer_destroy(&writer);
    semu_apollo4_timer_destroy(target);
    semu_apollo4_timer_destroy(source);
    semu_scheduler_destroy(target_scheduler);
    semu_scheduler_destroy(source_scheduler);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_inconsistent_event_refuses),
        SEMU_TEST_CASE(test_invalid_control_state_refuses),
        SEMU_TEST_CASE(test_event_requires_schedulable_channel),
        SEMU_TEST_CASE(test_later_values_round_trip)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
