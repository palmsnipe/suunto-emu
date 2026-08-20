#include "test.h"

#include <stdint.h>
#include <string.h>

#include "../../src/soc/apollo4/mspi.h"

typedef struct mspi_fixture {
    semu_error error;
    semu_bus *bus;
    semu_apollo4_mspi *mspi;
} mspi_fixture;

static int fixture_init_base(mspi_fixture *fixture, uint32_t base,
                             unsigned irq)
{
    semu_error_clear(&fixture->error);
    fixture->bus = semu_bus_create(&fixture->error);
    if (fixture->bus == NULL) return 0;
    fixture->mspi = semu_apollo4_mspi_create(
        fixture->bus, base, irq, NULL, NULL, NULL, NULL, &fixture->error);
    return fixture->mspi != NULL;
}

static int fixture_init(mspi_fixture *fixture)
{
    return fixture_init_base(fixture, SEMU_APOLLO4_MSPI2_BASE,
                             SEMU_APOLLO4_MSPI2_IRQ);
}

static void fixture_destroy(mspi_fixture *fixture)
{
    semu_apollo4_mspi_destroy(fixture->mspi);
    semu_bus_destroy(fixture->bus);
}

static void test_unreachable_state_refuses(semu_test_context *context)
{
    static const size_t offsets[] = { 5u, 9u, 10u, 266u, 270u, 8207u };
    static const uint8_t values[] = { 1u, 1u, 1u, 1u, 1u, 1u };
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

static void test_mspi1_queue_count_refuses(semu_test_context *context)
{
    mspi_fixture source;
    mspi_fixture target;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    size_t offset = 10u + SEMU_APOLLO4_MSPI1_QUEUE_COUNT;
    uint8_t original;

    SEMU_TEST_ASSERT(context,
                     fixture_init_base(&source, SEMU_APOLLO4_MSPI1_BASE,
                                       SEMU_APOLLO4_MSPI1_IRQ));
    SEMU_TEST_ASSERT(context,
                     fixture_init_base(&target, SEMU_APOLLO4_MSPI1_BASE,
                                       SEMU_APOLLO4_MSPI1_IRQ));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_mspi_snapshot_write(source.mspi, &writer,
                                                      &source.error));
    original = writer.data[offset];
    writer.data[offset] = 1u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_apollo4_mspi_snapshot_read(target.mspi, &reader,
                                                     &target.error));
    writer.data[offset] = original;
    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&target);
    fixture_destroy(&source);
}

static void test_dma_status_requires_endpoint(semu_test_context *context)
{
    mspi_fixture source;
    mspi_fixture target;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;

    SEMU_TEST_ASSERT(context, fixture_init(&source));
    SEMU_TEST_ASSERT(context, fixture_init(&target));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_mspi_snapshot_write(source.mspi, &writer,
                                                      &source.error));
    /* DMA status follows the attached flag, status, and is four bytes wide. */
    writer.data[5u] = 2u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_apollo4_mspi_snapshot_read(target.mspi, &reader,
                                                     &target.error));

    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&target);
    fixture_destroy(&source);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_unreachable_state_refuses),
        SEMU_TEST_CASE(test_round_trip),
        SEMU_TEST_CASE(test_mspi1_queue_count_refuses),
        SEMU_TEST_CASE(test_dma_status_requires_endpoint)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
