#include "../../src/display/raster.h"
#include "test.h"

#include "semu/types.h"

#include <string.h>

#define W 16u
#define H 16u
#define STRIDE (W * 2u)

static uint8_t buf[W * H * 2u + 8u]; /* guard bytes at end */

static raster_target make_target(void)
{
    raster_target t;
    memset(buf, 0xAA, sizeof(buf));
    t.pixels = buf;
    t.width = W;
    t.height = H;
    t.stride = STRIDE;
    return t;
}

static uint16_t pixel_at(uint32_t x, uint32_t y)
{
    size_t off = y * STRIDE + x * 2u;
    return (uint16_t)(buf[off] | (buf[off + 1u] << 8));
}

static void test_clear(semu_test_context *context)
{
    semu_error err;
    raster_target t = make_target();
    raster_bounds dirty = {0};
    uint32_t x, y;
    uint16_t color = raster_rgb565(255, 0, 0);

    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        raster_clear(&t, 0, 0, W, H, color, &dirty, &err));
    SEMU_TEST_EQ_U64(context, 0u, dirty.min_x);
    SEMU_TEST_EQ_U64(context, 0u, dirty.min_y);
    SEMU_TEST_EQ_U64(context, W, dirty.max_x);
    SEMU_TEST_EQ_U64(context, H, dirty.max_y);
    for (y = 0u; y < H; ++y) {
        for (x = 0u; x < W; ++x) {
            SEMU_TEST_EQ_U64(context, color, pixel_at(x, y));
        }
    }
}

static void test_clear_partial(semu_test_context *context)
{
    semu_error err;
    raster_target t = make_target();
    uint16_t color = raster_rgb565(0, 255, 0);
    uint32_t x, y;

    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        raster_clear(&t, 4, 4, 8, 8, color, NULL, &err));
    /* Corners should be untouched (0xAA) */
    SEMU_TEST_EQ_U64(context, 0xAAAAu, pixel_at(0, 0));
    SEMU_TEST_EQ_U64(context, 0xAAAAu, pixel_at(15, 15));
    /* Inside should be green */
    SEMU_TEST_EQ_U64(context, color, pixel_at(4, 4));
    SEMU_TEST_EQ_U64(context, color, pixel_at(11, 11));
    /* Boundary just outside */
    SEMU_TEST_EQ_U64(context, 0xAAAAu, pixel_at(3, 4));
    SEMU_TEST_EQ_U64(context, 0xAAAAu, pixel_at(4, 3));
    (void)x; (void)y;
}

static void test_rect_clipped(semu_test_context *context)
{
    semu_error err;
    raster_target t = make_target();
    raster_bounds clip = {2, 2, 10, 10};
    uint16_t color = raster_rgb565(0, 0, 255);
    raster_bounds dirty = {0};

    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        raster_rect(&t, &clip, 0, 0, W, H, color, &dirty, &err));
    SEMU_TEST_EQ_U64(context, 2u, dirty.min_x);
    SEMU_TEST_EQ_U64(context, 2u, dirty.min_y);
    SEMU_TEST_EQ_U64(context, 10u, dirty.max_x);
    SEMU_TEST_EQ_U64(context, 10u, dirty.max_y);
    SEMU_TEST_EQ_U64(context, color, pixel_at(2, 2));
    SEMU_TEST_EQ_U64(context, color, pixel_at(9, 9));
    SEMU_TEST_EQ_U64(context, 0xAAAAu, pixel_at(1, 2));
    SEMU_TEST_EQ_U64(context, 0xAAAAu, pixel_at(10, 2));
}

static void test_clip_empty(semu_test_context *context)
{
    semu_error err;
    raster_target t = make_target();
    raster_bounds clip = {20, 20, 30, 30};
    uint16_t color = raster_rgb565(255, 255, 255);
    raster_bounds dirty = {0xFF, 0xFF, 0xFF, 0xFF};

    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        raster_rect(&t, &clip, 0, 0, W, H, color, &dirty, &err));
    SEMU_TEST_EQ_U64(context, 0u, dirty.min_x);
    SEMU_TEST_EQ_U64(context, 0u, dirty.max_x);
}

