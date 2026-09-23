#include "../../src/display/nema_backend_internal.h"
#include "../../src/core/bus_internal.h"

#include "semu/hash.h"
#include "test.h"
#include <stdlib.h>
#include <string.h>

/* Compile the same renderer with a deterministic staging allocation failure. */
static int refuse_allocation;
static void *test_malloc(size_t size)
{ return refuse_allocation ? NULL : malloc(size); }
#define malloc test_malloc
#define nema_rgba4444_draw fault_draw
#include "../../src/display/nema_rgba4444.c"
#undef nema_rgba4444_draw
#undef malloc

#define SOURCE 0x10040000u
#define TARGET 0x10080000u
#define LIST 0x100d0000u
#define BYTES 115200u
#define SOURCE_BYTES 46080u

typedef struct {
    semu_bus *bus;
    semu_nema_backend *backend;
    unsigned frames;
    uint8_t before[BYTES];
} fixture;
static void published(void *context, const semu_frame *frame)
{ fixture *f = context; (void)frame; ++f->frames; }
static void put16(uint8_t *p, uint32_t value)
{ p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8u); }
static int pair(fixture *f, uint32_t offset, uint32_t reg, uint32_t value)
{
    semu_error e;
    return semu_bus_write(f->bus, LIST + offset, 4u, reg, &e) == SEMU_OK &&
        semu_bus_write(f->bus, LIST + offset + 4u, 4u, value, &e) == SEMU_OK;
}
static int setup(fixture *f)
{
    semu_error e; unsigned x, y; size_t i;
    uint8_t source[SOURCE_BYTES];
    static const uint32_t words[] = {
        NEMA_REG_TEX0_BASE, TARGET, NEMA_REG_TEX0_FSTRIDE, 0x040001e0u,
        NEMA_REG_TEX0_RESXY, 0x00f000f0u, NEMA_REG_TEX1_BASE, SOURCE,
        NEMA_REG_TEX1_FSTRIDE, 0x060101e0u, NEMA_REG_TEX1_RESXY, 0x006000f0u,
        NEMA_REG_TEX_COLOR, 0xffffffffu, NEMA_REG_IMEM_ADDR, 0u,
        NEMA_REG_IMEM_DATAH, 0x004e0002u, NEMA_REG_IMEM_DATAL, 0x804b1286u,
        NEMA_REG_CLIPMIN, 0u, NEMA_REG_CLIPMAX, 0x006400f0u,
        NEMA_REG_MATMULT, 0u, NEMA_REG_CODEPTR, 0x941e8000u,
        NEMA_REG_DRAW_COLOR, 0xff000000u,
        NEMA_REG_POINT0_X, 0u, NEMA_REG_POINT0_Y, 0u,
        NEMA_REG_POINT1_X, 240u << 16u, NEMA_REG_POINT1_Y, 0u,
        NEMA_REG_POINT2_X, 240u << 16u, NEMA_REG_POINT2_Y, 100u << 16u,
        NEMA_REG_POINT3_X, 0u, NEMA_REG_POINT3_Y, 100u << 16u,
        NEMA_REG_MM00, 0x3f800000u, NEMA_REG_MM01, 0u,
        NEMA_REG_MM02, 0xbe800000u, NEMA_REG_MM10, 0u,
        NEMA_REG_MM11, 0x3f800000u, NEMA_REG_MM12, 0xbf000000u,
        NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD
    };
    memset(f, 0, sizeof(*f));
    f->bus = semu_bus_create(&e); f->backend = semu_nema_backend_create(&e);
    if (!f->bus || !f->backend || semu_bus_map_ram(f->bus, "synthetic",
        0x10000000u, 0x180000u, &e) != SEMU_OK) return 0;
    for (y = 0; y < 240u; ++y) for (x = 0; x < 240u; ++x) {
        put16(f->before + y * 480u + x * 2u,
            ((x % 32u) << 11u) | ((y % 64u) << 5u) | ((x + y) % 32u));
        if (y < 96u) put16(source + y * 480u + x * 2u,
            ((x & 15u) << 12u) | ((y & 15u) << 8u) |
            (((x + y) & 15u) << 4u) | ((3u * x + 7u * y) & 15u));
    }
    if (semu_bus_load(f->bus, SOURCE, source, sizeof(source), &e) != SEMU_OK ||
        semu_surface_write(f->backend->surface, 0, 0, 240, 240,
            f->before, 480u, &e) != SEMU_OK) return 0;
    for (i = 0; i < SEMU_ARRAY_LEN(words); i += 2u)
        if (!pair(f, (uint32_t)i * 4u, words[i], words[i + 1u])) return 0;
    return 1;
}
static void finish(fixture *f)
{ semu_nema_backend_destroy(f->backend); semu_bus_destroy(f->bus); }
static semu_transaction_result submit(fixture *f, semu_error *e)
{ return semu_nema_backend_submit(f->backend, f->bus, LIST, 60u, 0u,
    published, f, e); }
