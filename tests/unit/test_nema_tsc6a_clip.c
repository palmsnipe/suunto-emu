#include "../../src/display/nema_tsc6a_internal.h"
#include "test.h"
#include <string.h>

#define SOURCE UINT32_C(0x100a490c)
#define TARGET UINT32_C(0x10121d40)
static uint8_t asset[2700];
static uint8_t panel[480 * 240];
static uint8_t before[sizeof(panel)];

/* Synthetic white blocks and the existing E-SAP-0041-EXT6/7 tuples. */
static nema_draw_snapshot tuple(void)
{
    nema_draw_snapshot s;
    memset(&s, 0, sizeof(s));
    s.src_present = 1u; s.src_base = SOURCE; s.src_format = NEMA_FMT_TSC6A;
    s.src_sampling = 1u; s.src_stride = 180u; s.src_width = s.src_height = 60u;
    s.target_base = TARGET; s.target_format = NEMA_FMT_RGB565;
    s.target_stride = 480u; s.target_width = s.target_height = 240u;
    s.clip_min_y = 81u; s.clip_max_x = 240u; s.clip_max_y = 162u;
    s.draw_cmd = NEMA_DRAW_QUAD; s.draw_color = 0xff555555u;
    s.tex_color = 0xffffffffu; s.codeptr = 0x941e8000u;
    s.imem_datah = 0x004e0002u; s.imem_datal = 0x804b1286u;
    s.matrix_present = 1u; s.mm00 = s.mm11 = 0x3f800000u;
    s.mm02 = 0x42540000u; /* x + 53: left-clipped seven-column strip */
    s.mm12 = 0xc2b40000u; /* y - 90 */
    s.point1_x = s.point2_x = 7u << 16u;
    s.point0_y = s.point1_y = 90u << 16u;
    s.point2_y = s.point3_y = 150u << 16u;
    return s;
}

static void white_asset(void)
{
    size_t i;
    for (i = 0; i < sizeof(asset); i += 12u) {
        memset(asset + i, 255, 9u);
        asset[i + 9u] = 7u; asset[i + 10u] = asset[i + 11u] = 0u;
    }
}

static semu_bus *source_bus(size_t size, semu_error *error)
{
    semu_bus *bus = semu_bus_create(error);
    if (bus != NULL && semu_bus_map_ram(bus, "source", SOURCE, size, error) != SEMU_OK) {
        semu_bus_destroy(bus); return NULL;
    }
    return bus;
}

static void test_clipped_auxiliary_blocks(semu_test_context *c)
{
    semu_error error;
    semu_bus *bus = source_bus(sizeof(asset), &error);
    nema_tsc6a *surface = NULL;
    unsigned variant, x, y, b;
    SEMU_TEST_ASSERT(c, bus != NULL);
    SEMU_TEST_EQ_U64(c, SEMU_OK, nema_tsc6a_create(&surface, &error));
    for (variant = 0; variant < 3u; ++variant) {
        nema_draw_snapshot s = tuple();
        unsigned x0 = 0u, x1 = 7u, y0 = 90u, y1 = 150u;
        white_asset();
        if (variant == 0u) {
            /* Only block columns 13 and 14 are sampled. */
            for (b = 0u; b < 225u; ++b)
                if (b % 15u < 13u) asset[b * 12u + 10u] = 1u;
        } else {
            /* EXT7 top clip: y - 151 - one binary32 ULP; only rows 0..10. */
            x0 = 171u; x1 = 231u; y0 = 151u; y1 = 162u;
            s.point0_x = s.point3_x = x0 << 16u;
            s.point1_x = s.point2_x = x1 << 16u;
            s.point0_y = s.point1_y = y0 << 16u;
            s.point2_y = s.point3_y = y1 << 16u;
            s.mm02 = 0xc32b0001u; s.mm12 = 0xc3170001u;
            for (b = 45u; b < 225u; ++b) asset[b * 12u + 9u] |= 8u;
            if (variant == 2u) {
                /* An empty clip samples nothing, but still validates memory. */
                s.clip_min_x = s.clip_max_x;
                for (b = 0u; b < 225u; ++b) asset[b * 12u + 11u] = 128u;
            }
        }
        SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_load(bus, SOURCE, asset, sizeof(asset), &error));
        memset(panel, 0, sizeof(panel));
        SEMU_TEST_EQ_U64(c, SEMU_OK,
            nema_tsc6a_resolve_mask(surface, bus, &s, panel, 480u, &error));
        for (y = 0; y < 240u; ++y) for (x = 0; x < 240u; ++x) {
            unsigned expected = variant != 2u && x >= x0 && x < x1 && y >= y0 && y < y1 ? 255u : 0u;
            SEMU_TEST_EQ_U64(c, expected, panel[y * 480u + x * 2u]);
            SEMU_TEST_EQ_U64(c, expected, panel[y * 480u + x * 2u + 1u]);
        }
    }
    nema_tsc6a_destroy(surface); semu_bus_destroy(bus);
}

static void test_last_visible_block_refuses_atomically(semu_test_context *c)
{
    semu_error error;
    semu_bus *bus = source_bus(sizeof(asset), &error);
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot s = tuple();
    unsigned bit;
    SEMU_TEST_ASSERT(c, bus != NULL);
    SEMU_TEST_EQ_U64(c, SEMU_OK, nema_tsc6a_create(&surface, &error));
    for (bit = 75u; bit <= 95u; ++bit) {
        white_asset();
        /* Last visible block: failure must not leak earlier target writes. */
        asset[224u * 12u + bit / 8u] |= (uint8_t)(1u << (bit % 8u));
        SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_load(bus, SOURCE, asset, sizeof(asset), &error));
        memset(panel, 0xa5, sizeof(panel)); memcpy(before, panel, sizeof(panel));
        SEMU_TEST_EQ_U64(c, SEMU_ERR_UNSUPPORTED,
            nema_tsc6a_resolve_mask(surface, bus, &s, panel, 480u, &error));
        SEMU_TEST_ASSERT(c, strstr(error.text, "auxiliary") != NULL);
        SEMU_TEST_ASSERT(c, memcmp(before, panel, sizeof(panel)) == 0);
    }
    nema_tsc6a_destroy(surface); semu_bus_destroy(bus);
}

static void test_unsampled_memory_hole_still_refuses(semu_test_context *c)
{
    semu_error error;
    semu_bus *bus = source_bus(sizeof(asset) - 1u, &error);
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot s = tuple();
    SEMU_TEST_ASSERT(c, bus != NULL);
    SEMU_TEST_EQ_U64(c, SEMU_OK, nema_tsc6a_create(&surface, &error));
    white_asset();
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_load(bus, SOURCE, asset, sizeof(asset) - 1u, &error));
    s.clip_min_x = s.clip_max_x;
    memset(panel, 0x5a, sizeof(panel)); memcpy(before, panel, sizeof(panel));
    SEMU_TEST_ASSERT(c, nema_tsc6a_resolve_mask(surface, bus, &s, panel, 480u, &error) != SEMU_OK);
    SEMU_TEST_ASSERT(c, memcmp(before, panel, sizeof(panel)) == 0);
    nema_tsc6a_destroy(surface); semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_clipped_auxiliary_blocks),
        SEMU_TEST_CASE(test_last_visible_block_refuses_atomically),
        SEMU_TEST_CASE(test_unsampled_memory_hole_still_refuses)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
