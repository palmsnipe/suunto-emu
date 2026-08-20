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
    for (unsigned index = 44u; index < 52u; ++index) writer.data[index] = 0u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_apollo4_stimer_snapshot_read(fixture.stimer, &reader, &error));
    SEMU_TEST_EQ_U64(context, 1u,
                     semu_scheduler_event_count(fixture.scheduler));

    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_inconsistent_event_refuses)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
