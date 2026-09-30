#include "../../src/display/nema_tsc6a.h"
#include "test.h"

#include "semu/bus.h"

#include <string.h>

#define SRAM_BASE UINT32_C(0x10000000)
#define FSTRIDE_TSC UINT32_C(0x170005a0)
#define FSTRIDE_RGB UINT32_C(0x040001e0)

static semu_bus *make_bus(semu_error *error)
{
    semu_bus *bus = semu_bus_create(error);
    if (bus != NULL && semu_bus_map_ram(bus, "sram", SRAM_BASE,
                                        /* Full TSC6A SRAM window
                                         * (TSC6A_SRAM_END - start): the
                                         * compressed-asset cases below use
                                         * the census source/target spans at
                                         * 0x100a490c / 0x10121d40. */
                                        UINT32_C(0x180000),
                                        error) != SEMU_OK) {
        semu_bus_destroy(bus);
        return NULL;
    }
    return bus;
}

static nema_draw_snapshot target_triangle(void)
{
    nema_draw_snapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.target_base = SRAM_BASE;
    snapshot.target_format = NEMA_FMT_TSC6A;
    snapshot.target_stride = FSTRIDE_TSC & 0xffffu;
    snapshot.target_width = NEMA_TSC6A_WIDTH;
    snapshot.target_height = NEMA_TSC6A_HEIGHT;
    snapshot.clip_max_x = NEMA_TSC6A_WIDTH;
    snapshot.clip_max_y = NEMA_TSC6A_HEIGHT;
    snapshot.draw_cmd = NEMA_DRAW_TRI_AA;
    snapshot.draw_color = UINT32_C(0xff55ff00);
    snapshot.matmult = UINT32_C(0x90000000);
    snapshot.codeptr = UINT32_C(0x941eb400);
    snapshot.point1_x = UINT32_C(2u << 16);
    snapshot.point2_y = UINT32_C(2u << 16);
    return snapshot;
}

static nema_draw_snapshot resolve_state(void)
{
    nema_draw_snapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.src_base = SRAM_BASE + 0x1000u;
    snapshot.src_format = NEMA_FMT_TSC6A;
    snapshot.src_sampling = 1u;
    snapshot.src_stride = FSTRIDE_TSC & 0xffffu;
    snapshot.src_width = NEMA_TSC6A_WIDTH;
    snapshot.src_height = NEMA_TSC6A_HEIGHT;
    snapshot.target_base = SRAM_BASE + 0x2000u;
    snapshot.target_format = NEMA_FMT_RGB565;
    snapshot.target_stride = FSTRIDE_RGB & 0xffffu;
    snapshot.target_width = 240u;
    snapshot.target_height = 240u;
    snapshot.clip_max_x = 1u;
    snapshot.clip_max_y = 1u;
    snapshot.draw_cmd = NEMA_DRAW_TSC6A_RESOLVE;
    snapshot.draw_color = UINT32_C(0xff55ff00);
    snapshot.tex_color = UINT32_C(0xff55ff00);
    snapshot.codeptr = UINT32_C(0x941e8000);
    snapshot.imem_addr = 0u;
    snapshot.imem_datah = UINT32_C(0x004e0002);
    snapshot.imem_datal = UINT32_C(0x804b1286);
    snapshot.matrix_present = 1u;
    snapshot.mm00 = UINT32_C(0x3f800000);
    snapshot.mm11 = UINT32_C(0x3f800000);
    return snapshot;
}

static void test_target_triangle_and_resolve(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot target;
    nema_draw_snapshot resolve;
    uint8_t panel[480u];

    semu_error_clear(&error);
    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_create(&surface, &error));
    target = target_triangle();
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_draw_target(surface, bus, &target, &error));
    memset(panel, 0, sizeof(panel));
    resolve = resolve_state();
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_resolve(surface, &resolve, panel, 480u,
                                        &error));
    SEMU_TEST_ASSERT(context, panel[0] != 0u || panel[1] != 0u);
    nema_tsc6a_destroy(surface);
    semu_bus_destroy(bus);
}

