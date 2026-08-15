#include "test.h"

#include <stdint.h>

#include "../../src/soc/apollo4/mcu_control.h"
#include "../../src/soc/apollo4/reset.h"

typedef struct reset_fixture {
    semu_error error;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4_reset_controller *controller;
    semu_apollo4_mcu_control *mcu;
} reset_fixture;

static int fixture_init(reset_fixture *fixture)
{
    semu_error_clear(&fixture->error);
    fixture->bus = semu_bus_create(&fixture->error);
    fixture->scheduler = semu_scheduler_create(&fixture->error);
    if (fixture->bus == NULL || fixture->scheduler == NULL) {
        return 0;
    }
    fixture->controller = semu_apollo4_reset_controller_create(
        fixture->bus, fixture->scheduler, &fixture->error);
    if (fixture->controller == NULL) {
        return 0;
    }
    fixture->mcu = semu_apollo4_mcu_control_create(
        fixture->bus, fixture->scheduler, &fixture->error);
    return fixture->mcu != NULL;
}

static void fixture_destroy(reset_fixture *fixture)
{
    semu_apollo4_mcu_control_destroy(fixture->mcu);
    semu_apollo4_reset_controller_destroy(fixture->controller);
    semu_scheduler_destroy(fixture->scheduler);
    semu_bus_destroy(fixture->bus);
}

static semu_status mcu_read(reset_fixture *f, uint32_t offset, uint32_t *value)
{
    return semu_bus_read(f->bus, SEMU_APOLLO4_MCU_CONTROL_BASE + offset,
                         4u, value, &f->error);
}

static semu_status mcu_write(reset_fixture *f, uint32_t offset, uint32_t value)
{
    return semu_bus_write(f->bus, SEMU_APOLLO4_MCU_CONTROL_BASE + offset,
                          4u, value, &f->error);
}

/* --- callback tracking helpers --- */

typedef struct callback_log {
    unsigned call_order[4];
    unsigned count;
} callback_log;

static semu_status cb_ok(void *context, semu_error *error)
{
    callback_log *log = (callback_log *)context;
    if (log->count < 4u) {
        log->call_order[log->count] = log->count;
    }
    ++log->count;
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status cb_fail(void *context, semu_error *error)
{
    callback_log *log = (callback_log *)context;
    if (log->count < 4u) {
        log->call_order[log->count] = log->count;
    }
    ++log->count;
    semu_error_set(error, SEMU_ERR_CONFLICT, "callback failure test");
    return SEMU_ERR_CONFLICT;
}

static semu_status cb_never(void *context, semu_error *error)
{
    (void)context;
    semu_error_set(error, SEMU_ERR_CONFLICT, "callback should not run");
    return SEMU_ERR_CONFLICT;
}

/* --- tests --- */

static void test_chiprev(semu_test_context *context)
{
    reset_fixture fixture;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, mcu_read(&fixture, 0x0cu, &value));
    SEMU_TEST_EQ_U64(context, SEMU_APOLLO4_CHIPREV, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, mcu_read(&fixture, 0x0cu, &value));
    SEMU_TEST_EQ_U64(context, SEMU_APOLLO4_CHIPREV, value);
    fixture_destroy(&fixture);
}

static void test_mcu_other_offsets_read_zero(semu_test_context *context)
{
    reset_fixture fixture;
    static const uint32_t offsets[] = { 0x28u, 0x44u, 0x60u, 0x80u,
                                        0x88u, 0x108u, 0x124u, 0x33cu,
                                        0x42cu };
    size_t i;
    uint32_t value = 0xDEADu;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    for (i = 0u; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         mcu_read(&fixture, offsets[i], &value));
        SEMU_TEST_EQ_U64(context, 0u, value);
    }
    fixture_destroy(&fixture);
}

