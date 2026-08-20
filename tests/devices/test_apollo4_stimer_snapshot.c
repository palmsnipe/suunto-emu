#include "../../src/core/scheduler_internal.h"
#include "../../src/soc/apollo4/stimer.h"
#include "semu/scheduler.h"
#include "test.h"

typedef struct stimer_snapshot_fixture {
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4_stimer *stimer;
} stimer_snapshot_fixture;

static int fixture_init(stimer_snapshot_fixture *fixture,
                        semu_error *error)
{
    fixture->bus = semu_bus_create(error);
    fixture->scheduler = semu_scheduler_create(error);
    if (fixture->bus == NULL || fixture->scheduler == NULL) return 0;
    fixture->stimer = semu_apollo4_stimer_create(
        fixture->bus, fixture->scheduler, NULL, NULL, error);
    return fixture->stimer != NULL;
}

static void fixture_destroy(stimer_snapshot_fixture *fixture)
{
    semu_apollo4_stimer_destroy(fixture->stimer);
    semu_scheduler_destroy(fixture->scheduler);
    semu_bus_destroy(fixture->bus);
}

static void test_inconsistent_event_refuses(semu_test_context *context)
{
    semu_error error;
    stimer_snapshot_fixture fixture = { 0 };
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    uint8_t event_bytes[8];

    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, fixture_init(&fixture, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_stimer_write(fixture.stimer, 0x00u, 4u,
                                               0x503u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_stimer_write(fixture.stimer, 0x100u, 4u,
                                               0x100u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_stimer_write(fixture.stimer, 0x20u, 4u,
                                               5u, &error));
    SEMU_TEST_EQ_U64(context, 1u,
                     semu_scheduler_event_count(fixture.scheduler));

    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_stimer_snapshot_write(fixture.stimer, &writer, &error));
    /* Header is 40 bytes; compare-A event ID occupies bytes 44..51. */
    for (unsigned index = 44u; index < 52u; ++index) {
        event_bytes[index - 44u] = writer.data[index];
        writer.data[index] = 0u;
    }
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_apollo4_stimer_snapshot_read(fixture.stimer, &reader, &error));
    SEMU_TEST_EQ_U64(context, 1u,
                     semu_scheduler_event_count(fixture.scheduler));

    for (unsigned index = 44u; index < 52u; ++index)
        writer.data[index] = event_bytes[index - 44u];
    writer.data[13u] = 0u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_apollo4_stimer_snapshot_read(fixture.stimer, &reader, &error));

    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&fixture);
}

static void test_invalid_control_state_refuses(semu_test_context *context)
{
    static const size_t offsets[] = { 12u, 16u, 20u };
    static const uint8_t values[] = { 4u, 2u, 8u };
    semu_error error;
    stimer_snapshot_fixture source = { 0 };
    stimer_snapshot_fixture target = { 0 };
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    size_t index;

    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, fixture_init(&source, &error));
    SEMU_TEST_ASSERT(context, fixture_init(&target, &error));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_stimer_snapshot_write(source.stimer, &writer, &error));
    for (index = 0u; index < SEMU_ARRAY_LEN(offsets); ++index) {
        uint8_t original = writer.data[offsets[index]];
        writer.data[offsets[index]] = values[index];
        semu_snapshot_reader_init(&reader, writer.data, writer.size);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
            semu_apollo4_stimer_snapshot_read(target.stimer, &reader, &error));
        writer.data[offsets[index]] = original;
    }
    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&target);
    fixture_destroy(&source);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_inconsistent_event_refuses),
        SEMU_TEST_CASE(test_invalid_control_state_refuses)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
