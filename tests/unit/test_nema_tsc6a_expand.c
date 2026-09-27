/*
 * Unit tests for the pure TSC6A (format 0x17) block expansion (ticket 793).
 *
 * Synthetic cases are built as literal 12-byte blocks in this file with
 * hand-computed expectations from the E-RE-SAP235-TSC6A-001 law; they run
 * unconditionally and contain no firmware bytes.  The capture-golden case
 * follows the env-fixture pattern of test_nema_corpus.c: it expands the
 * 2700-byte refused-draw fixture named by SEMU_TSC6A_FIXTURE and compares
 * all 225 blocks (60x60 RGBA8888, 14400 B) byte-for-byte against the
 * twice-reproduced reference named by SEMU_TSC6A_REFERENCE.  It skips
 * cleanly when either variable is unset.
 */
#include "../../src/display/nema_tsc6a_internal.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIXTURE_BYTES 2700u
#define REFERENCE_BYTES 14400u

/* Expected RGBA8888 texel. */
typedef struct { uint8_t r, g, b, a; } expect_texel;

/* Every pixel in out[first, first+count) equals one texel. */
static void expect_fill(semu_test_context *context, const uint8_t out[16][4],
                        unsigned first, unsigned count, expect_texel e)
{
    unsigned i;
    for (i = 0u; i < count; ++i) {
        unsigned p = first + i;
        SEMU_TEST_EQ_U64(context, e.r, out[p][0]);
        SEMU_TEST_EQ_U64(context, e.g, out[p][1]);
        SEMU_TEST_EQ_U64(context, e.b, out[p][2]);
        SEMU_TEST_EQ_U64(context, e.a, out[p][3]);
    }
}

/* Pixel p in out[first, first+count) equals table[p % 4] (raster index
 * p = 4*r + c keeps the column pattern constant along each row). */
static void expect_table(semu_test_context *context, const uint8_t out[16][4],
                         const expect_texel table[4], unsigned first,
                         unsigned count)
{
    unsigned i;
    for (i = 0u; i < count; ++i) {
        unsigned p = first + i;
        SEMU_TEST_EQ_U64(context, table[p % 4u].r, out[p][0]);
        SEMU_TEST_EQ_U64(context, table[p % 4u].g, out[p][1]);
        SEMU_TEST_EQ_U64(context, table[p % 4u].b, out[p][2]);
        SEMU_TEST_EQ_U64(context, table[p % 4u].a, out[p][3]);
    }
}

/* E0 = 0xEFE2 -> R=14*17=238, G=15*17=255, B=14*17=238 (nibble 0 is A,
 * ignored); E1 = 0x02B0 -> (0,34,187); alpha 0x7FF.  Pixels 0..3 carry
 * indices 0,1,2,3 in raster order; the rest are index 0. */
static const uint8_t k_block_grad[12] = {
    0xE4u, 0x00u, 0x00u, 0x00u,        /* sixteen 2-bit indices            */
    0xE2u, 0xEFu,                      /* E0 little-endian                 */
    0xB0u, 0x02u,                      /* E1 little-endian                 */
    0xFFu, 0x07u,                      /* alpha = 0x7FF                    */
    0x00u, 0x00u                       /* auxiliary bits 75..95 = 0        */
};

/* idx0 = E0 = (238,255,238); idx1 = (2*E0+E1+1)/3 = (159,181,221);
 * idx2 = (E0+2*E1+1)/3 = (79,108,204); idx3 = E1 = (0,34,187); a = 255.
 * idx1 red (2*238+0=476, plain floor 158) and idx2 green (255+2*34=323,
 * plain floor 107) are exactly the two steps where the pinned +1 rounding
 * differs from plain floor division, in both weight directions. */
static const expect_texel k_grad[4] = {
    { 238u, 255u, 238u, 255u },
    { 159u, 181u, 221u, 255u },
    {  79u, 108u, 204u, 255u },
    {   0u,  34u, 187u, 255u }
};

