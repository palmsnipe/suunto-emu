#include "test.h"

#include <stdint.h>

#include "../../src/soc/apollo4/rstgen.h"

typedef struct rstgen_fixture {
    semu_error error;
    semu_bus *bus;
    semu_apollo4_rstgen *rstgen;
} rstgen_fixture;

static int fixture_init(rstgen_fixture *fixture)
{
    semu_error_clear(&fixture->error);
    fixture->bus = semu_bus_create(&fixture->error);
    if (fixture->bus == NULL)
        return 0;
    fixture->rstgen = semu_apollo4_rstgen_create(fixture->bus,
                                                 &fixture->error);
    return fixture->rstgen != NULL;
}

static void fixture_destroy(rstgen_fixture *fixture)
{
    semu_apollo4_rstgen_destroy(fixture->rstgen);
    semu_bus_destroy(fixture->bus);
}

static semu_status read_cfg(rstgen_fixture *fixture, uint32_t *value)
{
    return semu_bus_read(fixture->bus, SEMU_APOLLO4_RSTGEN_BASE, 4u,
                         value, &fixture->error);
}

static semu_status write_cfg(rstgen_fixture *fixture, uint32_t value)
{
    return semu_bus_write(fixture->bus, SEMU_APOLLO4_RSTGEN_BASE, 4u,
                          value, &fixture->error);
}

static void test_cfg_reset_write_and_read(semu_test_context *context)
{
    rstgen_fixture fixture;
    uint32_t value = UINT32_C(0xffffffff);

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_cfg(&fixture, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_cfg(&fixture, 2u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_cfg(&fixture, &value));
    SEMU_TEST_EQ_U64(context, 2u, value);
    semu_apollo4_rstgen_reset(fixture.rstgen);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_cfg(&fixture, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    fixture_destroy(&fixture);
}

static void test_cfg_refusals_are_atomic(semu_test_context *context)
{
    rstgen_fixture fixture;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_cfg(&fixture, 2u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     write_cfg(&fixture, 4u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_write(fixture.bus, SEMU_APOLLO4_RSTGEN_BASE,
                                    2u, 0u, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus,
                                   SEMU_APOLLO4_RSTGEN_BASE + 4u,
                                   4u, &value, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_cfg(&fixture, &value));
    SEMU_TEST_EQ_U64(context, 2u, value);
    fixture_destroy(&fixture);
}

static void test_cfg_snapshot_round_trip_and_refusal(
    semu_test_context *context)
{
    rstgen_fixture fixture;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_error error;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_cfg(&fixture, 2u));
    semu_snapshot_writer_init(&writer);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_rstgen_snapshot_write(
                         fixture.rstgen, &writer, &error));
    semu_apollo4_rstgen_reset(fixture.rstgen);
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_rstgen_snapshot_read(
                         fixture.rstgen, &reader, &error));
    SEMU_TEST_ASSERT(context, semu_snapshot_reader_done(&reader));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_cfg(&fixture, &value));
    SEMU_TEST_EQ_U64(context, 2u, value);

    writer.data[0] = 4u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_apollo4_rstgen_snapshot_read(
                         fixture.rstgen, &reader, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_cfg(&fixture, &value));
    SEMU_TEST_EQ_U64(context, 2u, value);
    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_cfg_reset_write_and_read),
        SEMU_TEST_CASE(test_cfg_refusals_are_atomic),
        SEMU_TEST_CASE(test_cfg_snapshot_round_trip_and_refusal)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
