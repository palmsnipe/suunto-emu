#include "../../src/display/sampling.h"
#include "test.h"
#include <string.h>

static void check_empty(semu_test_context *context, uint32_t x, uint32_t y,
    uint32_t w, uint32_t h, const raster_bounds *clip)
{
    uint8_t pixel[2] = {0};
    raster_target target = {pixel, 240u, 240u, 480u};
    raster_bounds out = {1u, 2u, 3u, 4u};
    int32_t dx = -1, dy = -1; semu_error error;
    SEMU_TEST_EQ_U64(context, SEMU_OK, sampling_compute_clip(&target, clip,
        x, y, w, h, &out, &dx, &dy, &error));
    SEMU_TEST_EQ_U64(context, 0u, out.min_x | out.min_y | out.max_x | out.max_y);
    SEMU_TEST_EQ_U64(context, 0u, dx);
    SEMU_TEST_EQ_U64(context, 0u, dy);
}

static void test_sampling_clip_offscreen(semu_test_context *context)
{
    const raster_bounds clip = {239u, 0u, 240u, 42u};
    /* E-SAP-UI-239-001: animated glyph entirely right of the viewport. */
    check_empty(context, 298u, 15u, 9u, 12u, &clip);
    check_empty(context, 15u, 298u, 12u, 9u, NULL);
    check_empty(context, 240u, 0u, 1u, 1u, NULL);
    check_empty(context, 0u, 240u, 1u, 1u, NULL);
}

static void test_sampling_clip_disjoint(semu_test_context *context)
{
    const raster_bounds clips[] = {{0u, 0u, 100u, 42u}, {160u, 0u, 240u, 42u},
        {0u, 0u, 240u, 10u}, {0u, 30u, 240u, 240u}};
    for (unsigned i = 0u; i < SEMU_ARRAY_LEN(clips); ++i)
        check_empty(context, 150u, 15u, 9u, 12u, &clips[i]);
    check_empty(context, 12u, 12u, 0u, 8u, NULL);
    check_empty(context, 12u, 12u, 8u, 0u, NULL);
}

static void test_sampling_clip_intersection_oracle(semu_test_context *context)
{
    static const uint32_t starts[] = {0u, 1u, 7u, 8u, 9u, 298u};
    static const uint32_t sizes[] = {0u, 1u, 3u, 9u};
    const raster_bounds clips[] = {{0u, 0u, 8u, 8u}, {2u, 3u, 7u, 6u}, {9u, 9u, 12u, 12u}};
    uint8_t pixel[2] = {0}; raster_target target = {pixel, 8u, 8u, 16u};
    for (unsigned c = 0u; c < SEMU_ARRAY_LEN(clips); ++c)
    for (unsigned a = 0u; a < SEMU_ARRAY_LEN(starts); ++a)
    for (unsigned b = 0u; b < SEMU_ARRAY_LEN(starts); ++b)
    for (unsigned w = 0u; w < SEMU_ARRAY_LEN(sizes); ++w)
    for (unsigned h = 0u; h < SEMU_ARRAY_LEN(sizes); ++h) {
        raster_bounds expected = {8u, 8u, 0u, 0u}, out;
        int32_t dx, dy; semu_error error; unsigned count = 0u;
        /* Independent pixel-membership oracle, not the implementation's min/max formula. */
        for (unsigned y = 0u; y < 8u; ++y) for (unsigned x = 0u; x < 8u; ++x) {
            if (x < starts[a] || y < starts[b] || x >= starts[a] + sizes[w] ||
                y >= starts[b] + sizes[h] || x < clips[c].min_x || y < clips[c].min_y ||
                x >= clips[c].max_x || y >= clips[c].max_y) continue;
            if (x < expected.min_x) expected.min_x = x;
            if (y < expected.min_y) expected.min_y = y;
            if (x + 1u > expected.max_x) expected.max_x = x + 1u;
            if (y + 1u > expected.max_y) expected.max_y = y + 1u;
            ++count;
        }
        if (!count) memset(&expected, 0, sizeof(expected));
        SEMU_TEST_EQ_U64(context, SEMU_OK, sampling_compute_clip(&target, &clips[c],
            starts[a], starts[b], sizes[w], sizes[h], &out, &dx, &dy, &error));
        SEMU_TEST_ASSERT(context, !memcmp(&expected, &out, sizeof(out)));
        SEMU_TEST_EQ_U64(context, count ? expected.min_x - starts[a] : 0u, dx);
        SEMU_TEST_EQ_U64(context, count ? expected.min_y - starts[b] : 0u, dy);
    }
}