static void test_gradient_and_uniform_blocks(semu_test_context *context)
{
    uint8_t out[16][4];

    /* Mixed indices 0,1,2,3 on the first raster row, index 0 elsewhere. */
    SEMU_TEST_EQ_U64(context, 1u, tsc6a_expand_block(k_block_grad, out));
    expect_table(context, out, k_grad, 0u, 4u);
    expect_fill(context, out, 4u, 12u, k_grad[0]);

    /* Uniform index bytes 0x00/0x55/0xAA/0xFF repeat one color. */
    {
        static const uint8_t uniform[4] = { 0x00u, 0x55u, 0xAAu, 0xFFu };
        unsigned k;
        for (k = 0u; k < 4u; ++k) {
            uint8_t blk[12];
            memcpy(blk, k_block_grad, sizeof(blk));
            blk[0] = uniform[k];
            blk[1] = uniform[k];
            blk[2] = uniform[k];
            blk[3] = uniform[k];
            SEMU_TEST_EQ_U64(context, 1u, tsc6a_expand_block(blk, out));
            expect_fill(context, out, 0u, 16u, k_grad[k]);
        }
    }
}

/* Index word bytes 00 55 AA FF make block row r entirely index r, proving
 * pixel order p = 4*r + c with row 0 first. */
static void test_raster_pixel_order(semu_test_context *context)
{
    uint8_t blk[12];
    uint8_t out[16][4];
    memcpy(blk, k_block_grad, sizeof(blk));
    blk[0] = 0x00u;
    blk[1] = 0x55u;
    blk[2] = 0xAAu;
    blk[3] = 0xFFu;
    SEMU_TEST_EQ_U64(context, 1u, tsc6a_expand_block(blk, out));
    expect_fill(context, out, 0u, 4u, k_grad[0]);   /* row 0: idx0 */
    expect_fill(context, out, 4u, 4u, k_grad[1]);    /* row 1: idx1 */
    expect_fill(context, out, 8u, 4u, k_grad[2]);    /* row 2: idx2 */
    expect_fill(context, out, 12u, 4u, k_grad[3]);   /* row 3: idx3 */
}

static void test_alpha_and_refusals(semu_test_context *context)
{
    uint8_t blk[12];
    uint8_t out[16][4];
    memcpy(blk, k_block_grad, sizeof(blk));

    /* alpha 0x000 -> texel alpha 0; colors unchanged. */
    blk[8] = 0x00u;
    blk[9] = 0x00u;
    SEMU_TEST_EQ_U64(context, 1u, tsc6a_expand_block(blk, out));
    SEMU_TEST_EQ_U64(context, 0u, out[0][3]);
    SEMU_TEST_EQ_U64(context, 238u, out[0][0]);
    SEMU_TEST_EQ_U64(context, 0u, out[15][3]);

    /* alpha 1024 (0x400): 1024*255/2047 = 127 (integer floor). */
    blk[8] = 0x00u;
    blk[9] = 0x04u;
    SEMU_TEST_EQ_U64(context, 1u, tsc6a_expand_block(blk, out));
    SEMU_TEST_EQ_U64(context, 127u, out[0][3]);

    /* alpha 2047 (0x7FF): 2047*255/2047 = 255. */
    blk[8] = 0xFFu;
    blk[9] = 0x07u;
    SEMU_TEST_EQ_U64(context, 1u, tsc6a_expand_block(blk, out));
    SEMU_TEST_EQ_U64(context, 255u, out[0][3]);

    /* Auxiliary refusals: lowest aux bit (bit 75 = byte 9 bit 3),
     * bit 80 = byte 10 low bit, bit 95 = byte 11 high bit.  Refusal is
     * fail-closed: return 0 with the output left exactly as found. */
    {
        static const unsigned aux_bytes[3] = { 9u, 10u, 11u };
        static const uint8_t aux_masks[3] = { 0x08u, 0x01u, 0x80u };
        unsigned k;
        for (k = 0u; k < 3u; ++k) {
            unsigned p;
            memcpy(blk, k_block_grad, sizeof(blk));
            blk[aux_bytes[k]] |= aux_masks[k];
            memset(out, 0xA5u, sizeof(out));
            SEMU_TEST_EQ_U64(context, 0u, tsc6a_expand_block(blk, out));
            for (p = 0u; p < 16u; ++p) {
                SEMU_TEST_EQ_U64(context, 0xA5u, out[p][0]);
                SEMU_TEST_EQ_U64(context, 0xA5u, out[p][1]);
                SEMU_TEST_EQ_U64(context, 0xA5u, out[p][2]);
                SEMU_TEST_EQ_U64(context, 0xA5u, out[p][3]);
            }
        }
    }
}

/* --- capture golden (env fixture, test_nema_corpus.c skip precedent) --- */