static void test_nema_rgba4444_lane_pixels(semu_test_context *context)
{
    fixture f; semu_error e; uint8_t digest[32]; char hex[65]; unsigned n, i;
    const uint32_t matrix[] = {0x3f7cd6eau, 0x3c4a4533u, 0xbed70a3du,
        0xbda3d70au, 0x3f95c28fu, 0x40466666u};
    const char *expected[] = {
        "f2a0a0e64d568e95741790f4ea55fd07ebca33743e32fa697f2e195f432318b3",
        "52f32d7eaf3e1ec7bdfa6c74ebe760e58dcb0fcebab6f122af3219f26bd5a880"
    };
    /* E-NEMA-RGBA4444-001: independently generated source and lane pixel pin. */
    for (n = 0; n < 4u; ++n) {
        SEMU_TEST_ASSERT(context, setup(&f));
        if (n >= 2u) for (i = 0; i < 6u; ++i)
            SEMU_TEST_ASSERT(context, pair(&f, 184u + 8u * i,
                NEMA_REG_MM00 + 4u * i, matrix[i]));
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, &e));
        semu_sha256(semu_nema_backend_frame(f.backend)->pixels, BYTES, digest);
        semu_sha256_format(digest, hex);
        SEMU_TEST_ASSERT(context, strcmp(hex, expected[n / 2u]) == 0);
        SEMU_TEST_EQ_U64(context, 1u, f.frames);
        finish(&f);
    }
}