static void test_refuses_unobserved_state(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot target;
    nema_draw_snapshot resolve;
    uint8_t panel[480u];

    semu_error_clear(&error);
    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_create(&surface, &error));
    target = target_triangle();
    target.codeptr = UINT32_C(0x12345678);
    SEMU_TEST_ASSERT(context,
                     nema_tsc6a_draw_target(surface, bus, &target, &error) !=
                     SEMU_OK);
    resolve = resolve_state();
    resolve.codeptr = UINT32_C(0x12345678);
    SEMU_TEST_ASSERT(context,
                     nema_tsc6a_resolve(surface, &resolve, panel, 480u,
                                        &error) != SEMU_OK);
    nema_tsc6a_destroy(surface);
    semu_bus_destroy(bus);
}

static void test_resolve_accepts_firmware_native_accent(semu_test_context *context)
{
    /* Ticket 794 resolve-law extension (E-SAP-0041-EXT4 census): all 124
     * main-screen resolve refusals in the natural-terminal window carry
     * tex_color == draw_color 0xff55aaff, and that accent is firmware-native
     * (bytes ff aa 55 ff at VA 0x19c597/0x19c5f7/0x19c617/0x19c697 of the
     * hash-pinned 2.35.34 application, theme/style table, twice-reproduced
     * with two independent decoders). Every other predicate of the pinned
     * tuple already passes, so the masked accent 0x0055aaff is admitted to
     * the ACCENT predicate and the draw resolves. */
    semu_error error;
    semu_bus *bus;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot target;
    nema_draw_snapshot resolve;
    uint8_t panel[480u];

    semu_error_clear(&error);
    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_create(&surface, &error));
    target = target_triangle();
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_draw_target(surface, bus, &target, &error));
    memset(panel, 0, sizeof(panel));
    resolve = resolve_state();
    resolve.draw_color = UINT32_C(0xff55aaff);
    resolve.tex_color = UINT32_C(0xff55aaff);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_resolve(surface, &resolve, panel, 480u,
                                        &error));
    SEMU_TEST_ASSERT(context, panel[0] != 0u || panel[1] != 0u);
    nema_tsc6a_destroy(surface);
    semu_bus_destroy(bus);
}

static void test_compressed_asset_refusal_diagnostic(semu_test_context *context)
{
    semu_error error;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot resolve = resolve_state();
    uint8_t panel[480u * 240u];
    uint8_t before[sizeof(panel)];

    /* E-EMU-SAP235-MAIN-TSC6A-001 descriptor, one near-miss away from the
     * ticket-793 acceptance tuple (stride 181 instead of the captured
     * 180), resolved with bus == NULL.  Ticket 793 accepted the exact
     * compressed tuple in nema_tsc6a_resolve_mask; a null bus is
     * fail-closed, and any compressed-shaped state that is not the exact
     * tuple keeps the specialized diagnostic.  The exact-tuple acceptance
     * and the zero-write auxiliary refusal are pinned in
     * test_nema_tsc6a_expand.c with a real bus.
     * No compressed firmware bytes or private frame pixels are needed. */
    resolve.src_base = UINT32_C(0x100a490c);
    resolve.src_present = 1u;
    resolve.src_width = 60u;
    resolve.src_height = 60u;
    resolve.src_stride = 181u;
    resolve.target_base = UINT32_C(0x10121d40);
    resolve.clip_min_y = 81u;
    resolve.clip_max_x = 240u;
    resolve.clip_max_y = 162u;
    resolve.draw_cmd = NEMA_DRAW_QUAD;
    resolve.draw_color = UINT32_C(0xff555555);
    resolve.tex_color = UINT32_C(0xffffffff);
    resolve.point0_x = resolve.point3_x = 171u << 16u;
    resolve.point1_x = resolve.point2_x = 231u << 16u;
    resolve.point0_y = resolve.point1_y = 90u << 16u;
    resolve.point2_y = resolve.point3_y = 150u << 16u;
    resolve.mm02 = UINT32_C(0xc32b0001);
    resolve.mm12 = UINT32_C(0xc2b40000);
    memset(panel, 0xa5, sizeof(panel));
    memcpy(before, panel, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_create(&surface, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_tsc6a_resolve_mask(surface, NULL, &resolve, panel, 480u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, error.code);
    SEMU_TEST_ASSERT(context, strcmp(error.text,
        "nema_tsc6a: compressed source 60x60 stride 181 is unsupported; "
        "only the 480x480 semantic shadow is modeled; "
        "mat=1/3f800000/00000000/c32b0001/00000000/3f800000/c2b40000 "
        "matmult=0 target_stride=480") == 0);
    SEMU_TEST_ASSERT(context, memcmp(before, panel, sizeof(panel)) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_tsc6a_resolve_mask(surface, NULL, &resolve, panel, 480u, NULL));
    SEMU_TEST_ASSERT(context, memcmp(before, panel, sizeof(panel)) == 0);

    /* A supported shadow descriptor still resolves; its invalid shader
     * retains the existing generic state diagnostic. */
    resolve.src_width = NEMA_TSC6A_WIDTH;
    resolve.src_height = NEMA_TSC6A_HEIGHT;
    resolve.src_stride = FSTRIDE_TSC & 0xffffu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        nema_tsc6a_resolve_mask(surface, NULL, &resolve, panel, 480u, &error));
    SEMU_TEST_ASSERT(context, memcmp(before, panel, sizeof(panel)) == 0);
    resolve.codeptr = UINT32_C(0x12345678);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_tsc6a_resolve_mask(surface, NULL, &resolve, panel, 480u, &error));
    SEMU_TEST_ASSERT(context, strcmp(error.text,
        "nema_tsc6a: unsupported mask resolve state") == 0);
    SEMU_TEST_ASSERT(context, memcmp(before, panel, sizeof(panel)) == 0);
    nema_tsc6a_destroy(surface);
}