static int read_exact(const char *path, uint8_t *buf, size_t size)
{
    FILE *f = fopen(path, "rb");
    size_t got;
    int extra;
    if (f == NULL) {
        return 0;
    }
    got = fread(buf, 1u, size, f);
    if (got != size) {
        fclose(f);
        return 0;
    }
    extra = fgetc(f);
    fclose(f);
    return extra == EOF; /* reject files longer than the expected size */
}

static void test_capture_golden_expansion(semu_test_context *context)
{
    static uint8_t fixture[FIXTURE_BYTES];
    static uint8_t reference[REFERENCE_BYTES];
    static uint8_t expanded[REFERENCE_BYTES];
    unsigned by, bx, py, px;
    const char *fixture_path = getenv("SEMU_TSC6A_FIXTURE");
    const char *reference_path = getenv("SEMU_TSC6A_REFERENCE");

    if (fixture_path == NULL || fixture_path[0] == '\0' ||
        reference_path == NULL || reference_path[0] == '\0') {
        /* Env-fixture pattern: skip cleanly when private inputs are absent. */
        return;
    }
    SEMU_TEST_ASSERT(context, read_exact(fixture_path, fixture,
                                         FIXTURE_BYTES));
    SEMU_TEST_ASSERT(context, read_exact(reference_path, reference,
                                         REFERENCE_BYTES));
    memset(expanded, 0x5Au, sizeof(expanded));
    for (by = 0u; by < 15u; ++by) {
        for (bx = 0u; bx < 15u; ++bx) {
            uint8_t texels[16][4];
            const uint8_t *block = fixture + (size_t)by * 180u +
                                   (size_t)bx * 12u;
            SEMU_TEST_ASSERT(context, tsc6a_expand_block(block, texels) == 1);
            for (py = 0u; py < 4u; ++py) {
                for (px = 0u; px < 4u; ++px) {
                    unsigned x = bx * 4u + px;
                    unsigned y = by * 4u + py;
                    memcpy(expanded + ((size_t)y * 60u + x) * 4u,
                           texels[py * 4u + px], 4u);
                }
            }
        }
    }
    SEMU_TEST_ASSERT(context,
                     memcmp(expanded, reference, REFERENCE_BYTES) == 0);
}

/* --- resolve-path acceptance with a synthetic bus (ticket 793) ---
 * The tuple is E-EMU-SAP235-MAIN-TSC6A-001; the asset bytes are synthetic
 * all-white opaque blocks, never firmware bytes. */

#define SRC_BASE UINT32_C(0x100a490c)
#define TARGET_BASE UINT32_C(0x10121d40)
#define SRAM_TEST_BASE UINT32_C(0x10000000)

static uint8_t panel[480u * 240u];
static uint8_t panel_before[sizeof(panel)];
static uint8_t asset[FIXTURE_BYTES];

static semu_bus *make_sram_bus(semu_error *error)
{
    semu_bus *bus = semu_bus_create(error);
    if (bus != NULL && semu_bus_map_ram(bus, "sram", SRAM_TEST_BASE,
                                        UINT32_C(0x200000),
                                        error) != SEMU_OK) {
        semu_bus_destroy(bus);
        return NULL;
    }
    return bus;
}

static nema_draw_snapshot compressed_tuple(void)
{
    nema_draw_snapshot s;
    memset(&s, 0, sizeof(s));
    s.src_present = 1u;
    s.src_base = SRC_BASE;
    s.src_format = NEMA_FMT_TSC6A;
    s.src_sampling = 1u;
    s.src_stride = 180u;
    s.src_width = 60u;
    s.src_height = 60u;
    s.target_base = TARGET_BASE;
    s.target_format = NEMA_FMT_RGB565;
    s.target_stride = 480u;
    s.target_width = 240u;
    s.target_height = 240u;
    s.clip_min_x = 0u;
    s.clip_min_y = 81u;
    s.clip_max_x = 240u;
    s.clip_max_y = 162u;
    s.draw_cmd = NEMA_DRAW_QUAD;
    s.draw_color = UINT32_C(0xff555555);
    s.tex_color = UINT32_C(0xffffffff);
    s.matmult = 0u;
    s.codeptr = UINT32_C(0x941e8000);
    s.imem_addr = 0u;
    s.imem_datah = UINT32_C(0x004e0002);
    s.imem_datal = UINT32_C(0x804b1286);
    s.matrix_present = 1u;
    s.mm00 = UINT32_C(0x3f800000);
    s.mm01 = 0u;
    s.mm02 = UINT32_C(0xc32b0001);
    s.mm10 = 0u;
    s.mm11 = UINT32_C(0x3f800000);
    s.mm12 = UINT32_C(0xc2b40000);
    /* Axis-aligned quad (171,90)-(231,150) in signed 16.16 form. */
    s.point0_x = s.point3_x = UINT32_C(171) << 16u;
    s.point0_y = s.point1_y = UINT32_C(90) << 16u;
    s.point1_x = s.point2_x = UINT32_C(231) << 16u;
    s.point2_y = s.point3_y = UINT32_C(150) << 16u;
    return s;
}