static int replace(fixture *f, uint32_t reg, uint32_t value)
{
    uint32_t offset, found; semu_error e;
    for (offset = 0; offset < 240u; offset += 8u) {
        if (semu_bus_read(f->bus, LIST + offset, 4u, &found, &e) != SEMU_OK) return 0;
        if (found == reg) return pair(f, offset, reg, value);
    }
    return 0;
}
static void test_nema_rgba4444_state_refusals(semu_test_context *context)
{
    static const uint32_t invalid[][2] = {
        {NEMA_REG_TEX0_FSTRIDE, 0x040101e0u},
        {NEMA_REG_TEX0_FSTRIDE, 0x040001e2u},
        {NEMA_REG_TEX0_FSTRIDE, 0x050001e0u},
        {NEMA_REG_TEX0_RESXY, 0x00ef00f0u},
        {NEMA_REG_TEX1_FSTRIDE, 0x060001e0u},
        {NEMA_REG_TEX1_FSTRIDE, 0x060101e2u},
        {NEMA_REG_TEX1_RESXY, 0x006100f0u},
        {NEMA_REG_TEX1_RESXY, 0x006000efu},
        {NEMA_REG_TEX1_BASE, TARGET}, {NEMA_REG_TEX1_BASE, TARGET - 1u},
        {NEMA_REG_TEX1_BASE, 0x0fffffffu},
        {NEMA_REG_TEX1_BASE, 0x10180000u - SOURCE_BYTES + 1u},
        {NEMA_REG_TEX0_BASE, 0x10180000u - BYTES + 1u},
        {NEMA_REG_TEX0_BASE, 0xfffffff0u},
        {NEMA_REG_MATMULT, 1u}, {NEMA_REG_CODEPTR, 0x941e8001u},
        {NEMA_REG_IMEM_ADDR, 1u}, {NEMA_REG_IMEM_DATAH, 0u},
        {NEMA_REG_IMEM_DATAL, 0u}, {NEMA_REG_TEX_COLOR, 0xff00ffffu},
        {NEMA_REG_CLIPMIN, 0x00650000u}, {NEMA_REG_CLIPMAX, 0x00f100f0u},
        {NEMA_REG_POINT1_X, 0u}, {NEMA_REG_POINT1_Y, 1u},
        {NEMA_REG_POINT3_X, 1u}, {NEMA_REG_POINT3_Y, 0u},
        {NEMA_REG_MM00, 0x7f800000u}, {NEMA_REG_MM01, 0x7fc00000u},
        {NEMA_REG_MM02, 0xff800000u}, {NEMA_REG_MM10, 0x7f800001u},
        {NEMA_REG_MM11, 0x7f800000u}, {NEMA_REG_MM12, 0x7fc00000u},
        /* Finite matrix that overflows after staging the first two pixels. */
        {NEMA_REG_MM00, 0x7f7fffffu}
    };
    fixture f; semu_error e; size_t i;
    for (i = 0; i < SEMU_ARRAY_LEN(invalid); ++i) {
        SEMU_TEST_ASSERT(context, setup(&f));
        SEMU_TEST_ASSERT(context, replace(&f, invalid[i][0], invalid[i][1]));
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, submit(&f, &e));
        SEMU_TEST_ASSERT(context, e.code != SEMU_OK && e.text[0]);
        SEMU_TEST_EQ_U64(context, 0u, f.frames);
        SEMU_TEST_EQ_U64(context, 0u, nema_state_snapshot_count(f.backend->state));
        SEMU_TEST_ASSERT(context, memcmp(f.before,
            semu_nema_backend_frame(f.backend)->pixels, BYTES) == 0);
        finish(&f);
    }
}
static semu_status device_read(void *context, uint32_t offset,
    unsigned width, uint32_t *value, semu_error *error)
{
    unsigned *reads = context;
    (void)offset; (void)width; (void)error; ++*reads; *value = 0; return SEMU_OK;
}
static void test_nema_rgba4444_memory_and_retry(semu_test_context *context)
{
    const semu_bus_device_ops ops = {device_read, NULL, NULL};
    fixture f; semu_error e; unsigned reads = 0, n;
    uint8_t commands[240], source[SOURCE_BYTES];
    for (n = 0; n < 2u; ++n) {
        SEMU_TEST_ASSERT(context, setup(&f));
        if (n == 0u) {
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_overlay(f.bus,
                "source byte", SOURCE + SOURCE_BYTES - 2u, 1u, &ops, &reads, &e));
        } else {
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_bus_copy_out(f.bus, LIST, commands, sizeof(commands), &e));
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_bus_copy_out(f.bus, SOURCE, source, sizeof(source), &e));
            semu_bus_destroy(f.bus); f.bus = semu_bus_create(&e);
            SEMU_TEST_ASSERT(context, f.bus != NULL);
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_bus_map_ram(f.bus, "list", LIST, 256u, &e));
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_bus_map_ram(f.bus, "partial source", SOURCE, SOURCE_BYTES - 1u, &e));
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_bus_load(f.bus, LIST, commands, sizeof(commands), &e));
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_bus_load(f.bus, SOURCE, source, SOURCE_BYTES - 1u, &e));
        }
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, submit(&f, &e));
        SEMU_TEST_EQ_U64(context, 0u, reads);
        SEMU_TEST_EQ_U64(context, 0u, f.frames);
        SEMU_TEST_ASSERT(context, memcmp(f.before,
            semu_nema_backend_frame(f.backend)->pixels, BYTES) == 0);
        if (n == 0u) {
            semu_bus_unmap_overlay(f.bus, &reads);
            SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, &e));
            SEMU_TEST_EQ_U64(context, 1u, f.frames);
        }
        finish(&f);
    }
}
static void test_nema_rgba4444_later_child_retry(semu_test_context *context)
{
    fixture f; semu_error e;
    semu_display_list lists[] = {{LIST, 60u, 0u}, {LIST + 256u, 4u, 0u}};
    SEMU_TEST_ASSERT(context, setup(&f));
    SEMU_TEST_ASSERT(context, pair(&f, 256u, NEMA_REG_CODEPTR, 0x941e8001u));
    SEMU_TEST_ASSERT(context, pair(&f, 264u, NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        semu_nema_backend_ops.prepare(f.backend, f.bus, lists, 2u, 0u,
            published, &f, &e));
    SEMU_TEST_EQ_U64(context, 0u, f.frames);
    SEMU_TEST_EQ_U64(context, 0u, nema_state_snapshot_count(f.backend->state));
    SEMU_TEST_ASSERT(context, memcmp(f.before,
        semu_nema_backend_frame(f.backend)->pixels, BYTES) == 0);
    SEMU_TEST_ASSERT(context, pair(&f, 256u, NEMA_REG_CODEPTR, 0x941e8000u));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        semu_nema_backend_ops.prepare(f.backend, f.bus, lists, 2u, 0u,
            published, &f, &e));
    semu_nema_backend_ops.commit(f.backend);
    SEMU_TEST_EQ_U64(context, 2u, f.frames);
    SEMU_TEST_EQ_U64(context, 2u, nema_state_snapshot_count(f.backend->state));
    finish(&f);
}