static void test_mcu_write_accept_and_unknown_refuse(semu_test_context *context)
{
    reset_fixture fixture;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     mcu_write(&fixture, 0x340u, UINT32_C(0x40)));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     mcu_write(&fixture, 0x380u, UINT32_C(0x80000000)));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     mcu_read(&fixture, 0x340u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);

    /* Unknown offset refuses read and write. */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     mcu_read(&fixture, 0x0u, &value));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     mcu_write(&fixture, 0x0u, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     mcu_read(&fixture, 0x100u, &value));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     mcu_write(&fixture, 0x100u, 0u));

    /* Read-only offset: write refuses. */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     mcu_write(&fixture, 0x0cu, 0u));

    /* Wrong width refuses. */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus,
                                   SEMU_APOLLO4_MCU_CONTROL_BASE + 0x0cu,
                                   2u, &value, &fixture.error));
    fixture_destroy(&fixture);
}

static void test_reset_order_deterministic(semu_test_context *context)
{
    reset_fixture fixture;
    callback_log log = { 0u };

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_reset_controller_register(
                         fixture.controller, cb_ok, &log, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_reset_controller_register(
                         fixture.controller, cb_ok, &log, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_reset_controller_register(
                         fixture.controller, cb_ok, &log, &fixture.error));
    semu_apollo4_reset_controller_reset(fixture.controller);
    SEMU_TEST_EQ_U64(context, 3u, log.count);
    SEMU_TEST_EQ_U64(context, 0u, log.call_order[0]);
    SEMU_TEST_EQ_U64(context, 1u, log.call_order[1]);
    SEMU_TEST_EQ_U64(context, 2u, log.call_order[2]);
    fixture_destroy(&fixture);
}

static void test_reset_failure_stops(semu_test_context *context)
{
    reset_fixture fixture;
    callback_log log = { 0u };

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_reset_controller_register(
                         fixture.controller, cb_ok, &log, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_reset_controller_register(
                         fixture.controller, cb_fail, &log, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_reset_controller_register(
                         fixture.controller, cb_never, &log, &fixture.error));
    semu_apollo4_reset_controller_reset(fixture.controller);
    SEMU_TEST_EQ_U64(context, 2u, log.count);
    fixture_destroy(&fixture);
}

static void test_repeated_reset(semu_test_context *context)
{
    reset_fixture fixture;
    callback_log log = { 0u };

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_reset_controller_register(
                         fixture.controller, cb_ok, &log, &fixture.error));
    semu_apollo4_reset_controller_reset(fixture.controller);
    SEMU_TEST_EQ_U64(context, 1u, log.count);
    log.count = 0u;
    semu_apollo4_reset_controller_reset(fixture.controller);
    SEMU_TEST_EQ_U64(context, 1u, log.count);
    log.count = 0u;
    semu_apollo4_reset_controller_reset(fixture.controller);
    SEMU_TEST_EQ_U64(context, 1u, log.count);
    SEMU_TEST_ASSERT(context, !semu_scheduler_has_events(fixture.scheduler));
    fixture_destroy(&fixture);
}

static void test_reset_controller_bus_ops_refuse(semu_test_context *context)
{
    reset_fixture fixture;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_apollo4_reset_controller_read(fixture.controller, 0x0u,
                                                       4u, &value, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_apollo4_reset_controller_write(fixture.controller, 0x0u,
                                                        4u, 0u, &fixture.error));
    fixture_destroy(&fixture);
}

static void test_callback_table_overflow(semu_test_context *context)
{
    reset_fixture fixture;
    callback_log log = { 0u };
    unsigned i;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    for (i = 0u; i < SEMU_APOLLO4_RESET_MAX_CALLBACKS; ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_apollo4_reset_controller_register(
                             fixture.controller, cb_ok, &log, &fixture.error));
    }
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
                     semu_apollo4_reset_controller_register(
                         fixture.controller, cb_ok, &log, &fixture.error));
    fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_chiprev),
        SEMU_TEST_CASE(test_mcu_other_offsets_read_zero),
        SEMU_TEST_CASE(test_mcu_write_accept_and_unknown_refuse),
        SEMU_TEST_CASE(test_reset_order_deterministic),
        SEMU_TEST_CASE(test_reset_failure_stops),
        SEMU_TEST_CASE(test_repeated_reset),
        SEMU_TEST_CASE(test_reset_controller_bus_ops_refuse),
        SEMU_TEST_CASE(test_callback_table_overflow)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
