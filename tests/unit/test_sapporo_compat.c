#include "test.h"

#include <string.h>

#include "../../src/compat/sapporo_222.h"
#include "semu/compat.h"

static const char *const correct_hashes[] = {
    "a409b088a061c2fe61689c8f39a79b2c35ed0059cd66987646e0195e47a2f522",
    "c8f2d9e4c114fef0774056a316ad09c42d31b95e2e956f887ed691c3c15a9bfc",
    "ec2a4b1c472844ac6ff9cc575cb9383c29a74302107af3619f9a08fdf6abcaf1"
};

static const char *const wrong_hashes[] = {
    "0000000000000000000000000000000000000000000000000000000000000000",
    "c8f2d9e4c114fef0774056a316ad09c42d31b95e2e956f887ed691c3c15a9bfc",
    "ec2a4b1c472844ac6ff9cc575cb9383c29a74302107af3619f9a08fdf6abcaf1"
};

static void test_correct_hash_enables(semu_test_context *context)
{
    semu_layer_state state;
    semu_error error;
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_layer_enable_checked(&state,
            &semu_sapporo_222_no_device_layer,
            "sapporo-2.22.60", correct_hashes, 3u, &error));
    SEMU_TEST_ASSERT(context, state.enabled);
}

static void test_wrong_hash_refuses(semu_test_context *context)
{
    semu_layer_state state;
    semu_error error;
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
        semu_layer_enable_checked(&state,
            &semu_sapporo_222_no_device_layer,
            "sapporo-2.22.60", wrong_hashes, 3u, &error));
    SEMU_TEST_ASSERT(context, !state.enabled);
}

static void test_wrong_profile_refuses(semu_test_context *context)
{
    semu_layer_state state;
    semu_error error;
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
        semu_layer_enable_checked(&state,
            &semu_sapporo_222_no_device_layer,
            "wrong-profile", correct_hashes, 3u, &error));
}

static void test_intervention_hit_and_budget(semu_test_context *context)
{
    semu_layer_state state;
    semu_logger logger;
    semu_error error;
    semu_error_clear(&error);
    semu_log_init(&logger, NULL, SEMU_LOG_ERROR);
    SEMU_TEST_ASSERT(context,
        semu_layer_enable_checked(&state,
            &semu_sapporo_222_no_device_layer,
            "sapporo-2.22.60", correct_hashes, 3u, &error) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_layer_intervention_hit(&state, &logger,
            SEMU_SAPPORO_222_IV_GPS_STARTUP, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_layer_intervention_hit(&state, &logger,
            SEMU_SAPPORO_222_IV_GPS_STARTUP, &error));
}

static void test_disabled_intervention_refuses(semu_test_context *context)
{
    semu_layer_state state;
    semu_logger logger;
    semu_error error;
    memset(&state, 0, sizeof(state));
    semu_error_clear(&error);
    semu_log_init(&logger, NULL, SEMU_LOG_ERROR);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_layer_intervention_hit(&state, &logger,
            SEMU_SAPPORO_222_IV_GPS_STARTUP, &error));
}

static void test_out_of_range_intervention(semu_test_context *context)
{
    semu_layer_state state;
    semu_logger logger;
    semu_error error;
    semu_error_clear(&error);
    semu_log_init(&logger, NULL, SEMU_LOG_ERROR);
    SEMU_TEST_ASSERT(context,
        semu_layer_enable_checked(&state,
            &semu_sapporo_222_no_device_layer,
            "sapporo-2.22.60", correct_hashes, 3u, &error) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        semu_layer_intervention_hit(&state, &logger, 99u, &error));
}

static void test_separate_intervention_budgets(semu_test_context *context)
{
    semu_layer_state state;
    semu_logger logger;
    semu_error error;
    semu_error_clear(&error);
    semu_log_init(&logger, NULL, SEMU_LOG_ERROR);
    SEMU_TEST_ASSERT(context,
        semu_layer_enable_checked(&state,
            &semu_sapporo_222_no_device_layer,
            "sapporo-2.22.60", correct_hashes, 3u, &error) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_layer_intervention_hit(&state, &logger,
            SEMU_SAPPORO_222_IV_GPS_STARTUP, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_layer_intervention_hit(&state, &logger,
            SEMU_SAPPORO_222_IV_OHR_STARTUP, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_layer_intervention_hit(&state, &logger,
            SEMU_SAPPORO_222_IV_GPS_STARTUP, &error));
}

static void test_production_install_and_checksum(semu_test_context *context)
{
    semu_bus *bus;
    semu_layer_state state;
    semu_logger logger;
    semu_error error;
    uint8_t table[64];
    uint8_t sector[4096];
    size_t i;
    semu_error_clear(&error);
    semu_log_init(&logger, NULL, SEMU_LOG_ERROR);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_ASSERT(context,
        semu_bus_map_ram(bus, "firmware", 0u, 0x200000u, &error) == SEMU_OK);
    SEMU_TEST_ASSERT(context,
        semu_bus_map_ram(bus, "flash", 0x14000000u, 0x02000000u,
                         &error) == SEMU_OK);
    for (i = 0u; i < sizeof(table); ++i) {
        table[i] = (uint8_t)(i * 17u + 3u);
    }
    SEMU_TEST_ASSERT(context,
        semu_bus_load(bus, 0x00199ee8u, table, sizeof(table), &error)
        == SEMU_OK);
    SEMU_TEST_ASSERT(context,
        semu_layer_enable_checked(&state, &semu_sapporo_222_no_device_layer,
            "sapporo-2.22.60", correct_hashes, 3u, &error) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_222_install_no_device(bus, &state, &logger, &error));
    SEMU_TEST_EQ_U64(context, 1u, state.hits);
    SEMU_TEST_ASSERT(context,
        semu_bus_copy_out(bus, 0x14fff000u, sector, sizeof(sector),
                         &error) == SEMU_OK);
    SEMU_TEST_ASSERT(context,
        memcmp(sector, "ProductionData", 14u) == 0);
    SEMU_TEST_ASSERT(context,
        memcmp(sector + 256u, "ACCR", 4u) == 0);
    SEMU_TEST_ASSERT(context,
        memcmp(sector + 512u, "ACCC", 4u) == 0);
    SEMU_TEST_ASSERT(context,
        memcmp(sector + 768u, "MAGN", 4u) == 0);
    SEMU_TEST_ASSERT(context,
        memcmp(sector + 1536u, "HLAT", 4u) == 0);
    SEMU_TEST_ASSERT(context,
        memcmp(sector + 1552u, "EMUHLAT00001", 12u) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_sapporo_222_install_no_device(bus, &state, &logger, &error));
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_correct_hash_enables),
        SEMU_TEST_CASE(test_wrong_hash_refuses),
        SEMU_TEST_CASE(test_wrong_profile_refuses),
        SEMU_TEST_CASE(test_intervention_hit_and_budget),
        SEMU_TEST_CASE(test_disabled_intervention_refuses),
        SEMU_TEST_CASE(test_out_of_range_intervention),
        SEMU_TEST_CASE(test_separate_intervention_budgets),
        SEMU_TEST_CASE(test_production_install_and_checksum)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
