#include "../../src/display/nema_texture.h"
#include "test.h"

#include "semu/bus.h"
#include "semu/types.h"

#include <string.h>

#define SRAM_BASE 0x10000000u
#define SRAM_SIZE 0x00100000u
#define TEX_BASE (SRAM_BASE + 0x10000u)

static semu_bus *make_bus(semu_error *err)
{
    semu_bus *bus = semu_bus_create(err);
    if (bus != NULL) {
        semu_bus_map_ram(bus, "sram", SRAM_BASE, SRAM_SIZE, err);
    }
    return bus;
}

static void load(semu_bus *bus, uint32_t addr, const uint8_t *data,
                 size_t len, semu_error *err)
{
    semu_bus_load(bus, addr, data, len, err);
}

static void test_rgb565_sample(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    nema_texture_desc d = {0};
    nema_texel t;
    uint8_t buf[4u];

    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);

    /* pixel 0: RGB565 0x0000 (black), pixel 1: 0xFFFF (white) */
    buf[0u] = 0x00u; buf[1u] = 0x00u;
    buf[2u] = 0xFFu; buf[3u] = 0xFFu;
    load(bus, TEX_BASE, buf, 4u, &err);

    d.base = TEX_BASE; d.format = NEMA_TEX_FMT_RGB565;
    d.stride = 4u; d.width = 2u; d.height = 1u;

    SEMU_TEST_ASSERT(context,
        nema_texture_validate(bus, &d, &err) == SEMU_OK);
    SEMU_TEST_ASSERT(context,
        nema_texture_sample(bus, &d, 0u, 0u, &t, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 0u, t.r);
    SEMU_TEST_EQ_U64(context, 0u, t.g);
    SEMU_TEST_EQ_U64(context, 0u, t.b);
    SEMU_TEST_EQ_U64(context, 255u, t.a);
    SEMU_TEST_ASSERT(context,
        nema_texture_sample(bus, &d, 1u, 0u, &t, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 255u, t.r);
    SEMU_TEST_EQ_U64(context, 255u, t.g);
    SEMU_TEST_EQ_U64(context, 255u, t.b);
    semu_bus_destroy(bus);
}

static void test_a2le_sample(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    nema_texture_desc d = {0};
    nema_texel t;
    /* byte 0: samples 0,1,2,3 = 0,1,2,3 → 0b11_10_01_00 = 0xE4 */
    uint8_t buf[1u] = { 0xE4u };

    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    load(bus, TEX_BASE, buf, 1u, &err);

    d.base = TEX_BASE; d.format = NEMA_TEX_FMT_A2LE;
    d.stride = 1u; d.width = 4u; d.height = 1u;

    SEMU_TEST_ASSERT(context,
        nema_texture_validate(bus, &d, &err) == SEMU_OK);
    SEMU_TEST_ASSERT(context,
        nema_texture_sample(bus, &d, 0u, 0u, &t, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 0u, t.a);
    SEMU_TEST_ASSERT(context,
        nema_texture_sample(bus, &d, 1u, 0u, &t, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 85u, t.a);
    SEMU_TEST_ASSERT(context,
        nema_texture_sample(bus, &d, 2u, 0u, &t, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 170u, t.a);
    SEMU_TEST_ASSERT(context,
        nema_texture_sample(bus, &d, 3u, 0u, &t, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 255u, t.a);
    semu_bus_destroy(bus);
}

static void test_a2le_multi_row(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    nema_texture_desc d = {0};
    nema_texel t;
    /* 2 rows, 4 pixels each, stride=1.
     * row0 byte = 0xE4 (0,1,2,3), row1 byte = 0x1B (3,2,1,0) */
    uint8_t buf[2u] = { 0xE4u, 0x1Bu };

    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    load(bus, TEX_BASE, buf, 2u, &err);

    d.base = TEX_BASE; d.format = NEMA_TEX_FMT_A2LE;
    d.stride = 1u; d.width = 4u; d.height = 2u;

    SEMU_TEST_ASSERT(context,
        nema_texture_validate(bus, &d, &err) == SEMU_OK);
    SEMU_TEST_ASSERT(context,
        nema_texture_sample(bus, &d, 0u, 0u, &t, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 0u, t.a);
    SEMU_TEST_ASSERT(context,
        nema_texture_sample(bus, &d, 0u, 1u, &t, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 255u, t.a);
    semu_bus_destroy(bus);
}

static void test_stride_too_small(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    nema_texture_desc d = {0};

    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);

    d.base = TEX_BASE; d.format = NEMA_TEX_FMT_A2LE;
    d.stride = 1u; d.width = 8u; d.height = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_texture_validate(bus, &d, &err));

    d.format = NEMA_TEX_FMT_RGB565;
    d.stride = 2u; d.width = 2u; d.height = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_texture_validate(bus, &d, &err));
    semu_bus_destroy(bus);
}

static void test_out_of_range(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    nema_texture_desc d = {0};
    nema_texel t;
    uint8_t buf[1u] = { 0u };

    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    load(bus, TEX_BASE, buf, 1u, &err);

    d.base = TEX_BASE; d.format = NEMA_TEX_FMT_A2LE;
    d.stride = 1u; d.width = 4u; d.height = 1u;
    SEMU_TEST_ASSERT(context,
        nema_texture_validate(bus, &d, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_texture_sample(bus, &d, 4u, 0u, &t, &err));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_texture_sample(bus, &d, 0u, 1u, &t, &err));
    semu_bus_destroy(bus);
}

static void test_unsupported_format(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    nema_texture_desc d = {0};
    uint8_t buf[4u] = {0};

    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    load(bus, TEX_BASE, buf, 4u, &err);

    d.base = TEX_BASE; d.format = NEMA_TEX_FMT_TSC6A;
    d.stride = 4u; d.width = 2u; d.height = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_texture_validate(bus, &d, &err));

    /* RGBA4444 (0x06) — not evidenced */
    d.format = 0x06u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_texture_validate(bus, &d, &err));
    semu_bus_destroy(bus);
}

static void test_unsupported_sampling(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    nema_texture_desc d = {0};

    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);

    d.base = TEX_BASE; d.format = NEMA_TEX_FMT_RGB565;
    d.sampling = 1u;
    d.stride = 2u; d.width = 1u; d.height = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_texture_validate(bus, &d, &err));

    d.sampling = 0xFFu;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_texture_validate(bus, &d, &err));
    semu_bus_destroy(bus);
}

