/* Ticket 805 / E-RE-SAP235-WIDGET-SCALE-001. Synthetic pixels only. */
#include "../../src/display/nema_tsc6a_internal.h"
#include "test.h"
#include <string.h>

#define SOURCE UINT32_C(0x100a490c)
static uint8_t asset[2700];
static uint8_t panel[480 * 240];
static uint8_t before[sizeof(panel)];

static nema_draw_snapshot widgets_tuple(void)
{
    nema_draw_snapshot s;
    memset(&s, 0, sizeof(s));
    s.src_present = 1u; s.src_base = SOURCE; s.src_format = NEMA_FMT_TSC6A;
    s.src_sampling = 1u; s.src_stride = 180u; s.src_width = s.src_height = 60u;
    s.target_base = 0x1012b520u; s.target_format = NEMA_FMT_RGB565;
    s.target_stride = 480u; s.target_width = s.target_height = 240u;
    s.clip_max_x = 240u; s.clip_max_y = 81u;
    s.draw_cmd = NEMA_DRAW_QUAD; s.draw_color = 0xff555555u;
    s.tex_color = 0xffffffffu; s.codeptr = 0x941e8000u;
    s.imem_datah = 0x004e0002u; s.imem_datal = 0x804b1286u;
    s.matrix_present = 1u; s.mm00 = 0x3f7fffffu; s.mm11 = 0x3f800000u;
    s.mm02 = 0xc32b0000u; s.mm12 = 0xc1c00000u; /* -171, -24 */
    s.point0_x = s.point3_x = 171u << 16u;
    s.point1_x = s.point2_x = 231u << 16u;
    s.point0_y = s.point1_y = 24u << 16u;
    s.point2_y = s.point3_y = 81u << 16u;
    return s;
}

static semu_bus *source_bus(size_t size, semu_error *error)
{
    semu_bus *bus = semu_bus_create(error);
    if (bus != NULL && semu_bus_map_ram(bus, "source", SOURCE, size, error) != SEMU_OK) {
        semu_bus_destroy(bus); return NULL;
    }
    return bus;
}

/* Alternating red/gray blocks, with a diagonal 0/1/2/3 index ramp per block.
 * This exposes both block addressing and individual texel row/column order. */
static void patterned_asset(void)
{
    unsigned b, row, col;
    memset(asset, 0, sizeof(asset));
    for (b = 0u; b < 225u; ++b) {
        for (row = 0u; row < 4u; ++row)
            for (col = 0u; col < 4u; ++col)
                asset[b * 12u + row] |= (uint8_t)(((row + col) % 4u) << (col * 2u));
        asset[b * 12u + 6u] = b % 2u ? 0u : 0xf0u;
        asset[b * 12u + 7u] = b % 2u ? 0xf0u : 0xffu;
        asset[b * 12u + 8u] = 0xffu; asset[b * 12u + 9u] = 7u;
    }
}

static void test_widgets_scale_pixels(semu_test_context *c)
{
    /* Hand-computed RGB565 for channels 0,85,170,255, with E0 black. */
    static const uint16_t colors[2][4] = {
        { 0x0000u, 0x52aau, 0xad55u, 0xffffu },
        { 0x0000u, 0x5000u, 0xa800u, 0xf800u }
    };
    semu_error error;
    semu_bus *bus = source_bus(sizeof(asset), &error);
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot s = widgets_tuple();
    unsigned x, y, variant;
    SEMU_TEST_ASSERT(c, bus != NULL);
    SEMU_TEST_EQ_U64(c, SEMU_OK, nema_tsc6a_create(&surface, &error));
    patterned_asset();
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_load(bus, SOURCE, asset, sizeof(asset), &error));
    for (variant = 0u; variant < 2u; ++variant) {
        /* The observed alternate and the already accepted identity consume
         * exactly the same 3420 texels; every other panel byte stays intact. */
        s.mm00 = variant == 0u ? 0x3f7fffffu : 0x3f800000u;
        memset(panel, 0x5a, sizeof(panel));
        SEMU_TEST_EQ_U64(c, SEMU_OK,
            nema_tsc6a_resolve_mask(surface, bus, &s, panel, 480u, &error));
        for (y = 0u; y < 240u; ++y) for (x = 0u; x < 240u; ++x) {
            uint16_t expected = 0x5a5au;
            size_t off = y * 480u + x * 2u;
            if (x >= 171u && x < 231u && y >= 24u && y < 81u) {
                unsigned sx = x - 171u, sy = y - 24u;
                unsigned block = sy / 4u * 15u + sx / 4u;
                expected = colors[block % 2u][(sx % 4u + sy % 4u) % 4u];
            }
            SEMU_TEST_EQ_U64(c, expected, panel[off] | ((uint16_t)panel[off + 1u] << 8u));
        }
    }
    nema_tsc6a_destroy(surface); semu_bus_destroy(bus);
}

