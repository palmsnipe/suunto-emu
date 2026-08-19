#include "../../src/display/sampling.h"
#include "test.h"

#include "semu/bus.h"
#include "semu/types.h"

#include <string.h>

#define SRAM_BASE 0x10000000u
#define SRAM_SIZE 0x00100000u
#define TEX_BASE (SRAM_BASE + 0x10000u)
#define MASK_BASE (SRAM_BASE + 0x20000u)

#define TW 8u
#define TH 8u

static uint8_t target_buf[TW * TH * 2u + 8u];

static raster_target make_target(void)
{
    raster_target t;
    memset(target_buf, 0, sizeof(target_buf));
    t.pixels = target_buf;
    t.width = TW;
    t.height = TH;
    t.stride = TW * 2u;
    return t;
}

static uint16_t pixel_at(uint32_t x, uint32_t y)
{
    size_t off = y * (TW * 2u) + x * 2u;
    return (uint16_t)(target_buf[off] | (target_buf[off + 1u] << 8));
}

static semu_bus *make_bus(semu_error *err)
{
    semu_bus *bus = semu_bus_create(err);
    if (bus != NULL) {
        semu_bus_map_ram(bus, "sram", SRAM_BASE, SRAM_SIZE, err);
    }
    return bus;
}

static void test_texture_copy(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    raster_target t = make_target();
    nema_texture_desc src = {0};
    raster_bounds dirty = {0};
    uint16_t color = raster_rgb565(255, 0, 0);
    uint8_t src_pixels[TW * TH * 2u];
    uint32_t i;

    for (i = 0u; i < TW * TH * 2u; i += 2u) {
        src_pixels[i] = (uint8_t)color;
        src_pixels[i + 1u] = (uint8_t)(color >> 8);
    }

    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_bus_load(bus, TEX_BASE, src_pixels, sizeof(src_pixels), &err);

    src.base = TEX_BASE; src.format = NEMA_TEX_FMT_RGB565;
    src.stride = TW * 2u; src.width = TW; src.height = TH;

    SEMU_TEST_EQ_U64(context, SEMU_OK,
        draw_texture(&t, NULL, bus, &src, 0, 0, 0, 0, TW, TH,
                     &dirty, &err));
    SEMU_TEST_EQ_U64(context, 0u, dirty.min_x);
    SEMU_TEST_EQ_U64(context, TW, dirty.max_x);
    for (i = 0u; i < TW; ++i) {
        SEMU_TEST_EQ_U64(context, color, pixel_at(i, 0));
    }
    semu_bus_destroy(bus);
}

static void test_texture_clipped(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    raster_target t = make_target();
    nema_texture_desc src = {0};
    raster_bounds clip = {2, 2, 6, 6};
    raster_bounds dirty = {0};
    uint16_t color = raster_rgb565(0, 255, 0);
    uint8_t src_pixels[TW * TH * 2u];
    uint32_t i;

    memset(src_pixels, 0, sizeof(src_pixels));
    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    for (i = 0u; i < TW * TH * 2u; i += 2u) {
        src_pixels[i] = (uint8_t)color;
        src_pixels[i + 1u] = (uint8_t)(color >> 8);
    }
    semu_bus_load(bus, TEX_BASE, src_pixels, sizeof(src_pixels), &err);

    src.base = TEX_BASE; src.format = NEMA_TEX_FMT_RGB565;
    src.stride = TW * 2u; src.width = TW; src.height = TH;

    SEMU_TEST_EQ_U64(context, SEMU_OK,
        draw_texture(&t, &clip, bus, &src, 0, 0, 0, 0, TW, TH,
                     &dirty, &err));
    SEMU_TEST_EQ_U64(context, 2u, dirty.min_x);
    SEMU_TEST_EQ_U64(context, 6u, dirty.max_x);
    SEMU_TEST_EQ_U64(context, color, pixel_at(2, 2));
    SEMU_TEST_EQ_U64(context, color, pixel_at(5, 5));
    SEMU_TEST_EQ_U64(context, 0u, pixel_at(1, 2));
    SEMU_TEST_EQ_U64(context, 0u, pixel_at(6, 2));
    semu_bus_destroy(bus);
}