/* --- Ticket 788: compressed-asset bounce-animation law -----------------
 *
 * The ticket-793 pinned instant (x0 = 171, mm02 = 0xc32b0001, 60x60 quad,
 * draw_color 0xff555555) is one member of a 103-event family
 * (E-SAP-0041-EXT4 sibling census, twice-derived from the pinned-window
 * transcript): the same 60x60 format-0x17 asset at 0x100a490c is
 * repainted by an eased horizontal bounce, one refusal per ~10.08 ms
 * repaint tick in 6 bursts aligned with setup-walk steps 25..30.  Every
 * member keeps the pinned constant prefix (draw=QUAD, RGB565 240x240
 * target, tex_color 0xffffffff, clip (0,81)-(240,162), code 0x941e8000,
 * imem triple, matrix_present=1, y0=90/height 60, src 0x100a490c fmt 0x17
 * stride 180) and animates only
 *   - the matrix x-translation: mm02 = 60 - rect_x1 (offline RE of the
 *     pinned application, two decoders agreeing) with mm00=mm11=1.0f,
 *     mm01=mm10=0.0f and mm12=-90.0f unchanged; the pinned instant's
 *     1-ULP (2^-16) bias defines the admitted slack of 2^-15,
 *   - the rect width: 60 while moving, clipped to 1..46 at the left wall
 *     (all 31 clipped witnesses have rect_x0 == 0; height stays 60),
 *   - the draw color: 0xff000000 on the 14 wall-rest draws, alongside
 *     the pinned 0xff555555.
 * The tests use synthetic format-0x17 blocks (no firmware bytes): block
 * column bx carries the solid opaque color ((bx+1)*17, 0, 0), alpha
 * 0x7ff -> 255, so each source column blends to a distinct hand-checked
 * RGB565 word via the pinned pack law r5 = (r*31+127)/255:
 *   bx=0 -> r=17 -> 0x1000, bx=5 -> r=102 -> 0x6000,
 *   bx=13 -> r=238 -> 0xe800, bx=14 -> r=255 -> 0xf800. */

