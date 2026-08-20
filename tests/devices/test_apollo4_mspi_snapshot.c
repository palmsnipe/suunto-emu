#include "test.h"

#include <stdint.h>
#include <string.h>

#include "../../src/soc/apollo4/mspi.h"

typedef struct mspi_fixture {
    semu_error error;
    semu_bus *bus;
    semu_apollo4_mspi *mspi;
} mspi_fixture;

static int fixture_init(mspi_fixture *fixture)
{
    semu_error_clear(&fixture->error);
    fixture->bus = semu_bus_create(&fixture->error);
    if (fixture->bus == NULL) return 0;
    fixture->mspi = semu_apollo4_mspi_create(
        fixture->bus, SEMU_APOLLO4_MSPI2_BASE, SEMU_APOLLO4_MSPI2_IRQ,
        NULL, NULL, NULL, NULL, &fixture->error);
    return fixture->mspi != NULL;
}

static void fixture_destroy(mspi_fixture *fixture)
{
    semu_apollo4_mspi_destroy(fixture->mspi);
    semu_bus_destroy(fixture->bus);
}

static void test_unreachable_state_refuses(semu_test_context *context)
{
    static const size_t offsets[] = { 5u, 9u, 8207u };
    static const uint8_t values[] = { 1u, 1u, 1u };
    mspi_fixture source;
    mspi_fixture target;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    size_t index;

    SEMU_TEST_ASSERT(context, fixture_init(&source));
    SEMU_TEST_ASSERT(context, fixture_init(&target));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_mspi_snapshot_write(source.mspi, &writer,
                                                      &source.error));
    for (index = 0u; index < sizeof(offsets) / sizeof(offsets[0]); ++index) {
        uint8_t original = writer.data[offsets[index]];
        writer.data[offsets[index]] = values[index];
        semu_snapshot_reader_init(&reader, writer.data, writer.size);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                         semu_apollo4_mspi_snapshot_read(target.mspi, &reader,
                                                          &target.error));
        writer.data[offsets[index]] = original;
    }
    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&target);
    fixture_destroy(&source);
}

static void test_round_trip(semu_test_context *context)
{
    mspi_fixture source;
    mspi_fixture target;
    semu_snapshot_writer writer;
    semu_snapshot_writer after;
    semu_snapshot_reader reader;

    SEMU_TEST_ASSERT(context, fixture_init(&source));
    SEMU_TEST_ASSERT(context, fixture_init(&target));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(source.bus,
                                    SEMU_APOLLO4_MSPI2_BASE +
                                        SEMU_APOLLO4_MSPI_INTEN,
                                    4u, 1u, &source.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(source.bus,
                                    SEMU_APOLLO4_MSPI2_BASE +
                                        SEMU_APOLLO4_MSPI_INTSET,
                                    4u, 1u, &source.error));
    semu_snapshot_writer_init(&writer);
    semu_snapshot_writer_init(&after);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_mspi_snapshot_write(source.mspi, &writer,
                                                      &source.error));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_mspi_snapshot_read(target.mspi, &reader,
                                                      &target.error));
    SEMU_TEST_ASSERT(context, semu_snapshot_reader_done(&reader));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_mspi_snapshot_write(target.mspi, &after,
                                                      &target.error));
    SEMU_TEST_EQ_U64(context, writer.size, after.size);
    SEMU_TEST_ASSERT(context, memcmp(writer.data, after.data, writer.size) == 0);
    semu_snapshot_writer_destroy(&after);
    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&target);
    fixture_destroy(&source);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_unreachable_state_refuses),
        SEMU_TEST_CASE(test_round_trip)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
