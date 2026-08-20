#include "test.h"

#include <stdint.h>

#include "../../src/soc/apollo4/iom.h"

typedef struct iom_fixture {
    semu_error error;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4_iom *iom;
} iom_fixture;

static int fixture_init(iom_fixture *fixture)
{
    semu_error_clear(&fixture->error);
    fixture->bus = semu_bus_create(&fixture->error);
    fixture->scheduler = semu_scheduler_create(&fixture->error);
    if (fixture->bus == NULL || fixture->scheduler == NULL) {
        return 0;
    }
    fixture->iom = semu_apollo4_iom_create(
        fixture->bus, SEMU_APOLLO4_IOM2_BASE, SEMU_APOLLO4_IOM2_IRQ,
        NULL, NULL, NULL, NULL, fixture->scheduler, &fixture->error);
    return fixture->iom != NULL;
}

static void fixture_destroy(iom_fixture *fixture)
{
    semu_apollo4_iom_destroy(fixture->iom);
    semu_scheduler_destroy(fixture->scheduler);
    semu_bus_destroy(fixture->bus);
}

static void test_unreachable_state_refuses(semu_test_context *context)
{
    static const size_t offsets[] = {
        1u, 10u, 14u, 18u, 23u, 29u, 30u, 46u, 82u
    };
    static const uint8_t values[] = {
        1u, 4u, 1u, 4u, 0x10u, 0x20u, 3u, 0u, 5u
    };
    iom_fixture source;
    iom_fixture target;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    size_t index;

    SEMU_TEST_ASSERT(context, fixture_init(&source));
    SEMU_TEST_ASSERT(context, fixture_init(&target));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_iom_snapshot_write(source.iom, &writer,
                                                     &source.error));
    for (index = 0u; index < sizeof(offsets) / sizeof(offsets[0]); ++index) {
        uint8_t original = writer.data[offsets[index]];
        writer.data[offsets[index]] = values[index];
        semu_snapshot_reader_init(&reader, writer.data, writer.size);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                         semu_apollo4_iom_snapshot_read(target.iom, &reader,
                                                        &target.error));
        writer.data[offsets[index]] = original;
    }
    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&target);
    fixture_destroy(&source);
}

static void test_dma_status_requires_endpoint(semu_test_context *context)
{
    iom_fixture source;
    iom_fixture target;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;

    SEMU_TEST_ASSERT(context, fixture_init(&source));
    SEMU_TEST_ASSERT(context, fixture_init(&target));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_iom_snapshot_write(source.iom, &writer,
                                                     &source.error));
    /* DMA status begins after the two flag bytes and seven registers. */
    writer.data[30u] = 1u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_apollo4_iom_snapshot_read(target.iom, &reader,
                                                    &target.error));

    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&target);
    fixture_destroy(&source);
}

static void test_attached_snapshot_requires_endpoint(semu_test_context *context)
{
    iom_fixture source;
    iom_fixture target;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;

    SEMU_TEST_ASSERT(context, fixture_init(&source));
    SEMU_TEST_ASSERT(context, fixture_init(&target));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_iom_snapshot_write(source.iom, &writer,
                                                     &source.error));
    writer.data[0u] = 1u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_apollo4_iom_snapshot_read(target.iom, &reader,
                                                    &target.error));

    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&target);
    fixture_destroy(&source);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_unreachable_state_refuses),
        SEMU_TEST_CASE(test_dma_status_requires_endpoint),
        SEMU_TEST_CASE(test_attached_snapshot_requires_endpoint)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