static nema_draw_snapshot compressed_asset(int rect_x0, int rect_x1)
{
    nema_draw_snapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.src_base = UINT32_C(0x100a490c);
    snapshot.src_present = 1u;
    snapshot.src_format = NEMA_FMT_TSC6A;
    snapshot.src_sampling = 1u;
    snapshot.src_stride = 180u;
    snapshot.src_width = 60u;
    snapshot.src_height = 60u;
    snapshot.target_base = UINT32_C(0x10121d40);
    snapshot.target_format = NEMA_FMT_RGB565;
    snapshot.target_stride = 480u;
    snapshot.target_width = 240u;
    snapshot.target_height = 240u;
    snapshot.clip_min_x = 0u;
    snapshot.clip_min_y = 81u;
    snapshot.clip_max_x = 240u;
    snapshot.clip_max_y = 162u;
    snapshot.draw_cmd = NEMA_DRAW_QUAD;
    snapshot.tex_color = UINT32_C(0xffffffff);
    snapshot.codeptr = UINT32_C(0x941e8000);
    snapshot.imem_addr = 0u;
    snapshot.imem_datah = UINT32_C(0x004e0002);
    snapshot.imem_datal = UINT32_C(0x804b1286);
    snapshot.matrix_present = 1u;
    snapshot.mm00 = UINT32_C(0x3f800000);
    snapshot.mm11 = UINT32_C(0x3f800000);
    snapshot.mm12 = UINT32_C(0xc2b40000);
    snapshot.draw_color = UINT32_C(0xff555555);
    snapshot.point0_x = snapshot.point3_x = (uint32_t)rect_x0 << 16u;
    snapshot.point1_x = snapshot.point2_x = (uint32_t)rect_x1 << 16u;
    snapshot.point0_y = snapshot.point1_y = 90u << 16u;
    snapshot.point2_y = snapshot.point3_y = 150u << 16u;
    return snapshot;
}

static void fill_compressed_asset(semu_test_context *context, semu_bus *bus,
                                  semu_error *error)
{
    uint8_t asset[15u * 15u * 12u];
    unsigned bx, by;
    for (by = 0u; by < 15u; ++by) {
        for (bx = 0u; bx < 15u; ++bx) {
            uint8_t *block = asset + ((size_t)by * 15u + bx) * 12u;
            block[0] = 0x00u;
            block[1] = 0x00u;
            block[2] = 0x00u;
            block[3] = 0x00u;                    /* all texels index 0 */
            block[4] = 0x00u;                    /* E0: B=0, A ignored  */
            block[5] = (uint8_t)((bx + 1u) << 4u); /* E0: R=(bx+1), G=0 */
            block[6] = 0x00u;
            block[7] = 0x00u;                    /* E1 unused           */
            block[8] = 0xFFu;
            block[9] = 0x07u;                    /* alpha 0x7ff -> 255  */
            block[10] = 0x00u;
            block[11] = 0x00u;                   /* aux bits 75..95 = 0 */
        }
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_load(bus, UINT32_C(0x100a490c), asset,
                                   sizeof(asset), error));
}

static uint16_t panel16(const uint8_t *panel, unsigned x, unsigned y)
{
    size_t off = (size_t)y * 480u + (size_t)x * 2u;
    return (uint16_t)(panel[off] | ((uint16_t)panel[off + 1u] << 8));
}