static void test_sampling_clip_refusal_atomic(semu_test_context *context)
{
    for (unsigned mode = 0u; mode < 14u; ++mode) {
        uint8_t pixel[2] = {0}; raster_target target = {pixel, 240u, 240u, 480u};
        raster_bounds clip = {0u, 0u, 240u, 240u};
        raster_bounds out = {1u, 2u, 3u, 4u}, before = out;
        int32_t dx = 17, dy = 19; semu_error error;
        uint32_t x = 0u, y = 0u, w = 1u, h = 1u;
        if (mode == 1u) target.pixels = NULL;
        if (mode == 5u) target.width = 0u;
        if (mode == 6u) target.height = 0u;
        if (mode == 7u) target.stride = 479u;
        if (mode == 8u) { target.width = UINT32_MAX; target.stride = UINT32_MAX; }
        if (mode == 9u) x = UINT32_MAX;
        if (mode == 10u) y = UINT32_MAX;
        if (mode == 11u) clip.min_x = 241u;
        if (mode == 12u) clip.min_y = 241u;
        if (mode == 13u) {
            target.height = UINT32_MAX; h = UINT32_MAX;
            clip.min_y = (uint32_t)INT32_MAX + 1u; clip.max_y = clip.min_y + 1u;
        }
        SEMU_TEST_ASSERT(context, sampling_compute_clip(mode == 0u ? NULL : &target,
            &clip, x, y, w, h, mode == 2u ? NULL : &out,
            mode == 3u ? NULL : &dx, mode == 4u ? NULL : &dy, &error) != SEMU_OK);
        SEMU_TEST_ASSERT(context, !memcmp(&out, &before, sizeof(out)));
        SEMU_TEST_EQ_U64(context, 17u, dx);
        SEMU_TEST_EQ_U64(context, 19u, dy);
    }
}

static void test_sampling_clip_draw_consumers(semu_test_context *context)
{
    uint8_t pixels[128], before[128]; raster_target target = {pixels, 8u, 8u, 16u};
    semu_error error; semu_bus *bus = semu_bus_create(&error);
    nema_texture_desc texture = {0x10000000u, NEMA_TEX_FMT_RGB565, 0u, 8u, 4u, 4u};
    nema_texture_desc mask = {0x10000000u, NEMA_TEX_FMT_A2LE, 1u, 1u, 4u, 4u};
    nema_affine_matrix matrix = {0x3f800000u, 0u, 0u, 0u, 0x3f800000u, 0u};
    raster_bounds clip = {0u, 0u, 2u, 2u};
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_ram(bus, "mask", 0x10000000u, 32u, &error));
    for (unsigned i = 0u; i < 32u; i += 4u)
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, 0x10000000u + i, 4u, UINT32_MAX, &error));
    for (unsigned mode = 0u; mode < 3u; ++mode) {
        memset(pixels, 0x5a, sizeof(pixels)); memcpy(before, pixels, sizeof(before));
        for (unsigned phase = 0u; phase < 3u; ++phase) {
            uint32_t x = phase == 0u ? 298u : phase == 1u ? 3u : 1u;
            semu_status status = mode == 0u ? draw_texture(&target, &clip, bus, &texture,
                0u, 0u, x, 1u, 1u, 1u, NULL, &error) : mode == 1u ?
                draw_mask(&target, &clip, bus, &mask, 0u, 0u, x, 1u, 1u, 1u,
                    NEMA_BL_SIMPLE, UINT32_MAX, NULL, &error) :
                draw_mask_affine(&target, &clip, bus, &mask, x, 1u, 1u, 1u, &matrix,
                    NEMA_BL_SIMPLE, UINT32_MAX, NULL, &error);
            SEMU_TEST_EQ_U64(context, SEMU_OK, status);
            if (phase < 2u) SEMU_TEST_ASSERT(context, !memcmp(before, pixels, sizeof(before)));
        }
        before[18] = before[19] = 0xffu;
        SEMU_TEST_ASSERT(context, !memcmp(before, pixels, sizeof(before)));
    }
    /* No empty-region shortcut around the affine format/matrix checks. */
    memcpy(before, pixels, sizeof(before)); matrix.mm00 = 0x7fc00000u;
    SEMU_TEST_ASSERT(context, draw_mask_affine(&target, &clip, bus, &mask,
        298u, 0u, 1u, 1u, &matrix, NEMA_BL_SIMPLE, UINT32_MAX, NULL, &error) != SEMU_OK);
    matrix.mm00 = 0x3f800000u; mask.format = 0xffu;
    SEMU_TEST_ASSERT(context, draw_mask_affine(&target, &clip, bus, &mask,
        298u, 0u, 1u, 1u, &matrix, NEMA_BL_SIMPLE, UINT32_MAX, NULL, &error) != SEMU_OK);
    SEMU_TEST_ASSERT(context, !memcmp(before, pixels, sizeof(before)));
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_sampling_clip_offscreen),
        SEMU_TEST_CASE(test_sampling_clip_disjoint),
        SEMU_TEST_CASE(test_sampling_clip_intersection_oracle),
        SEMU_TEST_CASE(test_sampling_clip_refusal_atomic),
        SEMU_TEST_CASE(test_sampling_clip_draw_consumers)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
