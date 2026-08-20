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

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_inconsistent_event_refuses)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
