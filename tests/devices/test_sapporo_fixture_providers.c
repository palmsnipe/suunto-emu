#include "test.h"

#include <string.h>

#include "../../src/compat/sapporo_222.h"
#include "sapporo_cxd5610.h"
#include "sapporo_ohr2.h"
#include "semu/scheduler.h"

static const char *const correct_hashes[] = {
    "a409b088a061c2fe61689c8f39a79b2c35ed0059cd66987646e0195e47a2f522",
    "c8f2d9e4c114fef0774056a316ad09c42d31b95e2e956f887ed691c3c15a9bfc",
    "ec2a4b1c472844ac6ff9cc575cb9383c29a74302107af3619f9a08fdf6abcaf1"
};

static void init_layer(semu_layer_state *state, semu_logger *logger,
                       semu_error *error)
{
    semu_error_clear(error);
    semu_log_init(logger, NULL, SEMU_LOG_ERROR);
    semu_layer_enable_checked(state, &semu_sapporo_222_no_device_layer,
        "sapporo-2.22.60", correct_hashes, 3u, error);
}

static void dummy_rx(void *context, uint8_t value, uint64_t virtual_time_ns)
{
    (void)context;
    (void)value;
    (void)virtual_time_ns;
}

static void test_gps_match(semu_test_context *context)
{
    semu_layer_state state;
    semu_logger logger;
    semu_error error;
    semu_scheduler *scheduler;
    semu_sapporo_cxd5610 *transport;
    semu_sapporo_222_fixture_context ctx;
    static const uint8_t request[] = { '@','V','E','R','\r','\n' };
    semu_transaction_result result;

    init_layer(&state, &logger, &error);
    memset(&ctx, 0, sizeof(ctx));
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    transport = semu_sapporo_cxd5610_create(scheduler, NULL, NULL,
        dummy_rx, NULL, NULL, NULL, &error);
    SEMU_TEST_ASSERT(context, transport != NULL);

    ctx.state = &state;
    ctx.logger = &logger;
    result = semu_sapporo_222_gps_exchange(&ctx, request, sizeof(request),
                                           transport, &error);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, result);
    SEMU_TEST_EQ_U64(context, 1u, state.hits);

    semu_sapporo_cxd5610_destroy(transport);
    semu_scheduler_destroy(scheduler);
}

static void test_gps_miss(semu_test_context *context)
{
    semu_layer_state state;
    semu_logger logger;
    semu_error error;
    semu_sapporo_222_fixture_context ctx;
    static const uint8_t bad[] = { 'X','X','X','X','X','X' };
    semu_transaction_result result;

    init_layer(&state, &logger, &error);
    memset(&ctx, 0, sizeof(ctx));
    ctx.state = &state;
    ctx.logger = &logger;
    result = semu_sapporo_222_gps_exchange(&ctx, bad, sizeof(bad),
                                           NULL, &error);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, result);
    SEMU_TEST_EQ_U64(context, 0u, state.hits);
}

static void test_ohr_match(semu_test_context *context)
{
    semu_layer_state state;
    semu_logger logger;
    semu_error error;
    semu_sapporo_222_fixture_context ctx;
    uint8_t request[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE];
    uint8_t response[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE];
    semu_transaction_result result;

    init_layer(&state, &logger, &error);
    memset(&ctx, 0, sizeof(ctx));
    ctx.state = &state;
    ctx.logger = &logger;
    memset(request, 0, sizeof(request));
    memset(response, 0xFF, sizeof(response));
    result = semu_sapporo_222_ohr_body_provider(&ctx,
        SEMU_SAPPORO_OHR2_COMMAND_IDENTITY, 0u, SEMU_SAPPORO_OHR2_BSL,
        request, response, &error);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, result);
    SEMU_TEST_EQ_U64(context, 1u, state.hits);
    SEMU_TEST_ASSERT(context, memcmp(response + 9u, "BSL\0", 4u) == 0);
}

static void test_ohr_unknown_command(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_222_fixture_context ctx;
    uint8_t response[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE];
    semu_transaction_result result;

    semu_error_clear(&error);
    memset(&ctx, 0, sizeof(ctx));
    memset(response, 0xFF, sizeof(response));
    result = semu_sapporo_222_ohr_body_provider(&ctx,
        (semu_sapporo_ohr2_command)99, 0u, SEMU_SAPPORO_OHR2_BSL,
        (const uint8_t *)"", response, &error);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, result);
}

static void test_gps_running_status(semu_test_context *context)
{
    semu_layer_state state;
    semu_logger logger;
    semu_error error;
    semu_scheduler *scheduler;
    semu_sapporo_cxd5610 *transport;
    semu_sapporo_222_fixture_context ctx;
    static const uint8_t request[] = { '@', 'G', 'S', 'R', '\r', '\n' };

    init_layer(&state, &logger, &error);
    memset(&ctx, 0, sizeof(ctx));
    ctx.state = &state;
    ctx.logger = &logger;
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    transport = semu_sapporo_cxd5610_create(scheduler, NULL, NULL,
        dummy_rx, NULL, NULL, NULL, &error);
    SEMU_TEST_ASSERT(context, transport != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_222_arm_gps_running_status(transport, &ctx, &error));
    SEMU_TEST_EQ_U64(context, 2u, state.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_scheduler_advance(scheduler, UINT64_C(10000000), &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        semu_sapporo_222_gps_exchange(&ctx, request, sizeof(request),
                                       transport, &error));
    semu_sapporo_cxd5610_destroy(transport);
    semu_scheduler_destroy(scheduler);
}

static void test_gps_running_status_refuses_unarmed(semu_test_context *context)
{
    semu_layer_state state;
    semu_logger logger;
    semu_error error;
    semu_sapporo_222_fixture_context ctx;
    static const uint8_t request[] = { '@', 'G', 'S', 'R', '\r', '\n' };

    init_layer(&state, &logger, &error);
    memset(&ctx, 0, sizeof(ctx));
    ctx.state = &state;
    ctx.logger = &logger;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        semu_sapporo_222_gps_exchange(&ctx, request, sizeof(request),
                                       NULL, &error));
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_gps_match),
        SEMU_TEST_CASE(test_gps_miss),
        SEMU_TEST_CASE(test_gps_running_status),
        SEMU_TEST_CASE(test_gps_running_status_refuses_unarmed),
        SEMU_TEST_CASE(test_ohr_match),
        SEMU_TEST_CASE(test_ohr_unknown_command)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