static void test_mask_full_coverage(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    raster_target t = make_target();
    nema_texture_desc mask = {0};
    raster_bounds dirty = {0};
    uint32_t tex_color = 0x00FF0000u; /* R=0, G=0, B=255 */
    uint16_t expected = raster_rgb565(0, 0, 255);
    uint8_t mask_data[TH * 2u]; /* stride=2, 4 pixels per byte */
    uint32_t i;

    /* All alpha = 255 (0b11 per sample) */
    memset(mask_data, 0xFF, sizeof(mask_data));

    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_bus_load(bus, MASK_BASE, mask_data, sizeof(mask_data), &err);

    mask.base = MASK_BASE; mask.format = NEMA_TEX_FMT_A2LE;
    mask.stride = 2u; mask.width = TW; mask.height = TH;

    SEMU_TEST_EQ_U64(context, SEMU_OK,
        draw_mask(&t, NULL, bus, &mask, 0, 0, 0, 0, TW, TH,
                  NEMA_BL_SIMPLE, tex_color, &dirty, &err));
    SEMU_TEST_EQ_U64(context, 0u, dirty.min_x);
    SEMU_TEST_EQ_U64(context, TW, dirty.max_x);
    for (i = 0u; i < TW; ++i) {
        SEMU_TEST_EQ_U64(context, expected, pixel_at(i, 0));
    }
    semu_bus_destroy(bus);
}

static void test_mask_zero_coverage(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    raster_target t = make_target();
    nema_texture_desc mask = {0};
    uint32_t tex_color = 0x00FF0000u;
    uint8_t mask_data[TH * 2u];
    uint16_t initial = raster_rgb565(128, 64, 32);
    uint32_t i;

    /* Set initial target pixels */
    for (i = 0u; i < TW * TH * 2u; i += 2u) {
        target_buf[i] = (uint8_t)initial;
        target_buf[i + 1u] = (uint8_t)(initial >> 8);
    }

    /* All alpha = 0 */
    memset(mask_data, 0, sizeof(mask_data));

    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_bus_load(bus, MASK_BASE, mask_data, sizeof(mask_data), &err);

    mask.base = MASK_BASE; mask.format = NEMA_TEX_FMT_A2LE;
    mask.stride = 2u; mask.width = TW; mask.height = TH;

    SEMU_TEST_EQ_U64(context, SEMU_OK,
        draw_mask(&t, NULL, bus, &mask, 0, 0, 0, 0, TW, TH,
                  NEMA_BL_SIMPLE, tex_color, NULL, &err));
    /* Target should be unchanged */
    SEMU_TEST_EQ_U64(context, initial, pixel_at(0, 0));
    SEMU_TEST_EQ_U64(context, initial, pixel_at(TW - 1u, TH - 1u));
    semu_bus_destroy(bus);
}

static void test_mask_intermediate_coverage(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    raster_target t = make_target();
    nema_texture_desc mask = {0};
    uint32_t tex_color = 0x00FFFFFFu;
    uint16_t initial = raster_rgb565(128, 128, 128);
    uint8_t mask_data[TH * 2u];
    uint32_t i;

    /* Set initial target pixels */
    for (i = 0u; i < TW * TH * 2u; i += 2u) {
        target_buf[i] = (uint8_t)initial;
        target_buf[i + 1u] = (uint8_t)(initial >> 8);
    }

    /* Alpha = 1 (0b01 per sample) → 85, intermediate */
    memset(mask_data, 0x55, sizeof(mask_data));

    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_bus_load(bus, MASK_BASE, mask_data, sizeof(mask_data), &err);

    mask.base = MASK_BASE; mask.format = NEMA_TEX_FMT_A2LE;
    mask.stride = 2u; mask.width = TW; mask.height = TH;

    SEMU_TEST_EQ_U64(context, SEMU_OK,
        draw_mask(&t, NULL, bus, &mask, 0, 0, 0, 0, TW, TH,
                  NEMA_BL_SIMPLE, tex_color, NULL, &err));
    SEMU_TEST_EQ_U64(context, raster_rgb565(170u, 170u, 170u),
                     pixel_at(0, 0));
    semu_bus_destroy(bus);
}

