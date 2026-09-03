#include "test.h"

#include <stdint.h>

#include "../../src/soc/apollo4/watchdog.h"

typedef struct watchdog_fixture {
    semu_error error;
    semu_bus *bus;
    semu_apollo4_watchdog *watchdog;
} watchdog_fixture;

static int fixture_init(watchdog_fixture *fixture)
{
    semu_error_clear(&fixture->error);
    fixture->bus = semu_bus_create(&fixture->error);
    if (fixture->bus == NULL)
        return 0;
    fixture->watchdog = semu_apollo4_watchdog_create(fixture->bus,
                                                     &fixture->error);
    return fixture->watchdog != NULL;
}

static void fixture_destroy(watchdog_fixture *fixture)
{
    semu_apollo4_watchdog_destroy(fixture->watchdog);
    semu_bus_destroy(fixture->bus);
}

static semu_status read_cfg(watchdog_fixture *fixture, uint32_t *value)
{
    return semu_bus_read(fixture->bus, SEMU_APOLLO4_WATCHDOG_BASE, 4u,
                         value, &fixture->error);
}

static semu_status write_cfg(watchdog_fixture *fixture, uint32_t value)
{
    return semu_bus_write(fixture->bus, SEMU_APOLLO4_WATCHDOG_BASE, 4u,
                          value, &fixture->error);
}

static semu_status read_inten(watchdog_fixture *fixture, uint32_t *value)
{
    return semu_bus_read(fixture->bus, SEMU_APOLLO4_WATCHDOG_BASE +
                         SEMU_APOLLO4_WATCHDOG_INTEN_OFFSET, 4u,
                         value, &fixture->error);
}

static semu_status write_inten(watchdog_fixture *fixture, uint32_t value)
{
    return semu_bus_write(fixture->bus, SEMU_APOLLO4_WATCHDOG_BASE +
                          SEMU_APOLLO4_WATCHDOG_INTEN_OFFSET, 4u,
                          value, &fixture->error);
}

static void test_cfg_reset_write_and_read(semu_test_context *context)
{
    watchdog_fixture fixture;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_cfg(&fixture, &value));
    SEMU_TEST_EQ_U64(context, SEMU_APOLLO4_WATCHDOG_CFG_RESET, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_cfg(&fixture, UINT32_C(0x033c3d06)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_cfg(&fixture, &value));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x033c3d06), value);
    semu_apollo4_watchdog_reset(fixture.watchdog);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_cfg(&fixture, &value));
    SEMU_TEST_EQ_U64(context, SEMU_APOLLO4_WATCHDOG_CFG_RESET, value);
    fixture_destroy(&fixture);
}

static void test_cfg_refusals_are_atomic(semu_test_context *context)
{
    watchdog_fixture fixture;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_cfg(&fixture, UINT32_C(0x033c3d06)));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     write_cfg(&fixture, UINT32_C(0x0b3c3d06)));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     write_cfg(&fixture, UINT32_C(0x053c3d06)));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_write(fixture.bus,
                                    SEMU_APOLLO4_WATCHDOG_BASE,
                                    2u, 0u, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus,
                                   SEMU_APOLLO4_WATCHDOG_BASE + 8u,
                                   4u, &value, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_cfg(&fixture, &value));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x033c3d06), value);
    fixture_destroy(&fixture);
}

static void test_restart_key_and_refusals(semu_test_context *context)
{
    watchdog_fixture fixture;
    uint32_t value = UINT32_MAX;
    uint32_t address = SEMU_APOLLO4_WATCHDOG_BASE +
                       SEMU_APOLLO4_WATCHDOG_RESTART_OFFSET;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(fixture.bus, address, 4u, &value,
                                   &fixture.error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(fixture.bus, address, 4u,
                                    SEMU_APOLLO4_WATCHDOG_RESTART_KEY,
                                    &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_write(fixture.bus, address, 4u, 0u,
                                    &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_write(fixture.bus, address, 1u,
                                    SEMU_APOLLO4_WATCHDOG_RESTART_KEY,
                                    &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_cfg(&fixture, &value));
    SEMU_TEST_EQ_U64(context, SEMU_APOLLO4_WATCHDOG_CFG_RESET, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_inten(&fixture, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    fixture_destroy(&fixture);
}

static void test_inten_reset_write_and_refusal(semu_test_context *context)
{
    watchdog_fixture fixture;
    uint32_t value = UINT32_MAX;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_inten(&fixture, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_inten(&fixture, 1u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_inten(&fixture, &value));
    SEMU_TEST_EQ_U64(context, 1u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     write_inten(&fixture, 4u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus,
                                   SEMU_APOLLO4_WATCHDOG_BASE +
                                       SEMU_APOLLO4_WATCHDOG_INTEN_OFFSET,
                                   2u, &value, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_inten(&fixture, &value));
    SEMU_TEST_EQ_U64(context, 1u, value);
    semu_apollo4_watchdog_reset(fixture.watchdog);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_inten(&fixture, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    fixture_destroy(&fixture);
}

static void test_cfg_snapshot_round_trip_and_refusal(
    semu_test_context *context)
{
    watchdog_fixture fixture;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_error error;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_cfg(&fixture, UINT32_C(0x033c3d06)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_inten(&fixture, 1u));
    semu_snapshot_writer_init(&writer);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_watchdog_snapshot_write(
                         fixture.watchdog, &writer, &error));
    semu_apollo4_watchdog_reset(fixture.watchdog);
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_watchdog_snapshot_read(
                         fixture.watchdog, &reader, &error));
    SEMU_TEST_ASSERT(context, semu_snapshot_reader_done(&reader));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_cfg(&fixture, &value));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x033c3d06), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_inten(&fixture, &value));
    SEMU_TEST_EQ_U64(context, 1u, value);

    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_cfg(&fixture, SEMU_APOLLO4_WATCHDOG_CFG_RESET));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_inten(&fixture, 2u));
    writer.data[4] = 0x04u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_apollo4_watchdog_snapshot_read(
                         fixture.watchdog, &reader, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_cfg(&fixture, &value));
    SEMU_TEST_EQ_U64(context, SEMU_APOLLO4_WATCHDOG_CFG_RESET, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_inten(&fixture, &value));
    SEMU_TEST_EQ_U64(context, 2u, value);
    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_cfg_reset_write_and_read),
        SEMU_TEST_CASE(test_cfg_refusals_are_atomic),
        SEMU_TEST_CASE(test_restart_key_and_refusals),
        SEMU_TEST_CASE(test_inten_reset_write_and_refusal),
        SEMU_TEST_CASE(test_cfg_snapshot_round_trip_and_refusal)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
