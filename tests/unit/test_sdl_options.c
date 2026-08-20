#include "test.h"

#include "../../src/frontends/sdl_options.c"

#include <string.h>

static void test_default_and_valid_values(semu_test_context *context)
{
    semu_error error;
    uint32_t scale;
    char *default_argv[] = { (char *)"suunto-emu-sdl", (char *)"list" };
    char *valid_argv[] = {
        (char *)"suunto-emu-sdl", (char *)"run", (char *)"--scale",
        (char *)"0x8"
    };

    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sdl_parse_scale(2, default_argv, &scale, &error));
    SEMU_TEST_EQ_U64(context, 2u, scale);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sdl_parse_scale(4, valid_argv, &scale, &error));
    SEMU_TEST_EQ_U64(context, 8u, scale);
}

static void test_invalid_values_refuse(semu_test_context *context)
{
    static const char *values[] = {
        "0", "9", "-1", "+1", "garbage", "18446744073709551616"
    };
    char *argv[] = {
        (char *)"suunto-emu-sdl", (char *)"run", (char *)"--scale", NULL
    };
    semu_error error;
    uint32_t scale;
    size_t i;

    for (i = 0u; i < sizeof(values) / sizeof(values[0]); ++i) {
        argv[3] = (char *)values[i];
        semu_error_clear(&error);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
            semu_sdl_parse_scale(4, argv, &scale, &error));
    }
}

static void test_missing_and_null_arguments_refuse(semu_test_context *context)
{
    char *missing[] = {
        (char *)"suunto-emu-sdl", (char *)"--scale"
    };
    char *null_entry[] = {
        (char *)"suunto-emu-sdl", NULL
    };
    semu_error error;
    uint32_t scale;

    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        semu_sdl_parse_scale(2, missing, &scale, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        semu_sdl_parse_scale(2, null_entry, &scale, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        semu_sdl_parse_scale(2, NULL, &scale, &error));
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_default_and_valid_values),
        SEMU_TEST_CASE(test_invalid_values_refuse),
        SEMU_TEST_CASE(test_missing_and_null_arguments_refuse)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
