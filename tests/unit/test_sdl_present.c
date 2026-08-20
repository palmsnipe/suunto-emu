#include "test.h"

#include "../../src/frontends/sdl_present_core.c"

#include <limits.h>
#include <stdint.h>
#include <string.h>

static void test_valid_240x240(semu_test_context *context)
{
    semu_error err;
    sdl_present_descriptor desc;
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        sdl_present_core_validate(SEMU_PIXEL_RGB565_LE,
            240u, 240u, 480u, 115200u, 2u, &desc, &err));
    SEMU_TEST_EQ_U64(context, 240u, desc.width);
    SEMU_TEST_EQ_U64(context, 240u, desc.height);
    SEMU_TEST_EQ_U64(context, 480u, desc.stride);
    SEMU_TEST_EQ_U64(context, 2u, desc.scale);
    SEMU_TEST_EQ_U64(context, 115200u, desc.expected_size);
}

static void test_padded_stride(semu_test_context *context)
{
    semu_error err;
    sdl_present_descriptor desc;
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        sdl_present_core_validate(SEMU_PIXEL_RGB565_LE,
            240u, 240u, 512u, 122880u, 1u, &desc, &err));
    SEMU_TEST_EQ_U64(context, 512u, desc.stride);
    SEMU_TEST_EQ_U64(context, 122880u, desc.expected_size);
}

static void test_bad_format(semu_test_context *context)
{
    semu_error err;
    sdl_present_descriptor desc;
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        sdl_present_core_validate((semu_pixel_format)99,
            240u, 240u, 480u, 115200u, 1u, &desc, &err));
}

static void test_zero_dimensions(semu_test_context *context)
{
    semu_error err;
    sdl_present_descriptor desc;
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        sdl_present_core_validate(SEMU_PIXEL_RGB565_LE,
            0u, 240u, 480u, 115200u, 1u, &desc, &err));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        sdl_present_core_validate(SEMU_PIXEL_RGB565_LE,
            240u, 0u, 480u, 115200u, 1u, &desc, &err));
}

static void test_stride_too_small(semu_test_context *context)
{
    semu_error err;
    sdl_present_descriptor desc;
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        sdl_present_core_validate(SEMU_PIXEL_RGB565_LE,
            240u, 240u, 478u, 115200u, 1u, &desc, &err));
}

static void test_size_too_small(semu_test_context *context)
{
    semu_error err;
    sdl_present_descriptor desc;
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        sdl_present_core_validate(SEMU_PIXEL_RGB565_LE,
            240u, 240u, 480u, 115199u, 1u, &desc, &err));
}

static void test_scale_out_of_range(semu_test_context *context)
{
    semu_error err;
    sdl_present_descriptor desc;
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        sdl_present_core_validate(SEMU_PIXEL_RGB565_LE,
            240u, 240u, 480u, 115200u, 0u, &desc, &err));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        sdl_present_core_validate(SEMU_PIXEL_RGB565_LE,
            240u, 240u, 480u, 115200u, 9u, &desc, &err));
}

static void test_width_overflow(semu_test_context *context)
{
    semu_error err;
    sdl_present_descriptor desc;
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        sdl_present_core_validate(SEMU_PIXEL_RGB565_LE,
            0x80000001u, 1u, 2u, 2u, 1u, &desc, &err));
}

static void test_host_integer_bounds(semu_test_context *context)
{
    semu_error err;
    sdl_present_descriptor desc;
    uint32_t wide = (uint32_t)((uint64_t)INT_MAX / 2u) + 1u;
    uint32_t huge_stride = (uint32_t)INT_MAX + UINT32_C(1);
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        sdl_present_core_validate(SEMU_PIXEL_RGB565_LE,
            wide, 1u, wide * 2u, SIZE_MAX, 2u, &desc, &err));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        sdl_present_core_validate(SEMU_PIXEL_RGB565_LE,
            1u, 1u, huge_stride, SIZE_MAX, 1u, &desc, &err));
}

static void test_repeat_hash(semu_test_context *context)
{
    semu_error err;
    sdl_present_descriptor d1;
    sdl_present_descriptor d2;
    uint32_t i;
    semu_error_clear(&err);
    sdl_present_core_validate(SEMU_PIXEL_RGB565_LE,
        240u, 240u, 480u, 115200u, 3u, &d1, &err);
    sdl_present_core_validate(SEMU_PIXEL_RGB565_LE,
        240u, 240u, 480u, 115200u, 3u, &d2, &err);
    for (i = 0u; i < sizeof(d1); ++i) {
        SEMU_TEST_EQ_U64(context,
            ((uint8_t *)&d1)[i], ((uint8_t *)&d2)[i]);
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_valid_240x240),
        SEMU_TEST_CASE(test_padded_stride),
        SEMU_TEST_CASE(test_bad_format),
        SEMU_TEST_CASE(test_zero_dimensions),
        SEMU_TEST_CASE(test_stride_too_small),
        SEMU_TEST_CASE(test_size_too_small),
        SEMU_TEST_CASE(test_scale_out_of_range),
        SEMU_TEST_CASE(test_width_overflow),
        SEMU_TEST_CASE(test_host_integer_bounds),
        SEMU_TEST_CASE(test_repeat_hash)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
