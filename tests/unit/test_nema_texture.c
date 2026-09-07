#include "../../src/display/nema_texture.h"
#include "../../src/core/bus_internal.h"
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
    semu_error err; semu_bus *bus = make_bus(&err);
    nema_texture_desc d = {0};
    nema_texel t;
    uint8_t buf[4u];
    SEMU_TEST_ASSERT(context, bus != NULL);
    /* pixel 0: RGB565 0x0000 (black), pixel 1: 0xFFFF (white) */
    buf[0u] = 0x00u; buf[1u] = 0x00u;
    buf[2u] = 0xFFu; buf[3u] = 0xFFu;
    load(bus, TEX_BASE, buf, 4u, &err);
    d.base = TEX_BASE; d.format = NEMA_TEX_FMT_RGB565;
    d.stride = 4u; d.width = 2u; d.height = 1u;
    SEMU_TEST_ASSERT(context, nema_texture_validate(bus, &d, &err) == SEMU_OK);
    SEMU_TEST_ASSERT(context, nema_texture_sample(bus, &d, 0u, 0u, &t, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 0u, t.r);
    SEMU_TEST_EQ_U64(context, 0u, t.g);
    SEMU_TEST_EQ_U64(context, 0u, t.b);
    SEMU_TEST_EQ_U64(context, 255u, t.a);
    SEMU_TEST_ASSERT(context, nema_texture_sample(bus, &d, 1u, 0u, &t, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 255u, t.r);
    SEMU_TEST_EQ_U64(context, 255u, t.g);
    SEMU_TEST_EQ_U64(context, 255u, t.b);
    semu_bus_destroy(bus);
}

static void test_a2le_sample(semu_test_context *context)
{
    semu_error err; semu_bus *bus = make_bus(&err);
    nema_texture_desc d = {0};
    nema_texel t;
    /* byte 0: samples 0,1,2,3 = 0,1,2,3 → 0b11_10_01_00 = 0xE4 */
    uint8_t buf[1u] = { 0xE4u };
    SEMU_TEST_ASSERT(context, bus != NULL);
    load(bus, TEX_BASE, buf, 1u, &err);
    d.base = TEX_BASE; d.format = NEMA_TEX_FMT_A2LE;
    d.stride = 1u; d.width = 4u; d.height = 1u;
    SEMU_TEST_ASSERT(context, nema_texture_validate(bus, &d, &err) == SEMU_OK);
    SEMU_TEST_ASSERT(context, nema_texture_sample(bus, &d, 0u, 0u, &t, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 0u, t.a);
    SEMU_TEST_ASSERT(context, nema_texture_sample(bus, &d, 1u, 0u, &t, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 85u, t.a);
    SEMU_TEST_ASSERT(context, nema_texture_sample(bus, &d, 2u, 0u, &t, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 170u, t.a);
    SEMU_TEST_ASSERT(context, nema_texture_sample(bus, &d, 3u, 0u, &t, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 255u, t.a);
    semu_bus_destroy(bus);
}

static void test_a2le_multi_row(semu_test_context *context)
{
    semu_error err; semu_bus *bus = make_bus(&err);
    nema_texture_desc d = {0};
    nema_texel t;
    /* 2 rows, 4 pixels each, stride=1.
     * row0 byte = 0xE4 (0,1,2,3), row1 byte = 0x1B (3,2,1,0) */
    uint8_t buf[2u] = { 0xE4u, 0x1Bu };
    SEMU_TEST_ASSERT(context, bus != NULL);
    load(bus, TEX_BASE, buf, 2u, &err);
    d.base = TEX_BASE; d.format = NEMA_TEX_FMT_A2LE;
    d.stride = 1u; d.width = 4u; d.height = 2u;
    SEMU_TEST_ASSERT(context, nema_texture_validate(bus, &d, &err) == SEMU_OK);
    SEMU_TEST_ASSERT(context, nema_texture_sample(bus, &d, 0u, 0u, &t, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 0u, t.a);
    SEMU_TEST_ASSERT(context, nema_texture_sample(bus, &d, 0u, 1u, &t, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 255u, t.a);
    semu_bus_destroy(bus);
}

static void test_stride_too_small(semu_test_context *context)
{
    semu_error err; semu_bus *bus = make_bus(&err);
    nema_texture_desc d = {0};
    SEMU_TEST_ASSERT(context, bus != NULL);
    d.base = TEX_BASE; d.format = NEMA_TEX_FMT_A2LE;
    d.stride = 1u; d.width = 8u; d.height = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, nema_texture_validate(bus, &d, &err));
    d.format = NEMA_TEX_FMT_RGB565;
    d.stride = 2u; d.width = 2u; d.height = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, nema_texture_validate(bus, &d, &err));
    semu_bus_destroy(bus);
}

static void test_out_of_range(semu_test_context *context)
{
    semu_error err; semu_bus *bus = make_bus(&err);
    nema_texture_desc d = {0};
    nema_texel t;
    uint8_t buf[1u] = { 0u };
    SEMU_TEST_ASSERT(context, bus != NULL);
    load(bus, TEX_BASE, buf, 1u, &err);
    d.base = TEX_BASE; d.format = NEMA_TEX_FMT_A2LE;
    d.stride = 1u; d.width = 4u; d.height = 1u;
    SEMU_TEST_ASSERT(context, nema_texture_validate(bus, &d, &err) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, nema_texture_sample(bus, &d, 4u, 0u, &t, &err));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, nema_texture_sample(bus, &d, 0u, 1u, &t, &err));
    semu_bus_destroy(bus);
}

static void test_unsupported_format(semu_test_context *context)
{
    semu_error err; semu_bus *bus = make_bus(&err);
    nema_texture_desc d = {0};
    uint8_t buf[4u] = {0};
    SEMU_TEST_ASSERT(context, bus != NULL);
    load(bus, TEX_BASE, buf, 4u, &err);
    d.base = TEX_BASE; d.format = NEMA_TEX_FMT_TSC6A;
    d.stride = 4u; d.width = 2u; d.height = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, nema_texture_validate(bus, &d, &err));
    /* RGBA4444 (0x06) — not evidenced */
    d.format = 0x06u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, nema_texture_validate(bus, &d, &err));
    semu_bus_destroy(bus);
}

static void test_sampling_modes(semu_test_context *context)
{
    semu_error err; semu_bus *bus = make_bus(&err);
    nema_texture_desc d = {0};
    SEMU_TEST_ASSERT(context, bus != NULL);
    d.base = TEX_BASE; d.format = NEMA_TEX_FMT_RGB565;
    d.sampling = 1u;
    d.stride = 2u; d.width = 1u; d.height = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        nema_texture_validate(bus, &d, &err));
    d.sampling = 0xFFu;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, nema_texture_validate(bus, &d, &err));
    semu_bus_destroy(bus);
}