/* Every block: all sixteen pixels index 3, E0 = E1 = 0xFFFF (opaque
 * white through the law), alpha 0x7FF, auxiliary zero. */
static void fill_white_asset(void)
{
    static const uint8_t block[12] = {
        0xFFu, 0xFFu, 0xFFu, 0xFFu,
        0xFFu, 0xFFu, 0xFFu, 0xFFu,
        0xFFu, 0x07u, 0x00u, 0x00u
    };
    unsigned i;
    for (i = 0u; i < FIXTURE_BYTES / 12u; ++i) {
        memcpy(asset + i * 12u, block, 12u);
    }
}

static uint16_t panel_pixel(unsigned x, unsigned y)
{
    size_t off = (size_t)y * 480u + (size_t)x * 2u;
    return (uint16_t)(panel[off] | ((uint16_t)panel[off + 1u] << 8u));
}

static void test_resolve_compressed_accept(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot s;
    unsigned x, y;

    semu_error_clear(&error);
    bus = make_sram_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_tsc6a_create(&surface, &error));
    fill_white_asset();
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_load(bus, SRC_BASE, asset, sizeof(asset),
                                   &error));
    s = compressed_tuple();
    memset(panel, 0, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_resolve_mask(surface, bus, &s, panel, 480u,
                                             &error));
    /* Pixel-center mapping through the pinned matrix places texel (tx,ty)
     * at target (171+tx, 90+ty): the full 60x60 window becomes opaque
     * white (RGB565 0xFFFF) and nothing outside it changes. */
    for (y = 90u; y < 150u; ++y) {
        for (x = 171u; x < 231u; ++x) {
            SEMU_TEST_EQ_U64(context, 0xFFFFu, panel_pixel(x, y));
        }
    }
    SEMU_TEST_EQ_U64(context, 0u, panel_pixel(170u, 89u));
    SEMU_TEST_EQ_U64(context, 0u, panel_pixel(171u, 89u));
    SEMU_TEST_EQ_U64(context, 0u, panel_pixel(170u, 90u));
    SEMU_TEST_EQ_U64(context, 0u, panel_pixel(231u, 90u));
    SEMU_TEST_EQ_U64(context, 0u, panel_pixel(171u, 150u));
    SEMU_TEST_EQ_U64(context, 0u, panel_pixel(239u, 161u));
    nema_tsc6a_destroy(surface);
    semu_bus_destroy(bus);
}

static void test_resolve_aux_refusal_atomic(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot s;

    semu_error_clear(&error);
    bus = make_sram_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_tsc6a_create(&surface, &error));
    fill_white_asset();
    /* One bit in the auxiliary region of block (bx=3, by=2): byte 10
     * low bit (bit 80) of the block at span offset 2*180 + 3*12. */
    asset[2u * 180u + 3u * 12u + 10u] |= 0x01u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_load(bus, SRC_BASE, asset, sizeof(asset),
                                   &error));
    s = compressed_tuple();
    memset(panel, 0x5Au, sizeof(panel));
    memcpy(panel_before, panel, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     nema_tsc6a_resolve_mask(surface, bus, &s, panel, 480u,
                                             &error));
    SEMU_TEST_ASSERT(context, strstr(error.text, "auxiliary") != NULL);
    /* Zero writes anywhere in the full 115200-byte destination. */
    SEMU_TEST_ASSERT(context, memcmp(panel_before, panel,
                                      sizeof(panel)) == 0);
    nema_tsc6a_destroy(surface);
    semu_bus_destroy(bus);
}

