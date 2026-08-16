#include "test.h"

#include <stdint.h>

#include "../../src/soc/apollo4/power.h"

#define NEMA_MASK (UINT32_C(1) << 17)

typedef struct power_fixture {
    semu_error error;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4_power *power;
} power_fixture;

typedef struct callback_record {
    unsigned count;
    semu_apollo4_power_gate gate[8];
    int enabled[8];
    uint64_t time[8];
} callback_record;

static void power_callback(void *context, semu_apollo4_power_gate gate,
                           int enabled, uint64_t virtual_time_ns)
{
    callback_record *record = (callback_record *)context;
    if (record->count < 8u) {
        record->gate[record->count] = gate;
        record->enabled[record->count] = enabled;
        record->time[record->count] = virtual_time_ns;
    }
    ++record->count;
}

static int fixture_init(power_fixture *fixture, callback_record *record)
{
    semu_error_clear(&fixture->error);
    fixture->bus = semu_bus_create(&fixture->error);
    fixture->scheduler = semu_scheduler_create(&fixture->error);
    if (fixture->bus == NULL || fixture->scheduler == NULL) {
        return 0;
    }
    fixture->power = semu_apollo4_power_create(
        fixture->bus, fixture->scheduler, power_callback, record,
        &fixture->error);
    return fixture->power != NULL;
}

static void fixture_destroy(power_fixture *fixture)
{
    semu_apollo4_power_destroy(fixture->power);
    semu_scheduler_destroy(fixture->scheduler);
    semu_bus_destroy(fixture->bus);
}

static semu_status read_register(power_fixture *fixture, uint32_t offset,
                                  uint32_t *value)
{
    return semu_bus_read(fixture->bus, SEMU_APOLLO4_POWER_BASE + offset,
                         4u, value, &fixture->error);
}

static semu_status write_register(power_fixture *fixture, uint32_t offset,
                                   uint32_t value)
{
    return semu_bus_write(fixture->bus, SEMU_APOLLO4_POWER_BASE + offset,
                          4u, value, &fixture->error);
}

static void test_reset_values(semu_test_context *context)
{
    callback_record record = { 0u };
    power_fixture fixture;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture, &record));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x00u, &value));
    SEMU_TEST_EQ_U64(context, 0x0du, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x04u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x08u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x24u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x2cu, &value));
    SEMU_TEST_EQ_U64(context, 0x3fcu, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x100u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x108u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, 0u, record.count);
    fixture_destroy(&fixture);
}

static void test_requests_masks_and_callbacks(semu_test_context *context)
{
    callback_record record = { 0u };
    power_fixture fixture;
    uint32_t value = 0u;
    semu_error_clear(&fixture.error);

    SEMU_TEST_ASSERT(context, fixture_init(&fixture, &record));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler, 17u,
                                             &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x00u,
                                                      UINT32_C(0xffffffff)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x00u, &value));
    SEMU_TEST_EQ_U64(context, 0x1fu, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x04u,
                                                      NEMA_MASK | 0x20u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x04u, &value));
    SEMU_TEST_EQ_U64(context, NEMA_MASK | 0x20u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x08u, &value));
    SEMU_TEST_EQ_U64(context, NEMA_MASK | 0x1e0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x24u,
                                                      UINT32_C(0xffffffff)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x2cu,
                                                      UINT32_C(0xffffffff)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x100u, 1u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x24u, &value));
    SEMU_TEST_EQ_U64(context, 3u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x2cu, &value));
    SEMU_TEST_EQ_U64(context, 0x3ffu, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x108u, &value));
    SEMU_TEST_EQ_U64(context, 0x30u, value);
    SEMU_TEST_EQ_U64(context, 3u, record.count);
    SEMU_TEST_EQ_U64(context, SEMU_APOLLO4_POWER_GATE_NEMA, record.gate[0]);
    SEMU_TEST_EQ_U64(context, 1u, record.enabled[0]);
    SEMU_TEST_EQ_U64(context, 17u, record.time[0]);
    SEMU_TEST_EQ_U64(context, SEMU_APOLLO4_POWER_GATE_SHARED_SRAM,
                     record.gate[1]);
    SEMU_TEST_EQ_U64(context, SEMU_APOLLO4_POWER_GATE_SIMO_BUCK,
                     record.gate[2]);
    fixture_destroy(&fixture);
}

static void test_observed_legacy_accesses(semu_test_context *context)
{
    callback_record record = { 0u };
    power_fixture fixture;
    uint32_t value = 0u;
    uint32_t offset;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture, &record));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x14u,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 0x3fu, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x18u,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 0x3fu, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x1cu,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 0x8u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x28u,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x14u,
                                                      0x3fu));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x1cu,
                                                      0x8u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x24u,
                                                      3u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x28u,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 3u, value);
    for (offset = 0x140u; offset <= 0x188u; offset += 4u) {
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         write_register(&fixture, offset, 0u));
    }
    fixture_destroy(&fixture);
}

static void test_refusal_is_atomic(semu_test_context *context)
{
    callback_record record = { 0u };
    power_fixture fixture;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture, &record));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x24u, 3u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus, SEMU_APOLLO4_POWER_BASE + 0x20u,
                                   4u, &value, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_write(fixture.bus, SEMU_APOLLO4_POWER_BASE + 0x20u,
                                    4u, 0u, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_write(fixture.bus, SEMU_APOLLO4_POWER_BASE + 0x08u,
                                    4u, 0xffffffffu, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     write_register(&fixture, 0x140u, 1u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus, SEMU_APOLLO4_POWER_BASE + 0x24u,
                                   2u, &value, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x24u, &value));
    SEMU_TEST_EQ_U64(context, 3u, value);
    SEMU_TEST_EQ_U64(context, 1u, record.count);
    fixture_destroy(&fixture);
}

static void test_reset_and_repeatability(semu_test_context *context)
{
    callback_record record = { 0u };
    power_fixture fixture;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture, &record));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x100u, 1u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x04u,
                                                      NEMA_MASK));
    SEMU_TEST_EQ_U64(context, 2u, record.count);
    semu_apollo4_power_reset(fixture.power);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x00u, &value));
    SEMU_TEST_EQ_U64(context, 0x0du, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x2cu, &value));
    SEMU_TEST_EQ_U64(context, 0x3fcu, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x108u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, 4u, record.count);
    SEMU_TEST_EQ_U64(context, 0u, record.enabled[2]);
    SEMU_TEST_EQ_U64(context, 0u, record.enabled[3]);
    SEMU_TEST_ASSERT(context, !semu_scheduler_has_events(fixture.scheduler));
    fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_reset_values),
        SEMU_TEST_CASE(test_requests_masks_and_callbacks),
        SEMU_TEST_CASE(test_observed_legacy_accesses),
        SEMU_TEST_CASE(test_refusal_is_atomic),
        SEMU_TEST_CASE(test_reset_and_repeatability)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