static void test_triangle_axis_aligned(semu_test_context *context)
{
    semu_error err;
    raster_target t = make_target();
    uint16_t color = raster_rgb565(255, 255, 255);
    raster_bounds dirty = {0};

    /* Right triangle: (0,0), (8,0), (0,8) — integer coords */
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        raster_triangle(&t, NULL,
                        0u, 0u,
                        8u << RASTER_FP_SHIFT, 0u,
                        0u, 8u << RASTER_FP_SHIFT,
                        color, &dirty, &err));
    /* (0,0) should be inside */
    SEMU_TEST_EQ_U64(context, color, pixel_at(0, 0));
    /* (7,0) should be inside (on top edge) */
    SEMU_TEST_EQ_U64(context, color, pixel_at(7, 0));
    /* (0,7) should be inside (on left edge) */
    SEMU_TEST_EQ_U64(context, color, pixel_at(0, 7));
    /* (8,0) vertex area should be outside (past the hypotenuse) */
    SEMU_TEST_ASSERT(context, pixel_at(7, 7) != color);
}

static void test_triangle_refuse_non_axis_aligned(semu_test_context *context)
{
    semu_error err;
    raster_target t = make_target();
    uint16_t color = raster_rgb565(255, 0, 255);

    semu_error_clear(&err);
    /* Triangle with a diagonal edge — no horizontal or vertical edge */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        raster_triangle(&t, NULL,
                        0u, 0u,
                        8u << RASTER_FP_SHIFT, 4u << RASTER_FP_SHIFT,
                        4u << RASTER_FP_SHIFT, 8u << RASTER_FP_SHIFT,
                        color, NULL, &err));
}

static void test_triangle_refuse_fractional(semu_test_context *context)
{
    semu_error err;
    raster_target t = make_target();
    uint16_t color = raster_rgb565(0, 255, 255);

    semu_error_clear(&err);
    /* Fractional coordinates — partial edge */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        raster_triangle(&t, NULL,
                        0u, 0u,
                        (8u << RASTER_FP_SHIFT) + 1u, 0u,
                        0u, 8u << RASTER_FP_SHIFT,
                        color, NULL, &err));
}

static void test_overflow_bounds(semu_test_context *context)
{
    semu_error err;
    raster_target t = make_target();
    uint16_t color = raster_rgb565(255, 255, 0);

    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        raster_clear(&t, 0, 0, W + 1, H, color, NULL, &err));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        raster_clear(&t, 1, 0, W, H, color, NULL, &err));
}

static void test_invalid_stride(semu_test_context *context)
{
    semu_error err;
    raster_target t = make_target();
    uint16_t color = raster_rgb565(255, 0, 0);

    t.stride = W; /* too small for RGB565 */
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        raster_clear(&t, 0, 0, W, H, color, NULL, &err));
}

static void test_repeat_hash(semu_test_context *context)
{
    semu_error err;
    raster_target t1 = make_target();
    raster_target t2 = make_target();
    uint16_t color = raster_rgb565(31, 63, 31);
    uint32_t i;
    uint32_t hash1 = 0u, hash2 = 0u;

    semu_error_clear(&err);
    raster_clear(&t1, 0, 0, W, H, color, NULL, &err);
    raster_clear(&t2, 0, 0, W, H, color, NULL, &err);
    for (i = 0u; i < sizeof(buf); ++i) {
        hash1 = hash1 * 31u + t1.pixels[i];
        hash2 = hash2 * 31u + t2.pixels[i];
    }
    SEMU_TEST_EQ_U64(context, hash1, hash2);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_clear),
        SEMU_TEST_CASE(test_clear_partial),
        SEMU_TEST_CASE(test_rect_clipped),
        SEMU_TEST_CASE(test_clip_empty),
        SEMU_TEST_CASE(test_triangle_axis_aligned),
        SEMU_TEST_CASE(test_triangle_refuse_non_axis_aligned),
        SEMU_TEST_CASE(test_triangle_refuse_fractional),
        SEMU_TEST_CASE(test_overflow_bounds),
        SEMU_TEST_CASE(test_invalid_stride),
        SEMU_TEST_CASE(test_repeat_hash)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