static void test_bus_range_overflow(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    nema_texture_desc d = {0};

    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);

    /* base near end of SRAM, texture extends past it */
    d.base = SRAM_BASE + SRAM_SIZE - 2u;
    d.format = NEMA_TEX_FMT_RGB565;
    d.stride = 4u; d.width = 2u; d.height = 2u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_texture_validate(bus, &d, &err));
    semu_bus_destroy(bus);
}

static void test_repeat_sample(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    nema_texture_desc d = {0};
    nema_texel t1, t2;
    uint8_t buf[1u] = { 0xE4u };
    size_t i;

    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    load(bus, TEX_BASE, buf, 1u, &err);

    d.base = TEX_BASE; d.format = NEMA_TEX_FMT_A2LE;
    d.stride = 1u; d.width = 4u; d.height = 1u;
    SEMU_TEST_ASSERT(context,
        nema_texture_validate(bus, &d, &err) == SEMU_OK);
    for (i = 0u; i < 4u; ++i) {
        nema_texture_sample(bus, &d, (uint32_t)i, 0u, &t1, &err);
        nema_texture_sample(bus, &d, (uint32_t)i, 0u, &t2, &err);
        SEMU_TEST_EQ_U64(context, t1.a, t2.a);
    }
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_rgb565_sample),
        SEMU_TEST_CASE(test_a2le_sample),
        SEMU_TEST_CASE(test_a2le_multi_row),
        SEMU_TEST_CASE(test_stride_too_small),
        SEMU_TEST_CASE(test_out_of_range),
        SEMU_TEST_CASE(test_unsupported_format),
        SEMU_TEST_CASE(test_unsupported_sampling),
        SEMU_TEST_CASE(test_bus_range_overflow),
        SEMU_TEST_CASE(test_repeat_sample)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