static void test_resolve_out_of_tuple_diagnostic(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot s;

    semu_error_clear(&error);
    bus = make_sram_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_tsc6a_create(&surface, &error));
    fill_white_asset();
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_load(bus, SRC_BASE, asset, sizeof(asset),
                                   &error));
    s = compressed_tuple();
    /* One tuple bit off (draw color lsb): ticket 788 upgraded the
     * compressed-shaped refusal surface to name the failing animated
     * predicate; this near-miss now refuses at the draw-color law and
     * nothing may be written. */
    s.draw_color = UINT32_C(0xff555554);
    memset(panel, 0x33u, sizeof(panel));
    memcpy(panel_before, panel, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     nema_tsc6a_resolve_mask(surface, bus, &s, panel, 480u,
                                             &error));
    SEMU_TEST_ASSERT(context, strcmp(error.text,
        "nema_tsc6a: compressed asset draw_color 0xff555554 is outside the "
        "observed 0xff555555/0xff000000 pair") == 0);
    SEMU_TEST_ASSERT(context, memcmp(panel_before, panel,
                                      sizeof(panel)) == 0);
    nema_tsc6a_destroy(surface);
    semu_bus_destroy(bus);
}

/* --- ticket 788: the animated bounce family (census /tmp/sap235-788) ---
 * The same 60x60 stride-180 fmt-17 asset animates through the general
 * set-matrix API: mm02 carries the per-frame translation (RE law, twice
 * reproduced: mm02 = 60 - rect_x1 within 2^-15, mm12 = -90.0f exact,
 * mm00 = mm11 = 1.0f, mm01 = mm10 = 0), quads arrive left-clipped
 * (height 60, width 1..60, width < 60 only at x0 == 0), and a second
 * call site tints with draw_color 0xff000000.  Synthetic striped blocks
 * below make the consumed texel columns observable; no firmware bytes. */

static void fill_striped_asset(void)
{
    unsigned by, bx;
    /* All pixels index 0; E0 picks the block-column color; alpha 0x7FF;
     * auxiliary bits zero.  Block col 13 -> (255,0,0) 0xF800, col 14 ->
     * (0,255,0) 0x07E0, everything else -> (0,0,255) 0x001F. */
    for (by = 0u; by < 15u; ++by) {
        for (bx = 0u; bx < 15u; ++bx) {
            uint8_t blk[12] = { 0x00u, 0x00u, 0x00u, 0x00u,
                                0xF0u, 0x00u, 0x00u, 0x00u,
                                0xFFu, 0x07u, 0x00u, 0x00u };
            if (bx == 13u) {
                blk[4] = 0x00u;
                blk[5] = 0xF0u; /* E0 = 0xF000 */
            } else if (bx == 14u) {
                blk[4] = 0x00u;
                blk[5] = 0x0Fu; /* E0 = 0x0F00 */
            }
            memcpy(asset + ((size_t)by * 15u + bx) * 12u, blk, 12u);
        }
    }
}

static nema_draw_snapshot bounce_tuple(void)
{
    nema_draw_snapshot s = compressed_tuple();
    /* Left-clipped quad (0,90)-(7,150); mm02 = +53.0f = 60 - x1. */
    s.point0_x = s.point3_x = 0u;
    s.point1_x = s.point2_x = UINT32_C(7) << 16u;
    s.mm02 = UINT32_C(0x42540000);
    return s;
}

static void test_resolve_bounce_clipped_quad(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot s;

    semu_error_clear(&error);
    bus = make_sram_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_tsc6a_create(&surface, &error));
    fill_striped_asset();
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_load(bus, SRC_BASE, asset, sizeof(asset),
                                   &error));
    s = bounce_tuple();
    memset(panel, 0, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_resolve_mask(surface, bus, &s, panel, 480u,
                                             &error));
    /* Pixel centers map x -> texel column x + 53: columns 0..2 consume
     * block col 13, columns 3..6 block col 14; nothing else is written. */
    SEMU_TEST_EQ_U64(context, UINT32_C(0xF800), panel_pixel(0u, 100u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0xF800), panel_pixel(2u, 100u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x07E0), panel_pixel(3u, 100u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x07E0), panel_pixel(6u, 100u));
    SEMU_TEST_EQ_U64(context, 0u, panel_pixel(7u, 100u));
    SEMU_TEST_EQ_U64(context, 0u, panel_pixel(0u, 89u));
    SEMU_TEST_EQ_U64(context, 0u, panel_pixel(0u, 150u));
    SEMU_TEST_EQ_U64(context, 0u, panel_pixel(170u, 100u));
    nema_tsc6a_destroy(surface);
    semu_bus_destroy(bus);
}