static void capture(void *context, const nema_draw_snapshot *snapshot)
{ *(nema_draw_snapshot *)context = *snapshot; }
static void test_nema_rgba4444_direct_atomicity(semu_test_context *context)
{
    fixture f; semu_error e; nema_draw_snapshot s = {0}, changed; nema_state *state;
    uint8_t pixels[BYTES]; uint32_t offset; nema_record record;
    SEMU_TEST_ASSERT(context, setup(&f));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_state_create(&state, &e));
    for (offset = 0; offset < 240u; offset += 8u) {
        memset(&record, 0, sizeof(record));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_read(f.bus, LIST + offset, 4u, &record.reg_offset, &e));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_read(f.bus, LIST + offset + 4u, 4u, &record.value, &e));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            nema_state_record(state, &record, capture, &s, &e));
    }
    nema_state_destroy(state);
    memcpy(pixels, f.before, BYTES);
    refuse_allocation = 1;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_NOMEM, fault_draw(f.bus, &s, pixels, 480u, &e));
    refuse_allocation = 0;
    SEMU_TEST_ASSERT(context, memcmp(pixels, f.before, BYTES) == 0);
    changed = s; changed.mm00 = 0x7f7fffffu;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        fault_draw(f.bus, &changed, pixels, 480u, NULL));
    SEMU_TEST_ASSERT(context, memcmp(pixels, f.before, BYTES) == 0);
    changed = s; changed.point0_x = 0xfbff0000u; changed.point3_x = changed.point0_x;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        fault_draw(f.bus, &changed, pixels, 480u, &e));
    changed = s; changed.point0_x = changed.point1_x; changed.point3_x = changed.point0_x;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        fault_draw(f.bus, &changed, pixels, 480u, &e));
    /* Clipping the signed fractional edge produces the identical visible draw. */
    changed = s; changed.point0_x = 0xffffffffu; changed.point3_x = changed.point0_x;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        fault_draw(f.bus, &changed, pixels, 480u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, &e));
    SEMU_TEST_ASSERT(context, memcmp(pixels,
        semu_nema_backend_frame(f.backend)->pixels, BYTES) == 0);
    /* Entirely clipped rectangles are no-ops after source validation. */
    changed = s; changed.clip_min_y = 100u;
    memcpy(pixels, f.before, BYTES);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        fault_draw(f.bus, &changed, pixels, 480u, &e));
    SEMU_TEST_ASSERT(context, memcmp(pixels, f.before, BYTES) == 0);
    finish(&f);
}


