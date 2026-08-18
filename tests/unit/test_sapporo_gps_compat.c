#include "test.h"

#include "../../src/compat/sapporo_222.h"
#include "../../src/devices/sapporo_cxd5610.h"
#include "../../src/devices/sapporo_gps_compat.h"
#include "../../src/soc/apollo4/uart.h"

static const char *const correct_hashes[] = {
    "a409b088a061c2fe61689c8f39a79b2c35ed0059cd66987646e0195e47a2f522",
    "c8f2d9e4c114fef0774056a316ad09c42d31b95e2e956f887ed691c3c15a9bfc",
    "ec2a4b1c472844ac6ff9cc575cb9383c29a74302107af3619f9a08fdf6abcaf1"
};

static void gps_compat_rx(void *context, uint8_t value,
                          uint64_t virtual_time_ns)
{
    (void)context;
    (void)value;
    (void)virtual_time_ns;
}

static int make_fixture(
    semu_bus **bus, semu_scheduler **scheduler, semu_apollo4_uart **uart,
    semu_sapporo_cxd5610 **gps, semu_layer_state *state,
    semu_logger *logger, semu_error *error)
{
    *bus = NULL;
    *scheduler = NULL;
    *uart = NULL;
    *gps = NULL;
    semu_error_clear(error);
    semu_log_init(logger, NULL, SEMU_LOG_ERROR);
    *bus = semu_bus_create(error);
    if (*bus == NULL || semu_bus_map_ram(*bus, "sram",
            UINT32_C(0x10000000), UINT32_C(0x00180000), error) != SEMU_OK) {
        return 0;
    }
    if (semu_layer_enable_checked(state, &semu_sapporo_222_no_device_layer,
            "sapporo-2.22.60", correct_hashes, 3u, error) != SEMU_OK) {
        return 0;
    }
    *scheduler = semu_scheduler_create(error);
    if (*scheduler == NULL) return 0;
    *uart = semu_apollo4_uart_create(*scheduler, error);
    if (*uart == NULL) return 0;
    *gps = semu_sapporo_cxd5610_create(*scheduler, NULL, NULL,
                                       gps_compat_rx, NULL, NULL, NULL,
                                       error);
    return *gps != NULL;
}

static void destroy_fixture(
    semu_bus *bus, semu_scheduler *scheduler, semu_apollo4_uart *uart,
    semu_sapporo_cxd5610 *gps)
{
    semu_sapporo_cxd5610_destroy(gps);
    semu_apollo4_uart_destroy(uart);
    semu_scheduler_destroy(scheduler);
    semu_bus_destroy(bus);
}

static void test_running_status_requires_native_open(
    semu_test_context *context)
{
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4_uart *uart;
    semu_sapporo_cxd5610 *gps;
    semu_layer_state state;
    semu_sapporo_222_fixture_context fixture;
    semu_logger logger;
    semu_cpu_state cpu_state;
    semu_error error;
    uint32_t driver = UINT32_C(0x1004271c);
    uint32_t native_object = UINT32_C(0x1003067c);

    SEMU_TEST_ASSERT(context, make_fixture(
        &bus, &scheduler, &uart, &gps, &state, &logger, &error));
    fixture.state = &state;
    fixture.logger = &logger;
    fixture.gps_running_status_armed = 0;
    cpu_state = (semu_cpu_state){ 0 };
    cpu_state.r[0] = 1u;
    cpu_state.r[4] = driver;
    cpu_state.r[5] = driver + 0x74u;
    cpu_state.r[6] = driver + 0x1edu;
    cpu_state.r[15] = UINT32_C(0x0010f7c2);
    SEMU_TEST_ASSERT(context, semu_bus_write(
        bus, driver + 0x79u, 1u, 2u, &error) == SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(
        bus, driver + 0x1a0u, 4u, native_object, &error) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_gps_compat_apply(
        uart, bus, &cpu_state, gps, &fixture, &error));
    SEMU_TEST_ASSERT(context, fixture.gps_running_status_armed);
    SEMU_TEST_EQ_U64(context, UINT32_C(0x0010f7c2), cpu_state.r[15]);
    {
        uint32_t value = 0u;
        SEMU_TEST_ASSERT(context, semu_bus_read(
            bus, driver + 0x1a0u, 4u, &value, &error) == SEMU_OK);
        SEMU_TEST_EQ_U64(context, native_object, value);
    }
    destroy_fixture(bus, scheduler, uart, gps);
}

static void test_running_status_refuses_pre_open_call_without_mutation(
    semu_test_context *context)
{
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4_uart *uart;
    semu_sapporo_cxd5610 *gps;
    semu_layer_state state;
    semu_sapporo_222_fixture_context fixture;
    semu_logger logger;
    semu_cpu_state cpu_state;
    semu_error error;
    uint32_t driver = UINT32_C(0x1004271c);

    SEMU_TEST_ASSERT(context, make_fixture(
        &bus, &scheduler, &uart, &gps, &state, &logger, &error));
    fixture.state = &state;
    fixture.logger = &logger;
    fixture.gps_running_status_armed = 0;
    cpu_state = (semu_cpu_state){ 0 };
    cpu_state.r[0] = driver;
    cpu_state.r[4] = driver;
    cpu_state.r[5] = driver + 0x74u;
    cpu_state.r[6] = driver + 0x1edu;
    cpu_state.r[15] = UINT32_C(0x0010f7b8);
    SEMU_TEST_ASSERT(context, semu_bus_write(
        bus, driver + 0x79u, 1u, 2u, &error) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_gps_compat_apply(
        uart, bus, &cpu_state, gps, &fixture, &error));
    SEMU_TEST_ASSERT(context, !fixture.gps_running_status_armed);
    {
        uint32_t value = 0u;
        SEMU_TEST_ASSERT(context, semu_bus_read(
            bus, driver + 0x1a0u, 4u, &value, &error) == SEMU_OK);
        SEMU_TEST_EQ_U64(context, 0u, value);
    }
    destroy_fixture(bus, scheduler, uart, gps);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_running_status_requires_native_open),
        SEMU_TEST_CASE(test_running_status_refuses_pre_open_call_without_mutation)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
