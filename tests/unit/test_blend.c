#include "../../src/display/blend.h"
#include "test.h"

#include <string.h>

static void test_unpack_pack_roundtrip(semu_test_context *context)
{
    uint32_t r, g, b;
    for (r = 0u; r < 32u; ++r) {
        for (g = 0u; g < 64u; ++g) {
            for (b = 0u; b < 32u; ++b) {
                uint16_t packed = (uint16_t)((r << 11) | (g << 5) | b);
                rgb8 out;
                blend_unpack_rgb565(packed, &out);
                SEMU_TEST_EQ_U64(context, packed,
                    blend_pack_rgb565(out.r, out.g, out.b));
            }
        }
    }
}

static void test_coverage_zero(semu_test_context *context)
{
    semu_error err;
    rgb8 src = {255, 0, 0};
    rgb8 dst = {0, 255, 0};
    uint16_t out = 0;

    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        blend_simple(NEMA_BL_SIMPLE, src, dst, 0u, &out, &err));
    SEMU_TEST_EQ_U64(context, blend_pack_rgb565(dst.r, dst.g, dst.b), out);
}

static void test_coverage_full(semu_test_context *context)
{
    semu_error err;
    rgb8 src = {255, 128, 0};
    rgb8 dst = {0, 0, 255};
    uint16_t out = 0;

    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        blend_simple(NEMA_BL_SIMPLE, src, dst, 255u, &out, &err));
    SEMU_TEST_EQ_U64(context, blend_pack_rgb565(src.r, src.g, src.b), out);
}

static void test_intermediate_refused(semu_test_context *context)
{
    semu_error err;
    rgb8 src = {255, 0, 0};
    rgb8 dst = {0, 0, 0};
    uint16_t out = 0;

    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        blend_simple(NEMA_BL_SIMPLE, src, dst, 85u, &out, &err));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        blend_simple(NEMA_BL_SIMPLE, src, dst, 170u, &out, &err));
}

static void test_channel_extrema(semu_test_context *context)
{
    semu_error err;
    rgb8 black = {0, 0, 0};
    rgb8 white = {255, 255, 255};
    uint16_t out = 0;

    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        blend_simple(NEMA_BL_SIMPLE, white, black, 255u, &out, &err));
    SEMU_TEST_EQ_U64(context, 0xFFFFu, out);

    SEMU_TEST_EQ_U64(context, SEMU_OK,
        blend_simple(NEMA_BL_SIMPLE, black, white, 0u, &out, &err));
    SEMU_TEST_EQ_U64(context, 0xFFFFu, out);
}

static void test_tint_from_tex_color(semu_test_context *context)
{
    rgb8 tint = blend_tint_from_tex_color(0x00FF8040u);
    SEMU_TEST_EQ_U64(context, 0x40u, tint.r);
    SEMU_TEST_EQ_U64(context, 0x80u, tint.g);
    SEMU_TEST_EQ_U64(context, 0xFFu, tint.b);
}

static void test_unsupported_mode(semu_test_context *context)
{
    semu_error err;
    rgb8 src = {0, 0, 0};
    rgb8 dst = {0, 0, 0};
    uint16_t out = 0;

    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        blend_simple(0x999u, src, dst, 255u, &out, &err));
}

static void test_saturation(semu_test_context *context)
{
    semu_error err;
    rgb8 src = {255, 255, 255};
    rgb8 dst = {255, 255, 255};
    uint16_t out = 0;

    semu_error_clear(&err);
    /* Full coverage of white over white should not overflow */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        blend_simple(NEMA_BL_SIMPLE, src, dst, 255u, &out, &err));
    SEMU_TEST_EQ_U64(context, 0xFFFFu, out);
}

static void test_exhaustive_boundaries(semu_test_context *context)
{
    /* Exhaustive 5-bit and 6-bit boundary subsets for alpha 0 and 255 */
    uint32_t r, g, b;
    for (r = 0u; r < 256u; r += 8u) {
        for (g = 0u; g < 256u; g += 4u) {
            for (b = 0u; b < 256u; b += 8u) {
                semu_error err;
                rgb8 src = {(uint8_t)r, (uint8_t)g, (uint8_t)b};
                rgb8 dst = {(uint8_t)(255 - r), (uint8_t)(255 - g),
                             (uint8_t)(255 - b)};
                uint16_t out = 0;
                semu_error_clear(&err);
                /* alpha=255: result = src */
                blend_simple(NEMA_BL_SIMPLE, src, dst, 255u, &out, &err);
                SEMU_TEST_EQ_U64(context,
                    blend_pack_rgb565(src.r, src.g, src.b), out);
                /* alpha=0: result = dst */
                blend_simple(NEMA_BL_SIMPLE, src, dst, 0u, &out, &err);
                SEMU_TEST_EQ_U64(context,
                    blend_pack_rgb565(dst.r, dst.g, dst.b), out);
            }
        }
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_unpack_pack_roundtrip),
        SEMU_TEST_CASE(test_coverage_zero),
        SEMU_TEST_CASE(test_coverage_full),
        SEMU_TEST_CASE(test_intermediate_refused),
        SEMU_TEST_CASE(test_channel_extrema),
        SEMU_TEST_CASE(test_tint_from_tex_color),
        SEMU_TEST_CASE(test_unsupported_mode),
        SEMU_TEST_CASE(test_saturation),
        SEMU_TEST_CASE(test_exhaustive_boundaries)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