static void test_compressed_asset_ticket788_matrix_family(
    semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot draw;
    uint8_t panel[480u * 240u];
    unsigned x, y;

    semu_error_clear(&error);
    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_create(&surface, &error));
    fill_compressed_asset(context, bus, &error);

    /* Census row 1 (variant A, moving): quad (170,90)-(230,150), mm02 =
     * 60 - 230 = -170.0f exactly. */
    draw = compressed_asset(170, 230);
    draw.mm02 = UINT32_C(0xc32a0000);
    memset(panel, 0, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        nema_tsc6a_resolve_mask(surface, bus, &draw, panel, 480u, &error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x1000), panel16(panel, 170u, 90u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x6000), panel16(panel, 190u, 90u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0xf800), panel16(panel, 229u, 90u));
    for (y = 90u; y < 150u; ++y) {
        for (x = 170u; x < 230u; ++x) {
            SEMU_TEST_ASSERT(context, panel16(panel, x, y) != 0u);
        }
    }
    SEMU_TEST_EQ_U64(context, UINT32_C(0), panel16(panel, 169u, 90u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0), panel16(panel, 230u, 90u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0), panel16(panel, 170u, 89u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0), panel16(panel, 170u, 150u));

    /* The ticket-793 pinned instant (x1 = 231, mm02 = 0xc32b0001, one
     * 2^-16 bias) stays accepted under the generalized law. */
    draw = compressed_asset(171, 231);
    draw.mm02 = UINT32_C(0xc32b0001);
    memset(panel, 0, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        nema_tsc6a_resolve_mask(surface, bus, &draw, panel, 480u, &error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x1000), panel16(panel, 171u, 90u));
    for (y = 90u; y < 150u; ++y) {
        for (x = 171u; x < 231u; ++x) {
            SEMU_TEST_ASSERT(context, panel16(panel, x, y) != 0u);
        }
    }

    /* Census row 18 (left-clipped wall rest): quad (0,90)-(7,150), width
     * 7, mm02 = 60 - 7 = +53.0f; the raster must consume source texel
     * columns 53..59 (block columns 13 and 14). */
    draw = compressed_asset(0, 7);
    draw.mm02 = UINT32_C(0x42540000);
    memset(panel, 0, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        nema_tsc6a_resolve_mask(surface, bus, &draw, panel, 480u, &error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0xe800), panel16(panel, 0u, 90u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0xe800), panel16(panel, 2u, 90u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0xf800), panel16(panel, 3u, 90u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0xf800), panel16(panel, 6u, 90u));
    for (y = 90u; y < 150u; ++y) {
        for (x = 0u; x < 7u; ++x) {
            SEMU_TEST_ASSERT(context, panel16(panel, x, y) != 0u);
        }
    }
    SEMU_TEST_EQ_U64(context, UINT32_C(0), panel16(panel, 7u, 90u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0), panel16(panel, 0u, 89u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0), panel16(panel, 0u, 150u));

    /* One-ULP composer roundings observed twice in the 2026-09-27 walk
     * (residual census): width-43 wall draws carry mm11 = 0x3f7fffff,
     * width-1/2/4/8 draws carry mm12 = 0xc2b40001; all other matrix
     * words stay exact and the translation law keeps holding. */
    draw = compressed_asset(0, 43);
    draw.mm02 = UINT32_C(0x41880000); /* 17.0f = 60 - 43 */
    draw.mm11 = UINT32_C(0x3f7fffff);
    memset(panel, 0, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        nema_tsc6a_resolve_mask(surface, bus, &draw, panel, 480u, &error));
    for (y = 90u; y < 150u; ++y) {
        for (x = 0u; x < 43u; ++x) {
            SEMU_TEST_ASSERT(context, panel16(panel, x, y) != 0u);
        }
    }
    SEMU_TEST_EQ_U64(context, UINT32_C(0), panel16(panel, 43u, 90u));

    draw = compressed_asset(0, 2);
    draw.mm02 = UINT32_C(0x42680000); /* 58.0f = 60 - 2 */
    draw.mm12 = UINT32_C(0xc2b40001);
    memset(panel, 0, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        nema_tsc6a_resolve_mask(surface, bus, &draw, panel, 480u, &error));
    for (y = 90u; y < 150u; ++y) {
        for (x = 0u; x < 2u; ++x) {
            SEMU_TEST_ASSERT(context, panel16(panel, x, y) != 0u);
        }
    }
    SEMU_TEST_EQ_U64(context, UINT32_C(0), panel16(panel, 2u, 90u));
    nema_tsc6a_destroy(surface);
    semu_bus_destroy(bus);
}

static void test_compressed_asset_ticket788_draw_color_b(
    semu_test_context *context)
{
    /* Census variant B (14 wall-rest draws): draw_color 0xff000000 is
     * admitted alongside the pinned 0xff555555 (the shadow raster tints
     * with tex_color, so the pixels match variant A's geometry). */
    semu_error error;
    semu_bus *bus;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot draw;
    uint8_t panel[480u * 240u];
    unsigned x, y;

    semu_error_clear(&error);
    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_create(&surface, &error));
    fill_compressed_asset(context, bus, &error);
    draw = compressed_asset(170, 230);
    draw.mm02 = UINT32_C(0xc32a0000);
    draw.draw_color = UINT32_C(0xff000000);
    memset(panel, 0, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        nema_tsc6a_resolve_mask(surface, bus, &draw, panel, 480u, &error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x1000), panel16(panel, 170u, 90u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0xf800), panel16(panel, 229u, 90u));
    for (y = 90u; y < 150u; ++y) {
        for (x = 170u; x < 230u; ++x) {
            SEMU_TEST_ASSERT(context, panel16(panel, x, y) != 0u);
        }
    }
    nema_tsc6a_destroy(surface);
    semu_bus_destroy(bus);
}

