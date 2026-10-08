/* Ticket 806 / the restored-navigation Control Panel witness. Synthetic
 * pixels only; the real 20x32 asset bytes stay external. */
#include "../../src/display/nema_tsc6a_internal.h"
#include "test.h"
#include <string.h>

#define SOURCE UINT32_C(0x100a7aac)
static uint8_t asset[480];
static uint8_t panel[480 * 240];
static uint8_t before[sizeof(panel)];

static nema_draw_snapshot icon_tuple(void)
{
    nema_draw_snapshot s;
    memset(&s, 0, sizeof(s));
    s.src_present = 1u; s.src_base = SOURCE; s.src_format = NEMA_FMT_TSC6A;
    s.src_sampling = 1u; s.src_stride = 60u; s.src_width = 20u;
    s.src_height = 32u;
    s.target_base = 0x10118560u; s.target_format = NEMA_FMT_RGB565;
    s.target_stride = 480u; s.target_width = s.target_height = 240u;
    s.clip_min_y = 162u; s.clip_max_x = 240u; s.clip_max_y = 240u;
    s.draw_cmd = NEMA_DRAW_QUAD; s.draw_color = 0xffffffffu;
    s.tex_color = 0xffffffffu; s.codeptr = 0x941e8000u;
    s.imem_datah = 0x004e0002u; s.imem_datal = 0x804b1286u;
    s.matrix_present = 1u; s.mm00 = 0x3f800000u; s.mm11 = 0x3f800000u;
    s.mm02 = 0xc3480000u; s.mm12 = 0xc34b0000u; /* -200, -203 */
    s.point0_x = s.point3_x = 200u << 16u;
    s.point1_x = s.point2_x = 220u << 16u;
    s.point0_y = s.point1_y = 203u << 16u;
    s.point2_y = s.point3_y = 233u << 16u;
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

/* Alternating red/gray blocks with the same diagonal index ramp as the
 * widgets module, over the icon's 5x8 block grid (40 blocks). */
static void patterned_asset(void)
{
    unsigned b, row, col;
    memset(asset, 0, sizeof(asset));
    for (b = 0u; b < 40u; ++b) {
        for (row = 0u; row < 4u; ++row)
            for (col = 0u; col < 4u; ++col)
                asset[b * 12u + row] |= (uint8_t)(((row + col) % 4u) << (col * 2u));
        asset[b * 12u + 6u] = b % 2u ? 0u : 0xf0u;
        asset[b * 12u + 7u] = b % 2u ? 0xf0u : 0xffu;
        asset[b * 12u + 8u] = 0xffu; asset[b * 12u + 9u] = 7u;
    }
}

static void test_icon_pixels(semu_test_context *c)
{
    /* Same hand-computed RGB565 channels as the widgets module. */
    static const uint16_t colors[2][4] = {
        { 0x0000u, 0x52aau, 0xad55u, 0xffffu },
        { 0x0000u, 0x5000u, 0xa800u, 0xf800u }
    };
    semu_error error;
    semu_bus *bus = source_bus(sizeof(asset), &error);
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot s = icon_tuple();
    unsigned x, y, variant;
    SEMU_TEST_ASSERT(c, bus != NULL);
    SEMU_TEST_EQ_U64(c, SEMU_OK, nema_tsc6a_create(&surface, &error));
    patterned_asset();
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_load(bus, SOURCE, asset, sizeof(asset), &error));
    for (variant = 0u; variant < 2u; ++variant) {
        /* Both witnessed clips admit the identical full-asset sample set;
         * every panel byte outside the quad stays intact. */
        s.clip_min_y = variant == 0u ? 162u : 193u;
        memset(panel, 0x5a, sizeof(panel));
        SEMU_TEST_EQ_U64(c, SEMU_OK,
            nema_tsc6a_resolve_mask(surface, bus, &s, panel, 480u, &error));
        for (y = 0u; y < 240u; ++y) for (x = 0u; x < 240u; ++x) {
            uint16_t expected = 0x5a5au;
            size_t off = y * 480u + x * 2u;
            if (x >= 200u && x < 220u && y >= 203u && y < 233u) {
                unsigned sx = x - 200u, sy = y - 203u;
                unsigned block = sy / 4u * 5u + sx / 4u;
                expected = colors[block % 2u][(sx % 4u + sy % 4u) % 4u];
            }
            SEMU_TEST_EQ_U64(c, expected, panel[off] | ((uint16_t)panel[off + 1u] << 8u));
        }
    }
    nema_tsc6a_destroy(surface); semu_bus_destroy(bus);
}

static void test_icon_near_misses_refuse(semu_test_context *c)
{
    semu_error error;
    semu_bus *bus = source_bus(sizeof(asset), &error);
    nema_tsc6a *surface = NULL;
    unsigned variant;
    SEMU_TEST_ASSERT(c, bus != NULL);
    SEMU_TEST_EQ_U64(c, SEMU_OK, nema_tsc6a_create(&surface, &error));
    patterned_asset();
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_load(bus, SOURCE, asset, sizeof(asset), &error));
    for (variant = 0u; variant < 16u; ++variant) {
        nema_draw_snapshot s = icon_tuple();
        switch (variant) {
        case 0: s.mm00 = 0x3f7fffffu; break;   /* no scale word on this family */
        case 1: s.mm00 = 0x3f800001u; break;
        case 2: s.mm11 = 0x3f7fffffu; break;
        case 3: s.mm02 = 0xc3480001u; break;   /* one ULP off -200 */
        case 4: s.mm12 = 0xc34b0001u; break;   /* one ULP off -203 */
        case 5: s.mm02 = 0xc34b0000u; break;   /* swapped translations */
        case 6: s.mm01 = 1u; break;
        case 7: s.mm10 = 1u; break;
        case 8: s.clip_min_x = 1u; break;
        case 9: s.clip_min_y = 161u; break;    /* unwitnessed strip */
        case 10: s.clip_min_y = 194u; break;   /* unwitnessed strip */
        case 11: s.clip_max_y = 239u; break;
        case 12: s.draw_color = 0xff555555u; break; /* the 60x60 pair */
        case 13: s.point0_x = s.point3_x = 199u << 16u; break;
        case 14: s.point2_y = s.point3_y = 234u << 16u; break; /* 31-tall rect */
        default: s.src_height = 33u; break;    /* wrong shape */
        }
        memset(panel, 0xa5, sizeof(panel)); memcpy(before, panel, sizeof(panel));
        SEMU_TEST_EQ_U64(c, SEMU_ERR_UNSUPPORTED,
            nema_tsc6a_resolve_mask(surface, bus, &s, panel, 480u, &error));
        SEMU_TEST_ASSERT(c, memcmp(before, panel, sizeof(panel)) == 0);
    }
    nema_tsc6a_destroy(surface); semu_bus_destroy(bus);
}

static void test_icon_invalid_source_refuses(semu_test_context *c)
{
    semu_error error;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot s = icon_tuple();
    unsigned variant;
    SEMU_TEST_EQ_U64(c, SEMU_OK, nema_tsc6a_create(&surface, &error));
    for (variant = 0u; variant < 2u; ++variant) {
        size_t size = sizeof(asset) - variant;
        semu_bus *bus = source_bus(size, &error);
        SEMU_TEST_ASSERT(c, bus != NULL);
        patterned_asset();
        if (variant == 0u) asset[39u * 12u + 11u] = 128u; /* aux bit in the last block */
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
        SEMU_TEST_CASE(test_icon_pixels),
        SEMU_TEST_CASE(test_icon_near_misses_refuse),
        SEMU_TEST_CASE(test_icon_invalid_source_refuses)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