static void test_a2le_address_overflow(semu_test_context *context)
{
    semu_error err; semu_bus *bus = make_bus(&err);
    uint8_t alpha = 0u;
    SEMU_TEST_ASSERT(context, bus != NULL);
    /* stride*4 must be checked before the row-width comparison. */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_a2le_sample(bus, TEX_BASE, 0x40000001u, 1u, 1u,
                         0u, 0u, &alpha, &err));
    /* y*stride must not wrap into the beginning of the bus image. */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_a2le_sample(bus, TEX_BASE, 0x10000000u, 1u, 17u,
                         0u, 16u, &alpha, &err));
    semu_bus_destroy(bus);
}

static void test_bus_range_overflow(semu_test_context *context)
{
    semu_error err; semu_bus *bus = make_bus(&err);
    nema_texture_desc d = {0};
    SEMU_TEST_ASSERT(context, bus != NULL);
    /* base near end of SRAM, texture extends past it */
    d.base = SRAM_BASE + SRAM_SIZE - 2u;
    d.format = NEMA_TEX_FMT_RGB565;
    d.stride = 4u; d.width = 2u; d.height = 2u;
    /* Preserve the original mapped-memory error, not the old generic wrapper. */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, nema_texture_validate(bus, &d, &err));
    semu_bus_destroy(bus);
}

static void test_repeat_sample(semu_test_context *context)
{
    semu_error err; semu_bus *bus = make_bus(&err);
    nema_texture_desc d = {0};
    nema_texel t1, t2;
    uint8_t buf[1u] = { 0xE4u };
    size_t i;
    SEMU_TEST_ASSERT(context, bus != NULL);
    load(bus, TEX_BASE, buf, 1u, &err);
    d.base = TEX_BASE; d.format = NEMA_TEX_FMT_A2LE;
    d.stride = 1u; d.width = 4u; d.height = 1u;
    SEMU_TEST_ASSERT(context, nema_texture_validate(bus, &d, &err) == SEMU_OK);
    for (i = 0u; i < 4u; ++i) {
        nema_texture_sample(bus, &d, (uint32_t)i, 0u, &t1, &err);
        nema_texture_sample(bus, &d, (uint32_t)i, 0u, &t2, &err);
        SEMU_TEST_EQ_U64(context, t1.a, t2.a);
    }
    semu_bus_destroy(bus);
}

