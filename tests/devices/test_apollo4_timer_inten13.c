#include "../../src/soc/apollo4/timer.h"
#include "test.h"
#include <string.h>

typedef struct irq_log {
    unsigned count;
    unsigned channel;
    int level;
} irq_log;

static void irq(void *context, unsigned channel, int level)
{
    irq_log *log = (irq_log *)context;
    ++log->count;
    log->channel = channel;
    log->level = level;
}

static void same_state(semu_test_context *context, semu_apollo4_timer *timer,
                       const semu_snapshot_writer *before)
{
    semu_error error;
    semu_snapshot_writer after;
    semu_snapshot_writer_init(&after);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_snapshot_write(timer, &after, &error));
    SEMU_TEST_EQ_U64(context, before->size, after.size);
    SEMU_TEST_ASSERT(context, memcmp(before->data, after.data, after.size) == 0);
    semu_snapshot_writer_destroy(&after);
}

static void combined_value_preserves_event_and_refuses(semu_test_context *context)
{
    static const uint32_t refused[] = {
        0x08004000u, 0x08004002u, 0x0c004001u, 0xffffffffu
    };
    semu_error error;
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    semu_apollo4_timer *timer;
    semu_snapshot_writer before;
    irq_log log = {0u, 0u, 0};
    uint32_t value = 0u;
    size_t index;
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    timer = semu_apollo4_timer_create(scheduler, irq, &log, &error);
    SEMU_TEST_ASSERT(context, timer != NULL);
    semu_apollo4_timer_reset(timer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_write(timer, 0x10u, 4u, 0x27ffu, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_write(timer, 0x3b0u, 4u, 0x100u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_write(timer, 0x3acu, 4u, 5u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_write(timer, 0x3a0u, 4u, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_write(timer, 0x60u, 4u, 0x4001u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_write(timer, 0x60u, 4u, 0x08004001u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_read(timer, 0x60u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x08004001u, value);
    semu_snapshot_writer_init(&before);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_snapshot_write(timer, &before, &error));
    for (index = 0u; index < SEMU_ARRAY_LEN(refused); ++index) {
        SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
            semu_apollo4_timer_write(timer, 0x60u, 4u, refused[index], &error));
        same_state(context, timer, &before);
    }
    for (index = 1u; index <= 2u; ++index) {
        SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
            semu_apollo4_timer_write(timer, 0x60u, (unsigned)index,
                                     0x08004001u, &error));
        same_state(context, timer, &before);
    }
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        semu_apollo4_timer_write(timer, 0x61u, 4u, 0x08004001u, &error));
    same_state(context, timer, &before);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        semu_apollo4_timer_write(timer, 0x6cu, 4u, 0x08004001u, &error));
    same_state(context, timer, &before);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_scheduler_advance(scheduler, 4u, &error));
    SEMU_TEST_EQ_U64(context, 0u, log.count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_scheduler_advance(scheduler, 1u, &error));
    SEMU_TEST_EQ_U64(context, 1u, log.count);
    SEMU_TEST_EQ_U64(context, 13u, log.channel);
    SEMU_TEST_EQ_U64(context, 1u, log.level);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_read(timer, 0x60u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x08004001u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_write(timer, 0x68u, 4u, 0x08000000u, &error));
    SEMU_TEST_EQ_U64(context, 2u, log.count);
    SEMU_TEST_EQ_U64(context, 0u, log.level);
    semu_apollo4_timer_reset(timer);
    SEMU_TEST_ASSERT(context, !semu_scheduler_has_events(scheduler));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_read(timer, 0x60u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    semu_snapshot_writer_destroy(&before);
    semu_apollo4_timer_destroy(timer);
    semu_scheduler_destroy(scheduler);
}

static void combined_snapshot_round_trip_and_refusal(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    semu_apollo4_timer *timer;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    uint32_t value = 0u;
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    timer = semu_apollo4_timer_create(scheduler, NULL, NULL, &error);
    SEMU_TEST_ASSERT(context, timer != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_write(timer, 0x60u, 4u, 0x4001u, &error));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_snapshot_write(timer, &writer, &error));
    /* Existing LE status_value at byte 8: add Timer13 CMP1 at bit 27.
     * Construct the import independently of the MMIO acceptance predicate. */
    writer.data[11u] = 8u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_snapshot_read(timer, &reader, &error));
    SEMU_TEST_ASSERT(context, semu_snapshot_reader_done(&reader));
    same_state(context, timer, &writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_timer_read(timer, 0x60u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x08004001u, value);
    writer.data[8u] = 2u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_apollo4_timer_snapshot_read(timer, &reader, &error));
    writer.data[8u] = 1u;
    same_state(context, timer, &writer);
    semu_snapshot_reader_init(&reader, writer.data, writer.size - 1u);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_apollo4_timer_snapshot_read(timer, &reader, &error));
    same_state(context, timer, &writer);
    semu_snapshot_writer_destroy(&writer);
    semu_apollo4_timer_destroy(timer);
    semu_scheduler_destroy(scheduler);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(combined_value_preserves_event_and_refuses),
        SEMU_TEST_CASE(combined_snapshot_round_trip_and_refusal)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