static void test_mask_affine_translation(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    raster_target t = make_target();
    nema_texture_desc mask = {0};
    nema_affine_matrix matrix = {
        0x3F800000u, 0u, 0xC0000000u,
        0u, 0x3F800000u, 0xC0400000u
    };
    raster_bounds dirty = {0};
    uint8_t mask_data[4u] = {0x03u, 0u, 0u, 0u};
    uint16_t expected = raster_rgb565(255u, 0u, 0u);

    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_bus_load(bus, MASK_BASE, mask_data, sizeof(mask_data), &err);

    mask.base = MASK_BASE;
    mask.format = NEMA_TEX_FMT_A2LE;
    mask.sampling = NEMA_TEX_SAMPLING_BILINEAR;
    mask.stride = 1u;
    mask.width = 4u;
    mask.height = 4u;

    SEMU_TEST_EQ_U64(context, SEMU_OK,
        draw_mask_affine(&t, NULL, bus, &mask, 0u, 0u, TW, TH, &matrix,
                         NEMA_BL_SIMPLE, 0xFF0000FFu, &dirty, &err));
    SEMU_TEST_EQ_U64(context, TW, dirty.max_x);
    SEMU_TEST_EQ_U64(context, expected, pixel_at(2u, 3u));
    SEMU_TEST_EQ_U64(context, 0u, pixel_at(1u, 3u));
    SEMU_TEST_EQ_U64(context, 0u, pixel_at(2u, 2u));
    SEMU_TEST_EQ_U64(context, 0u, pixel_at(2u, 4u));
    semu_bus_destroy(bus);
}

static void test_mask_affine_invalid_matrix_atomic(
    semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    raster_target t = make_target();
    nema_texture_desc mask = {0};
    nema_affine_matrix matrix = {
        0x7FC00000u, 0u, 0u,
        0u, 0x3F800000u, 0u
    };
    uint8_t mask_data[4u] = {0xFFu, 0xFFu, 0xFFu, 0xFFu};
    uint8_t before[sizeof(target_buf)];
    uint32_t i;
    uint16_t initial = raster_rgb565(64u, 32u, 16u);

    for (i = 0u; i < TW * TH * 2u; i += 2u) {
        target_buf[i] = (uint8_t)initial;
        target_buf[i + 1u] = (uint8_t)(initial >> 8u);
    }
    memcpy(before, target_buf, sizeof(before));

    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_bus_load(bus, MASK_BASE, mask_data, sizeof(mask_data), &err);
    mask.base = MASK_BASE;
    mask.format = NEMA_TEX_FMT_A2LE;
    mask.sampling = NEMA_TEX_SAMPLING_BILINEAR;
    mask.stride = 1u;
    mask.width = 4u;
    mask.height = 4u;

    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        draw_mask_affine(&t, NULL, bus, &mask, 0u, 0u, TW, TH, &matrix,
                         NEMA_BL_SIMPLE, 0xFFFFFFFFu, NULL, &err));
    SEMU_TEST_ASSERT(context, memcmp(before, target_buf, sizeof(before)) == 0);
    semu_bus_destroy(bus);
}

static void test_mask_unsupported_sampling_refuses(
    semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    raster_target t = make_target();
    nema_texture_desc mask = {0};
    uint8_t mask_data[TH * 2u];
    uint16_t initial = raster_rgb565(128, 128, 128);
    uint32_t i;

    for (i = 0u; i < TW * TH * 2u; i += 2u) {
        target_buf[i] = (uint8_t)initial;
        target_buf[i + 1u] = (uint8_t)(initial >> 8);
    }
    memset(mask_data, 0xFF, sizeof(mask_data));
    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_bus_load(bus, MASK_BASE, mask_data, sizeof(mask_data), &err);
    mask.base = MASK_BASE; mask.format = NEMA_TEX_FMT_A2LE;
    mask.sampling = 0xFFu;
    mask.stride = 2u; mask.width = TW; mask.height = TH;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        draw_mask(&t, NULL, bus, &mask, 0, 0, 0, 0, TW, TH,
                  NEMA_BL_SIMPLE, 0x00FFFFFFu, NULL, &err));
    SEMU_TEST_EQ_U64(context, initial, pixel_at(0, 0));
    semu_bus_destroy(bus);
}

static void test_unsupported_blend_mode(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    raster_target t = make_target();
    nema_texture_desc mask = {0};
    uint8_t mask_data[TH * 2u];

    memset(mask_data, 0xFF, sizeof(mask_data));
    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_bus_load(bus, MASK_BASE, mask_data, sizeof(mask_data), &err);

    mask.base = MASK_BASE; mask.format = NEMA_TEX_FMT_A2LE;
    mask.stride = 2u; mask.width = TW; mask.height = TH;

    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        draw_mask(&t, NULL, bus, &mask, 0, 0, 0, 0, TW, TH,
                  0x999u, 0x00FFFFFFu, NULL, &err));
    semu_bus_destroy(bus);
}

