#include "test.h"

#include <stdint.h>

#include "../../src/soc/apollo4/mram.h"

typedef struct mram_fixture {
    semu_error error;
    semu_bus *bus;
    semu_apollo4_mram *mram;
} mram_fixture;

static int fixture_init(mram_fixture *fixture)
{
    semu_error_clear(&fixture->error);
    fixture->bus = semu_bus_create(&fixture->error);
    if (fixture->bus == NULL) {
        return 0;
    }
    fixture->mram = semu_apollo4_mram_create(fixture->bus, &fixture->error);
    return fixture->mram != NULL;
}

static void fixture_destroy(mram_fixture *fixture)
{
    semu_apollo4_mram_destroy(fixture->mram);
    semu_bus_destroy(fixture->bus);
}

static semu_status read_register(mram_fixture *fixture, uint32_t offset,
                                 unsigned width, uint32_t *value)
{
    return semu_bus_read(fixture->bus, SEMU_APOLLO4_MRAM_BASE + offset,
                         width, value, &fixture->error);
}

static semu_status write_register(mram_fixture *fixture, uint32_t offset,
                                  unsigned width, uint32_t value)
{
    return semu_bus_write(fixture->bus, SEMU_APOLLO4_MRAM_BASE + offset,
                          width, value, &fixture->error);
}

static void test_reset_values(semu_test_context *context)
{
    static const uint32_t offsets[] = {
        0x04u, 0x08u, 0x10u, 0x14u, 0x18u, 0x1cu, 0x50u, 0x54u, 0x70u
    };
    mram_fixture fixture;
    uint32_t value = UINT32_C(0xffffffff);
    size_t index;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    for (index = 0u; index < sizeof(offsets) / sizeof(offsets[0]); ++index) {
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         read_register(&fixture, offsets[index], 4u, &value));
        SEMU_TEST_EQ_U64(context, 0u, value);
    }
    fixture_destroy(&fixture);
}

static void test_trace_backed_writes_and_masks(semu_test_context *context)
{
    static const struct {
        uint32_t offset;
        uint32_t value;
    } writes[] = {
        { 0x04u, 0x08u }, { 0x08u, 0xc3u }, { 0x10u, 0x1058a0u },
        { 0x14u, 0u }, { 0x18u, 0u }, { 0x1cu, 0u },
        { 0x50u, 0x10u }, { 0x54u, 0x2405u }
    };
    mram_fixture fixture;
    uint32_t value = 0u;
    size_t index;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    for (index = 0u; index < sizeof(writes) / sizeof(writes[0]); ++index) {
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         write_register(&fixture, writes[index].offset, 4u,
                                        writes[index].value));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         read_register(&fixture, writes[index].offset, 4u,
                                       &value));
        SEMU_TEST_EQ_U64(context, writes[index].value, value);
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x54u, 4u,
                                                      0x2406u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x54u, 4u,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 0x2406u, value);
    fixture_destroy(&fixture);
}

static void test_refusal_is_atomic(semu_test_context *context)
{
    mram_fixture fixture;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x54u, 4u,
                                                      0x2405u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     write_register(&fixture, 0x54u, 4u, 0x80002405u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     write_register(&fixture, 0x70u, 4u, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     write_register(&fixture, 0x0cu, 4u, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     read_register(&fixture, 0x0cu, 4u, &value));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     read_register(&fixture, 0x54u, 2u, &value));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x54u, 4u,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 0x2405u, value);
    fixture_destroy(&fixture);
}

static void test_reset_and_repeatability(semu_test_context *context)
{
    mram_fixture fixture;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x04u, 4u,
                                                      0x08u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x10u, 4u,
                                                      0x1058a0u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x50u, 4u,
                                                      0x10u));
    semu_apollo4_mram_reset(fixture.mram);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x04u, 4u,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x10u, 4u,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x50u, 4u,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x04u, 4u,
                                                      0x08u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x04u, 4u,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 0x08u, value);
    fixture_destroy(&fixture);
}

static void test_snapshot_masks_refuse(semu_test_context *context)
{
    mram_fixture source;
    mram_fixture target;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;

    SEMU_TEST_ASSERT(context, fixture_init(&source));
    SEMU_TEST_ASSERT(context, fixture_init(&target));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_mram_snapshot_write(source.mram, &writer,
                                                      &source.error));
    /* Nine little-endian words follow the register order in mram.c. */
    writer.data[0u] = 0x80u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_apollo4_mram_snapshot_read(target.mram, &reader,
                                                      &target.error));
    writer.data[0u] = 0u;
    writer.data[6u * 4u] = 0x01u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_apollo4_mram_snapshot_read(target.mram, &reader,
                                                      &target.error));
    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&target);
    fixture_destroy(&source);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_reset_values),
        SEMU_TEST_CASE(test_trace_backed_writes_and_masks),
        SEMU_TEST_CASE(test_refusal_is_atomic),
        SEMU_TEST_CASE(test_reset_and_repeatability),
        SEMU_TEST_CASE(test_snapshot_masks_refuse)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