static semu_status device_read(void *context, uint32_t offset, unsigned width,
    uint32_t *value, semu_error *error)
{
    unsigned *reads = context;
    (void)offset; (void)width; ++*reads; *value = 255u;
    semu_error_clear(error); return SEMU_OK;
}
static void memory_only_case(semu_test_context *context, unsigned mode)
{
    const semu_bus_device_ops ops = {device_read, NULL, NULL};
    const nema_texel sentinel = {1u, 2u, 3u, 4u};
    nema_texel t = sentinel; uint8_t alpha = 7u; unsigned reads = 0u;
    semu_error e; semu_bus *bus = make_bus(&e); semu_status status;
    nema_texture_desc d = {TEX_BASE, mode < 3u ? NEMA_TEX_FMT_RGB565 : NEMA_TEX_FMT_A2LE,
        0u, mode < 3u ? 2u : 1u, 1u, 2u};
    uint32_t offset = mode == 0u ? 3u : (mode == 2u || mode == 4u ? 1u : 0u);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_texture_validate(bus, &d, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_overlay(bus, "device-byte",
        TEX_BASE + offset, 1u, &ops, &reads, &e));
    if (mode == 0u) status = nema_texture_validate(bus, &d, &e);
    else if (mode < 4u) status = nema_texture_sample(bus, &d, 0u, 0u, &t, &e);
    else status = nema_a2le_sample_bilinear(bus, TEX_BASE, 1u, 1u, 2u, 0u, 128u, &alpha, &e);
    SEMU_TEST_EQ_U64(context, 0u, reads);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, status);
    SEMU_TEST_EQ_U64(context, status, e.code);
    SEMU_TEST_ASSERT(context, strcmp(e.text, "copy is not within mapped memory") == 0);
    SEMU_TEST_ASSERT(context, memcmp(&sentinel, &t, sizeof(t)) == 0);
    SEMU_TEST_EQ_U64(context, 7u, alpha);
    semu_bus_unmap_overlay(bus, &reads);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_texture_validate(bus, &d, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_texture_sample(bus, &d, 0u, 0u, &t, &e));
    SEMU_TEST_EQ_U64(context, mode < 3u ? 255u : 0u, t.a);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_a2le_sample_bilinear(
        bus, TEX_BASE, 1u, 1u, 2u, 0u, 128u, &alpha, &e));
    SEMU_TEST_EQ_U64(context, 0u, alpha);
    SEMU_TEST_EQ_U64(context, 0u, reads);
    semu_bus_destroy(bus);
}
static void test_validation_no_mmio(semu_test_context *c) { memory_only_case(c, 0u); }
static void test_rgb_low_no_mmio(semu_test_context *c) { memory_only_case(c, 1u); }
static void test_rgb_high_no_mmio(semu_test_context *c) { memory_only_case(c, 2u); }
static void test_a2le_no_mmio(semu_test_context *c) { memory_only_case(c, 3u); }
static void test_bilinear_no_mmio(semu_test_context *c) { memory_only_case(c, 4u); }
static void test_unmapped_a2le_preserves_texel(semu_test_context *context)
{
    const nema_texel sentinel = {1u, 2u, 3u, 4u}; nema_texel t = sentinel;
    semu_error e; semu_bus *bus = make_bus(&e);
    nema_texture_desc d = {0x40000000u, NEMA_TEX_FMT_A2LE, 0u, 1u, 1u, 1u};
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_ASSERT(context, nema_texture_sample(bus, &d, 0u, 0u, &t, NULL) != SEMU_OK);
    SEMU_TEST_ASSERT(context, memcmp(&sentinel, &t, sizeof(t)) == 0);
    semu_bus_destroy(bus);
}
static void test_rom_and_adjacent_memory(semu_test_context *context)
{
    const uint8_t bytes[] = {0u, 0xf8u, 0xe4u, 0x1bu};
    semu_error e; semu_bus *bus = semu_bus_create(&e); nema_texel t; uint8_t alpha;
    nema_texture_desc d = {TEX_BASE, NEMA_TEX_FMT_RGB565, 0u, 2u, 1u, 1u};
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_rom(bus, "lo", TEX_BASE, bytes, 1u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_rom(bus, "hi", TEX_BASE + 1u, bytes + 1u, 3u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_texture_validate(bus, &d, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_texture_sample(bus, &d, 0u, 0u, &t, &e));
    SEMU_TEST_EQ_U64(context, 255u, t.r);
    SEMU_TEST_EQ_U64(context, 0u, t.g);
    SEMU_TEST_EQ_U64(context, 0u, t.b);
    SEMU_TEST_EQ_U64(context, 255u, t.a);
    d = (nema_texture_desc){TEX_BASE + 2u, NEMA_TEX_FMT_A2LE, 0u, 1u, 4u, 2u};
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_texture_validate(bus, &d, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_texture_sample(bus, &d, 1u, 0u, &t, &e));
    SEMU_TEST_EQ_U64(context, 85u, t.a);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_a2le_sample_bilinear(
        bus, d.base, 1u, 4u, 2u, 128u, 128u, &alpha, &e));
    SEMU_TEST_EQ_U64(context, 128u, alpha);
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
        SEMU_TEST_CASE(test_sampling_modes),
        SEMU_TEST_CASE(test_a2le_address_overflow),
        SEMU_TEST_CASE(test_bus_range_overflow),
        SEMU_TEST_CASE(test_repeat_sample),
        SEMU_TEST_CASE(test_validation_no_mmio),
        SEMU_TEST_CASE(test_rgb_low_no_mmio),
        SEMU_TEST_CASE(test_rgb_high_no_mmio),
        SEMU_TEST_CASE(test_a2le_no_mmio),
        SEMU_TEST_CASE(test_bilinear_no_mmio),
        SEMU_TEST_CASE(test_unmapped_a2le_preserves_texel),
        SEMU_TEST_CASE(test_rom_and_adjacent_memory)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