static void test_resolve_bounce_variant_b(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot s;

    semu_error_clear(&error);
    bus = make_sram_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_tsc6a_create(&surface, &error));
    fill_striped_asset();
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_load(bus, SRC_BASE, asset, sizeof(asset),
                                   &error));
    s = compressed_tuple();
    s.draw_color = UINT32_C(0xff000000);
    /* Census variant-B geometry: quad (170,90)-(230,150) with the exact
     * translation mm02 = -170.0f = 60 - 230. */
    s.point0_x = s.point3_x = UINT32_C(170) << 16u;
    s.point1_x = s.point2_x = UINT32_C(230) << 16u;
    s.mm02 = UINT32_C(0xc32a0000);
    memset(panel, 0, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_resolve_mask(surface, bus, &s, panel, 480u,
                                             &error));
    /* Full-width quad: panel x -> texel x - 170; the tint draw_color is
     * state-only (the blend consumes tex_color), so colors come from the
     * striped texture: blue through texel 51, red 52..55, green 56..59. */
    SEMU_TEST_EQ_U64(context, UINT32_C(0x001F), panel_pixel(170u, 100u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x001F), panel_pixel(174u, 100u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x001F), panel_pixel(221u, 100u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0xF800), panel_pixel(222u, 100u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0xF800), panel_pixel(225u, 100u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x07E0), panel_pixel(226u, 100u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x07E0), panel_pixel(229u, 100u));
    SEMU_TEST_EQ_U64(context, 0u, panel_pixel(169u, 100u));
    SEMU_TEST_EQ_U64(context, 0u, panel_pixel(230u, 100u));
    nema_tsc6a_destroy(surface);
    semu_bus_destroy(bus);
}

static void test_resolve_bounce_out_of_law(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot s;

    semu_error_clear(&error);
    bus = make_sram_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_tsc6a_create(&surface, &error));
    fill_striped_asset();
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_load(bus, SRC_BASE, asset, sizeof(asset),
                                   &error));
    memset(panel, 0x5Au, sizeof(panel));
    memcpy(panel_before, panel, sizeof(panel));

    /* (1) mm02 beyond the 2^-15 epsilon: 53.25f against 60 - 7. */
    s = bounce_tuple();
    s.mm02 = UINT32_C(0x42550000);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     nema_tsc6a_resolve_mask(surface, bus, &s, panel, 480u,
                                             &error));
    /* (2) width 61 violates the rect law even with an exact mm02. */
    s = bounce_tuple();
    s.point1_x = s.point2_x = UINT32_C(61) << 16u;
    s.mm02 = UINT32_C(0xbf800000); /* -1.0f = 60 - 61 */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     nema_tsc6a_resolve_mask(surface, bus, &s, panel, 480u,
                                             &error));
    /* (3) right-clipped rect: width 59 with x0 != 0 refuses. */
    s = bounce_tuple();
    s.point0_x = s.point3_x = UINT32_C(1) << 16u;
    s.point1_x = s.point2_x = UINT32_C(60) << 16u;
    s.mm02 = 0u; /* 0.0f = 60 - 60 */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     nema_tsc6a_resolve_mask(surface, bus, &s, panel, 480u,
                                             &error));
    /* (4) any third draw_color stays refused. */
    s = bounce_tuple();
    s.draw_color = UINT32_C(0xff000001);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     nema_tsc6a_resolve_mask(surface, bus, &s, panel, 480u,
                                             &error));
    /* (5) the family keeps the null-bus fail-closed rule. */
    s = bounce_tuple();
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     nema_tsc6a_resolve_mask(surface, NULL, &s, panel, 480u,
                                             &error));
    SEMU_TEST_ASSERT(context, memcmp(panel_before, panel,
                                      sizeof(panel)) == 0);
    nema_tsc6a_destroy(surface);
    semu_bus_destroy(bus);
}

int main(void)
{
    const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_gradient_and_uniform_blocks),
        SEMU_TEST_CASE(test_raster_pixel_order),
        SEMU_TEST_CASE(test_alpha_and_refusals),
        SEMU_TEST_CASE(test_capture_golden_expansion),
        SEMU_TEST_CASE(test_resolve_compressed_accept),
        SEMU_TEST_CASE(test_resolve_aux_refusal_atomic),
        SEMU_TEST_CASE(test_resolve_out_of_tuple_diagnostic),
        SEMU_TEST_CASE(test_resolve_bounce_clipped_quad),
        SEMU_TEST_CASE(test_resolve_bounce_variant_b),
        SEMU_TEST_CASE(test_resolve_bounce_out_of_law),
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