static void test_compressed_asset_ticket788_law_refusals(
    semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot draw;
    uint8_t panel[480u * 240u];
    uint8_t before[sizeof(panel)];

    semu_error_clear(&error);
    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_create(&surface, &error));

    /* Matrix translation outside the law: rect_x1 = 231 needs mm02 =
     * -171.0f; -170.0f is off by 1.0f, far beyond the 2^-15 slack. */
    draw = compressed_asset(171, 231);
    draw.mm02 = UINT32_C(0xc32a0000);
    memset(panel, 0xa5, sizeof(panel));
    memcpy(before, panel, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_tsc6a_resolve_mask(surface, bus, &draw, panel, 480u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, error.code);
    SEMU_TEST_ASSERT(context, memcmp(before, panel, sizeof(panel)) == 0);

    /* Rect width 61 (the census never exceeds 60): refuse. */
    draw = compressed_asset(170, 231);
    draw.mm02 = UINT32_C(0xc32b0001);
    memset(panel, 0xa5, sizeof(panel));
    memcpy(before, panel, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_tsc6a_resolve_mask(surface, bus, &draw, panel, 480u, &error));
    SEMU_TEST_ASSERT(context, memcmp(before, panel, sizeof(panel)) == 0);

    /* Right-clipped rect (x0 > 0 with width < 60; all 31 census clipped
     * witnesses are left-clipped only): refuse. */
    draw = compressed_asset(185, 240);
    draw.mm02 = UINT32_C(0xc3340000);
    memset(panel, 0xa5, sizeof(panel));
    memcpy(before, panel, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_tsc6a_resolve_mask(surface, bus, &draw, panel, 480u, &error));
    SEMU_TEST_ASSERT(context, memcmp(before, panel, sizeof(panel)) == 0);

    /* Any other draw_color stays refused. */
    draw = compressed_asset(170, 230);
    draw.mm02 = UINT32_C(0xc32a0000);
    draw.draw_color = UINT32_C(0xff000001);
    memset(panel, 0xa5, sizeof(panel));
    memcpy(before, panel, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_tsc6a_resolve_mask(surface, bus, &draw, panel, 480u, &error));
    SEMU_TEST_ASSERT(context, memcmp(before, panel, sizeof(panel)) == 0);

    /* Matrix words beyond the exact pinned pair and its two observed
     * one-ULP roundings stay refused. */
    draw = compressed_asset(0, 43);
    draw.mm02 = UINT32_C(0x41880000);
    draw.mm11 = UINT32_C(0x3f7ff000);
    memset(panel, 0xa5, sizeof(panel));
    memcpy(before, panel, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_tsc6a_resolve_mask(surface, bus, &draw, panel, 480u, &error));
    SEMU_TEST_ASSERT(context, memcmp(before, panel, sizeof(panel)) == 0);

    draw = compressed_asset(0, 2);
    draw.mm02 = UINT32_C(0x42680000);
    /* Ticket 710: mm12 is band-law-validated, no longer pair-pinned; the
     * observed one-ULP class (0xc2b40001/0xc2b40002 both land one 16.16
     * unit below -90) stays inside the band, and an eight-ULP
     * translation (four units) is outside it and refuses. */
    draw.mm12 = UINT32_C(0xc2b40008);
    memset(panel, 0xa5, sizeof(panel));
    memcpy(before, panel, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_tsc6a_resolve_mask(surface, bus, &draw, panel, 480u, &error));
    SEMU_TEST_ASSERT(context, memcmp(before, panel, sizeof(panel)) == 0);
    nema_tsc6a_destroy(surface);
    semu_bus_destroy(bus);
}

int main(void)
{
    const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_target_triangle_and_resolve),
        SEMU_TEST_CASE(test_refuses_unobserved_state),
        SEMU_TEST_CASE(test_resolve_accepts_firmware_native_accent),
        SEMU_TEST_CASE(test_compressed_asset_refusal_diagnostic),
        SEMU_TEST_CASE(test_compressed_asset_ticket788_matrix_family),
        SEMU_TEST_CASE(test_compressed_asset_ticket788_draw_color_b),
        SEMU_TEST_CASE(test_compressed_asset_ticket788_law_refusals),
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
