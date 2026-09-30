#include "test.h"

#include "../../src/soc/apollo4/apollo4_internal.h"

#include <string.h>

typedef struct apollo4_fixture {
    semu_error error;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4 *soc;
} apollo4_fixture;

static int fixture_init(apollo4_fixture *fixture)
{
    semu_error_clear(&fixture->error);
    fixture->bus = semu_bus_create(&fixture->error);
    fixture->scheduler = semu_scheduler_create(&fixture->error);
    if (fixture->bus == NULL || fixture->scheduler == NULL) return 0;
    fixture->soc = semu_apollo4_create(fixture->bus, &fixture->error);
    if (fixture->soc == NULL) return 0;
    return semu_apollo4_init(fixture->soc, fixture->scheduler, NULL, NULL,
                             &fixture->error) == SEMU_OK;
}

static void fixture_destroy(apollo4_fixture *fixture)
{
    semu_apollo4_destroy(fixture->soc);
    semu_scheduler_destroy(fixture->scheduler);
    semu_bus_destroy(fixture->bus);
}

static void test_invalid_gpio_level_refuses(semu_test_context *context)
{
    apollo4_fixture source;
    apollo4_fixture target;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;

    SEMU_TEST_ASSERT(context, fixture_init(&source));
    SEMU_TEST_ASSERT(context, fixture_init(&target));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_snapshot_write(source.soc, &writer,
                                                  &source.error));
    writer.data[0u] = 2u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_apollo4_snapshot_read(target.soc, &reader,
                                                &target.error));
    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&target);
    fixture_destroy(&source);
}

static void test_late_child_refusal_is_atomic(semu_test_context *context)
{
    apollo4_fixture source;
    apollo4_fixture target;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;

    SEMU_TEST_ASSERT(context, fixture_init(&source));
    SEMU_TEST_ASSERT(context, fixture_init(&target));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_set_gpio_input(source.soc, 0u, 0,
                                                 &source.error));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_snapshot_write(source.soc, &writer,
                                                  &source.error));
    writer.data[writer.size - 4u] = 1u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_apollo4_snapshot_read(target.soc, &reader,
                                                &target.error));
    SEMU_TEST_EQ_U64(context, 1u,
                     semu_apollo4_get_gpio_input(target.soc, 0u));
    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&target);
    fixture_destroy(&source);
}

/* Ticket 792 retired the E-EMU-SAP235-SNAPSHOT-001 guard: the live
 * RTC/IOM4 state is now codec-covered. The remaining fail-closed seam
 * is profile shape: a live image never applies to a stub SoC (trailing
 * data) and a stub image never feeds a live SoC (missing bytes). */
static void test_live235_snapshot_profile_shape_refuses(semu_test_context *context)
{
    apollo4_fixture live;
    apollo4_fixture stub;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    uint32_t fifo_word = 0u;
    SEMU_TEST_ASSERT(context, fixture_init(&live));
    SEMU_TEST_ASSERT(context, fixture_init(&stub));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_select_profile(live.soc, "sapporo-2.35.34",
                                    &live.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(live.bus, 0x40054000u, 4u, 0x12345678u,
                        &live.error));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_snapshot_write(live.soc, &writer, &live.error));
    /* Stub machine consuming the live image: shared bytes apply, the
     * live tail stays unconsumed and is therefore detectable. */
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_snapshot_read(stub.soc, &reader, &stub.error));
    SEMU_TEST_ASSERT(context, !semu_snapshot_reader_done(&reader));
    /* Live machine consuming the stub image: the live-module bytes are
     * missing, so the read refuses without touching the live state. */
    semu_snapshot_writer_destroy(&writer);
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_snapshot_write(stub.soc, &writer, &stub.error));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_apollo4_snapshot_read(live.soc, &reader, &live.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_read(live.bus, 0x40054000u, 4u, &fifo_word,
                       &live.error));
    SEMU_TEST_EQ_U64(context, 0x12345678u, fifo_word);
    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&stub);
    fixture_destroy(&live);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_invalid_gpio_level_refuses),
        SEMU_TEST_CASE(test_late_child_refusal_is_atomic),
        SEMU_TEST_CASE(test_live235_snapshot_profile_shape_refuses)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