static void test_widgets_near_misses_refuse(semu_test_context *c)
{
    semu_error error;
    semu_bus *bus = source_bus(sizeof(asset), &error);
    nema_tsc6a *surface = NULL;
    unsigned variant;
    SEMU_TEST_ASSERT(c, bus != NULL);
    SEMU_TEST_EQ_U64(c, SEMU_OK, nema_tsc6a_create(&surface, &error));
    patterned_asset();
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_load(bus, SOURCE, asset, sizeof(asset), &error));
    for (variant = 0u; variant < 15u; ++variant) {
        nema_draw_snapshot s = widgets_tuple();
        switch (variant) {
        case 0: s.mm00 = 0x3f7ffffeu; break; /* two ULP below one */
        case 1: s.mm00 = 0x3f800001u; break;
        case 2: s.mm00 = 0x3f000000u; break;
        case 3: s.mm11 = 0x3f7fffffu; break; /* no combined alternates */
        case 4: s.mm02 = 0xc32b0001u; break;
        case 5: s.mm12 = 0xc1c00001u; break;
        case 6: s.clip_min_x = 1u; break;
        case 7: s.clip_min_y = 1u; break;
        case 8: s.clip_max_x = 239u; break;
        case 9: s.clip_max_y = 82u; break;
        case 10: s.draw_color = 0xff000000u; break;
        case 11: s.point0_y = s.point1_y = 25u << 16u; break;
        case 12: s.point2_y = s.point3_y = 80u << 16u; break;
        case 13: s.mm01 = 1u; break;
        default: s.mm10 = 1u; break;
        }
        memset(panel, 0xa5, sizeof(panel)); memcpy(before, panel, sizeof(panel));
        SEMU_TEST_EQ_U64(c, SEMU_ERR_UNSUPPORTED,
            nema_tsc6a_resolve_mask(surface, bus, &s, panel, 480u, &error));
        SEMU_TEST_ASSERT(c, memcmp(before, panel, sizeof(panel)) == 0);
    }
    nema_tsc6a_destroy(surface); semu_bus_destroy(bus);
}

static void test_widgets_invalid_source_refuses(semu_test_context *c)
{
    semu_error error;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot s = widgets_tuple();
    unsigned variant;
    SEMU_TEST_EQ_U64(c, SEMU_OK, nema_tsc6a_create(&surface, &error));
    for (variant = 0u; variant < 2u; ++variant) {
        size_t size = sizeof(asset) - variant;
        semu_bus *bus = source_bus(size, &error);
        SEMU_TEST_ASSERT(c, bus != NULL);
        patterned_asset();
        if (variant == 0u) asset[224u * 12u + 11u] = 128u;
        SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_load(bus, SOURCE, asset, size, &error));
        memset(panel, 0xa5, sizeof(panel)); memcpy(before, panel, sizeof(panel));
        SEMU_TEST_ASSERT(c, nema_tsc6a_resolve_mask(surface, bus, &s,
            panel, 480u, &error) != SEMU_OK);
        if (variant == 0u) SEMU_TEST_ASSERT(c, strstr(error.text, "auxiliary") != NULL);
        SEMU_TEST_ASSERT(c, memcmp(before, panel, sizeof(panel)) == 0);
        semu_bus_destroy(bus);
    }
    nema_tsc6a_destroy(surface);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_widgets_scale_pixels),
        SEMU_TEST_CASE(test_widgets_near_misses_refuse),
        SEMU_TEST_CASE(test_widgets_invalid_source_refuses)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