static void test_source_boundary(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    raster_target t = make_target();
    nema_texture_desc src = {0};
    uint8_t pixels[4u * 4u * 2u];

    memset(pixels, 0, sizeof(pixels));
    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_bus_load(bus, TEX_BASE, pixels, sizeof(pixels), &err);

    src.base = TEX_BASE; src.format = NEMA_TEX_FMT_RGB565;
    src.stride = 4u * 2u; src.width = 4u; src.height = 4u;

    /* Source too small for 8x8 destination */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        draw_texture(&t, NULL, bus, &src, 0, 0, 0, 0, TW, TH,
                     NULL, &err));
    semu_bus_destroy(bus);
}

static void test_source_coordinate_overflow(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    raster_target t = make_target();
    raster_bounds clip = {1u, 0u, 2u, 1u};
    nema_texture_desc src = {0};
    nema_texture_desc mask = {0};
    uint8_t src_pixel[2u] = {0x00u, 0xF8u};
    uint8_t mask_pixel[1u] = {0xFFu};
    uint16_t initial = pixel_at(1u, 0u);

    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_bus_load(bus, TEX_BASE, src_pixel, sizeof(src_pixel), &err);
    semu_bus_load(bus, MASK_BASE, mask_pixel, sizeof(mask_pixel), &err);

    src.base = TEX_BASE; src.format = NEMA_TEX_FMT_RGB565;
    src.stride = 2u; src.width = 1u; src.height = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        draw_texture(&t, &clip, bus, &src, 0xFFFFFFFFu, 0u,
                     0u, 0u, 2u, 1u, NULL, &err));
    SEMU_TEST_EQ_U64(context, initial, pixel_at(1u, 0u));

    mask.base = MASK_BASE; mask.format = NEMA_TEX_FMT_A2LE;
    mask.stride = 1u; mask.width = 1u; mask.height = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        draw_mask(&t, &clip, bus, &mask, 0xFFFFFFFFu, 0u,
                  0u, 0u, 2u, 1u, NEMA_BL_SIMPLE,
                  0x00FF0000u, NULL, &err));
    SEMU_TEST_EQ_U64(context, initial, pixel_at(1u, 0u));
    semu_bus_destroy(bus);
}

static void test_repeat_hash(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    raster_target t1 = make_target();
    raster_target t2 = make_target();
    nema_texture_desc src = {0};
    uint16_t color = raster_rgb565(31, 63, 31);
    uint8_t pixels[TW * TH * 2u];
    uint32_t i;
    uint32_t hash1 = 0u, hash2 = 0u;

    for (i = 0u; i < sizeof(pixels); i += 2u) {
        pixels[i] = (uint8_t)color;
        pixels[i + 1u] = (uint8_t)(color >> 8);
    }

    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_bus_load(bus, TEX_BASE, pixels, sizeof(pixels), &err);

    src.base = TEX_BASE; src.format = NEMA_TEX_FMT_RGB565;
    src.stride = TW * 2u; src.width = TW; src.height = TH;

    draw_texture(&t1, NULL, bus, &src, 0, 0, 0, 0, TW, TH, NULL, &err);
    draw_texture(&t2, NULL, bus, &src, 0, 0, 0, 0, TW, TH, NULL, &err);

    for (i = 0u; i < sizeof(target_buf); ++i) {
        hash1 = hash1 * 31u + t1.pixels[i];
        hash2 = hash2 * 31u + t2.pixels[i];
    }
    SEMU_TEST_EQ_U64(context, hash1, hash2);
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_texture_copy),
        SEMU_TEST_CASE(test_texture_clipped),
        SEMU_TEST_CASE(test_mask_full_coverage),
        SEMU_TEST_CASE(test_mask_zero_coverage),
        SEMU_TEST_CASE(test_mask_intermediate_coverage),
        SEMU_TEST_CASE(test_mask_affine_translation),
        SEMU_TEST_CASE(test_mask_affine_invalid_matrix_atomic),
        SEMU_TEST_CASE(test_mask_unsupported_sampling_refuses),
        SEMU_TEST_CASE(test_unsupported_blend_mode),
        SEMU_TEST_CASE(test_source_boundary),
        SEMU_TEST_CASE(test_source_coordinate_overflow),
        SEMU_TEST_CASE(test_repeat_hash)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