static void test_nema_rgba4444_missing_program(semu_test_context *context)
{
    const uint32_t required[] = {NEMA_REG_MATMULT, NEMA_REG_CODEPTR,
        NEMA_REG_IMEM_ADDR, NEMA_REG_IMEM_DATAH, NEMA_REG_IMEM_DATAL,
        NEMA_REG_MM00, NEMA_REG_MM01, NEMA_REG_MM02,
        NEMA_REG_MM10, NEMA_REG_MM11, NEMA_REG_MM12};
    fixture f; semu_error e; size_t i; uint32_t offset, reg;
    for (i = 0; i < SEMU_ARRAY_LEN(required); ++i) {
        SEMU_TEST_ASSERT(context, setup(&f));
        for (offset = 0; offset < 232u; offset += 8u) {
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_bus_read(f.bus, LIST + offset, 4u, &reg, &e));
            if (reg == required[i]) {
                SEMU_TEST_ASSERT(context, pair(&f, offset, NEMA_REG_DRAW_COLOR, 0u));
                break;
            }
        }
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, submit(&f, &e));
        SEMU_TEST_EQ_U64(context, 0u, f.frames);
        SEMU_TEST_ASSERT(context, memcmp(f.before,
            semu_nema_backend_frame(f.backend)->pixels, BYTES) == 0);
        finish(&f);
    }
}


static void test_nema_rgba4444_inherited_source_clear(semu_test_context *context)
{
    fixture f; semu_error e; size_t i; const uint8_t *pixels;
    SEMU_TEST_ASSERT(context, setup(&f));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, &e));
    /* E-NEMA-LISTS-001: the known black clear wins over inherited TEX1. */
    SEMU_TEST_ASSERT(context, pair(&f, 256u, NEMA_REG_TEX0_BASE, 0x1011cf40u));
    SEMU_TEST_ASSERT(context, pair(&f, 264u, NEMA_REG_MATMULT, 0x90000000u));
    SEMU_TEST_ASSERT(context, pair(&f, 272u, NEMA_REG_CODEPTR, 0x941eb400u));
    SEMU_TEST_ASSERT(context, pair(&f, 280u, NEMA_REG_DRAW_COLOR, 0xff000000u));
    SEMU_TEST_ASSERT(context, pair(&f, 288u, NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, semu_nema_backend_submit(
        f.backend, f.bus, LIST + 256u, 10u, 0u, published, &f, &e));
    pixels = semu_nema_backend_frame(f.backend)->pixels;
    for (i = 0; i < 48000u; ++i) SEMU_TEST_EQ_U64(context, 0u, pixels[i]);
    SEMU_TEST_ASSERT(context, memcmp(pixels + 48000u, f.before + 48000u,
        BYTES - 48000u) == 0);
    SEMU_TEST_EQ_U64(context, 2u, f.frames);
    finish(&f);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_nema_rgba4444_lane_pixels),
        SEMU_TEST_CASE(test_nema_rgba4444_state_refusals),
        SEMU_TEST_CASE(test_nema_rgba4444_memory_and_retry),
        SEMU_TEST_CASE(test_nema_rgba4444_later_child_retry),
        SEMU_TEST_CASE(test_nema_rgba4444_direct_atomicity),
        SEMU_TEST_CASE(test_nema_rgba4444_missing_program),
        SEMU_TEST_CASE(test_nema_rgba4444_inherited_source_clear)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
