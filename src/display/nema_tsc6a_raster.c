/*
 * Integer raster and resolve loops for the evidence-gated TSC6A model.
 * Compressed guest TSC6A blocks are never decoded here: the single
 * capture-pinned compressed acceptance state (ticket 793) validates the
 * full tuple, then delegates each block to the pure law function in
 * nema_tsc6a_expand.c before any target write.
 */

#include "nema_tsc6a_internal.h"

#include "blend.h"
#include "nema_texture.h"

#include <stddef.h>
#include <string.h>

semu_status tsc6a_draw_triangle(nema_tsc6a *surface,
                                const nema_draw_snapshot *s,
                                semu_error *error)
{
    int64_t ax, ay, bx, by, cx, cy, area;
    int x0, y0, x1, y1, x, y;
    uint32_t source;

    if (!tsc6a_target_state(s) || s->matmult != TSC6A_OBSERVED_MATMULT ||
        s->codeptr != TSC6A_OBSERVED_CODE || !tsc6a_triangle_points_ok(s)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "nema_tsc6a: unsupported target triangle state");
        return SEMU_ERR_UNSUPPORTED;
    }
    ax = tsc6a_signed_fp16(s->point0_x);
    ay = tsc6a_signed_fp16(s->point0_y);
    bx = tsc6a_signed_fp16(s->point1_x);
    by = tsc6a_signed_fp16(s->point1_y);
    cx = tsc6a_signed_fp16(s->point2_x);
    cy = tsc6a_signed_fp16(s->point2_y);
    area = tsc6a_edge(ax, ay, bx, by, cx, cy);
    if (area == 0) return SEMU_OK;

    x0 = (int)tsc6a_floor_div_fp16(ax);
    if ((int)tsc6a_floor_div_fp16(bx) < x0) {
        x0 = (int)tsc6a_floor_div_fp16(bx);
    }
    if ((int)tsc6a_floor_div_fp16(cx) < x0) {
        x0 = (int)tsc6a_floor_div_fp16(cx);
    }
    y0 = (int)tsc6a_floor_div_fp16(ay);
    if ((int)tsc6a_floor_div_fp16(by) < y0) {
        y0 = (int)tsc6a_floor_div_fp16(by);
    }
    if ((int)tsc6a_floor_div_fp16(cy) < y0) {
        y0 = (int)tsc6a_floor_div_fp16(cy);
    }
    x1 = (int)tsc6a_ceil_div_fp16(ax);
    if ((int)tsc6a_ceil_div_fp16(bx) > x1) {
        x1 = (int)tsc6a_ceil_div_fp16(bx);
    }
    if ((int)tsc6a_ceil_div_fp16(cx) > x1) {
        x1 = (int)tsc6a_ceil_div_fp16(cx);
    }
    y1 = (int)tsc6a_ceil_div_fp16(ay);
    if ((int)tsc6a_ceil_div_fp16(by) > y1) {
        y1 = (int)tsc6a_ceil_div_fp16(by);
    }
    if ((int)tsc6a_ceil_div_fp16(cy) > y1) {
        y1 = (int)tsc6a_ceil_div_fp16(cy);
    }
    if (x0 < (int)s->clip_min_x) x0 = (int)s->clip_min_x;
    if (y0 < (int)s->clip_min_y) y0 = (int)s->clip_min_y;
    if (x1 > (int)s->clip_max_x) x1 = (int)s->clip_max_x;
    if (y1 > (int)s->clip_max_y) y1 = (int)s->clip_max_y;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)NEMA_TSC6A_WIDTH) x1 = (int)NEMA_TSC6A_WIDTH;
    if (y1 > (int)NEMA_TSC6A_HEIGHT) y1 = (int)NEMA_TSC6A_HEIGHT;
    source = s->draw_color;
    for (y = y0; y < y1; ++y) {
        for (x = x0; x < x1; ++x) {
            static const int64_t samples[2] = {
                INT64_C(16384), INT64_C(49152)
            };
            int covered = 0;
            unsigned sy, sx;
            for (sy = 0u; sy < 2u; ++sy) {
                for (sx = 0u; sx < 2u; ++sx) {
                    int64_t px = (int64_t)x * TSC6A_FP16_ONE + samples[sx];
                    int64_t py = (int64_t)y * TSC6A_FP16_ONE + samples[sy];
                    int inside = (area > 0)
                        ? tsc6a_edge(ax, ay, bx, by, px, py) >= 0 &&
                          tsc6a_edge(bx, by, cx, cy, px, py) >= 0 &&
                          tsc6a_edge(cx, cy, ax, ay, px, py) >= 0
                        : tsc6a_edge(ax, ay, bx, by, px, py) <= 0 &&
                          tsc6a_edge(bx, by, cx, cy, px, py) <= 0 &&
                          tsc6a_edge(cx, cy, ax, ay, px, py) <= 0;
                    covered += inside;
                }
            }
            if (covered != 0) {
                uint32_t coverage = ((uint32_t)covered * 255u) / 4u;
                size_t off = (size_t)y * NEMA_TSC6A_WIDTH + (size_t)x;
                surface->pixels[off] = tsc6a_blend_argb(
                    source, surface->pixels[off], coverage);
            }
        }
    }
    return SEMU_OK;
}

static semu_status sample_a2(semu_bus *bus, const nema_texture_desc *source,
                             int64_t x, int64_t y, uint8_t *alpha,
                             semu_error *error)
{
    if (x < 0 || y < 0 || x >= (int64_t)source->width ||
        y >= (int64_t)source->height) {
        *alpha = 0u;
        return SEMU_OK;
    }
    return nema_a2le_sample(bus, source->base, source->stride,
                            source->width, source->height, (uint32_t)x,
                            (uint32_t)y, alpha, error);
}

static semu_status sample_a2_bilinear_fp16(semu_bus *bus,
                                           const nema_texture_desc *source,
                                           int64_t x_fp16, int64_t y_fp16,
                                           uint8_t *alpha, semu_error *error)
{
    int64_t x0 = tsc6a_floor_div_fp16(x_fp16);
    int64_t y0 = tsc6a_floor_div_fp16(y_fp16);
    uint32_t fx = (uint32_t)((x_fp16 - x0 * TSC6A_FP16_ONE) >> 8u);
    uint32_t fy = (uint32_t)((y_fp16 - y0 * TSC6A_FP16_ONE) >> 8u);
    uint8_t a00, a10, a01, a11;
    uint64_t value;
    semu_status st;

    st = sample_a2(bus, source, x0, y0, &a00, error);
    if (st != SEMU_OK) return st;
    st = sample_a2(bus, source, x0 + 1, y0, &a10, error);
    if (st != SEMU_OK) return st;
    st = sample_a2(bus, source, x0, y0 + 1, &a01, error);
    if (st != SEMU_OK) return st;
    st = sample_a2(bus, source, x0 + 1, y0 + 1, &a11, error);
    if (st != SEMU_OK) return st;
    value = (uint64_t)a00 * (TSC6A_FP8_ONE - fx) * (TSC6A_FP8_ONE - fy) +
            (uint64_t)a10 * fx * (TSC6A_FP8_ONE - fy) +
            (uint64_t)a01 * (TSC6A_FP8_ONE - fx) * fy +
            (uint64_t)a11 * fx * fy;
    value = (value + 32768u) >> 16u;
    *alpha = value > 255u ? 255u : (uint8_t)value;
    return SEMU_OK;
}

semu_status tsc6a_draw_mask(nema_tsc6a *surface, semu_bus *bus,
                            const nema_draw_snapshot *s,
                            semu_error *error)
{
    nema_texture_desc source;
    tsc6a_fixed_matrix matrix;
    int x0, y0, x1, y1, x, y;
    semu_status st;

    if (!tsc6a_target_state(s) ||
        !(s->codeptr == TSC6A_RESOLVE_CODE ||
          s->codeptr == TSC6A_RESOLVE_CODE_ALT ||
          (s->codeptr == TSC6A_OBSERVED_CODE &&
           s->matmult == TSC6A_OBSERVED_MATMULT)) ||
        !s->src_present || s->src_format != NEMA_FMT_A2LE ||
        s->src_sampling != 1u || s->src_width == 0u ||
        s->src_height == 0u || s->src_width > 240u ||
        s->src_height > 240u ||
        s->src_stride < (s->src_width + 3u) / 4u || s->tex_color == 0u ||
        !tsc6a_rectangle(s, &x0, &y0, &x1, &y1)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "nema_tsc6a: unsupported target mask state");
        return SEMU_ERR_UNSUPPORTED;
    }
    source.base = s->src_base;
    source.format = s->src_format;
    source.sampling = s->src_sampling;
    source.stride = s->src_stride;
    source.width = s->src_width;
    source.height = s->src_height;
    st = nema_texture_validate(bus, &source, error);
    if (st != SEMU_OK) return st;
    st = tsc6a_snapshot_matrix(s, &matrix, error);
    if (st != SEMU_OK) return st;
    if (x0 < (int)s->clip_min_x) x0 = (int)s->clip_min_x;
    if (y0 < (int)s->clip_min_y) y0 = (int)s->clip_min_y;
    if (x1 > (int)s->clip_max_x) x1 = (int)s->clip_max_x;
    if (y1 > (int)s->clip_max_y) y1 = (int)s->clip_max_y;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)NEMA_TSC6A_WIDTH) x1 = (int)NEMA_TSC6A_WIDTH;
    if (y1 > (int)NEMA_TSC6A_HEIGHT) y1 = (int)NEMA_TSC6A_HEIGHT;
    for (y = y0; y < y1; ++y) {
        for (x = x0; x < x1; ++x) {
            int64_t u = (int64_t)matrix.mm00 * x +
                        (int64_t)matrix.mm01 * y + matrix.mm02;
            int64_t v = (int64_t)matrix.mm10 * x +
                        (int64_t)matrix.mm11 * y + matrix.mm12;
            uint8_t alpha;
            uint32_t off = (uint32_t)y * NEMA_TSC6A_WIDTH + (uint32_t)x;
            st = sample_a2_bilinear_fp16(bus, &source, u, v, &alpha, error);
            if (st != SEMU_OK) return st;
            if (alpha != 0u) {
                surface->pixels[off] = tsc6a_blend_argb(
                    s->tex_color, surface->pixels[off], alpha);
            }
        }
    }
    return SEMU_OK;
}

static uint16_t pack_round(uint32_t r, uint32_t g, uint32_t b)
{
    uint32_t r5 = (r * 31u + 127u) / 255u;
    uint32_t g6 = (g * 63u + 127u) / 255u;
    uint32_t b5 = (b * 31u + 127u) / 255u;
    return (uint16_t)((r5 << 11u) | (g6 << 5u) | b5);
}

static void blend_shadow_pixel(uint32_t source, uint32_t tint,
                               uint8_t *destination)
{
    uint32_t source_alpha = source >> 24u;
    uint32_t alpha = (source_alpha * (tint >> 24u)) / 255u;
    uint16_t destination_pixel;
    uint32_t dr, dg, db, sr, sg, sb, inv;
    uint16_t encoded;

    if (alpha == 0u) return;
    destination_pixel = (uint16_t)(destination[0] |
                                   ((uint16_t)destination[1] << 8u));
    dr = ((destination_pixel >> 11u) & 31u) * 255u / 31u;
    dg = ((destination_pixel >> 5u) & 63u) * 255u / 63u;
    db = (destination_pixel & 31u) * 255u / 31u;
    sr = (source >> 16u) & 0xffu;
    sg = (source >> 8u) & 0xffu;
    sb = source & 0xffu;
    inv = 255u - alpha;
    dr = (sr * alpha + dr * inv + 127u) / 255u;
    dg = (sg * alpha + dg * inv + 127u) / 255u;
    db = (sb * alpha + db * inv + 127u) / 255u;
    encoded = pack_round(dr, dg, db);
    destination[0] = (uint8_t)encoded;
    destination[1] = (uint8_t)(encoded >> 8u);
}

void tsc6a_resolve_point(const tsc6a_fixed_matrix *matrix, int x, int y,
                         int64_t *sx, int64_t *sy)
{
    int64_t cx = (int64_t)x * TSC6A_FP16_ONE + TSC6A_FP16_ONE / 2;
    int64_t cy = (int64_t)y * TSC6A_FP16_ONE + TSC6A_FP16_ONE / 2;
    int64_t u = ((int64_t)matrix->mm00 * cx) / TSC6A_FP16_ONE +
        ((int64_t)matrix->mm01 * cy) / TSC6A_FP16_ONE +
        matrix->mm02;
    int64_t v = ((int64_t)matrix->mm10 * cx) / TSC6A_FP16_ONE +
        ((int64_t)matrix->mm11 * cy) / TSC6A_FP16_ONE +
        matrix->mm12;
    *sx = tsc6a_floor_div_fp16(u);
    *sy = tsc6a_floor_div_fp16(v);
}

semu_status nema_tsc6a_resolve(const nema_tsc6a *surface,
                               const nema_draw_snapshot *s,
                               uint8_t *rgb565_le, uint32_t stride,
                               semu_error *error)
{
    tsc6a_fixed_matrix matrix;
    int x, y;
    semu_status st;

    if (surface == NULL || s == NULL || rgb565_le == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "nema_tsc6a: null resolve argument");
        return SEMU_ERR_ARGUMENT;
    }
    if (!tsc6a_resolve_state(s) || stride < 480u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "nema_tsc6a: unsupported resolve state");
        return SEMU_ERR_UNSUPPORTED;
    }
    st = tsc6a_snapshot_matrix(s, &matrix, error);
    if (st != SEMU_OK) return st;
    for (y = (int)s->clip_min_y; y < (int)s->clip_max_y; ++y) {
        for (x = (int)s->clip_min_x; x < (int)s->clip_max_x; ++x) {
            int64_t sx, sy;
            size_t off;

            tsc6a_resolve_point(&matrix, x, y, &sx, &sy);
            if (sx < 0 || sy < 0 || sx >= (int64_t)NEMA_TSC6A_WIDTH ||
                sy >= (int64_t)NEMA_TSC6A_HEIGHT) continue;
            off = (size_t)y * stride + (size_t)x * 2u;
            blend_shadow_pixel(surface->pixels[(size_t)sy * NEMA_TSC6A_WIDTH +
                                               (size_t)sx], s->tex_color,
                               rgb565_le + off);
        }
    }
    return SEMU_OK;
}

/*
 * Ticket 793: the single observed native compressed draw — the 60x60
 * compass crosshair refusing at 2.35 main entry (E-EMU-SAP235-MAIN-
 * TSC6A-001 descriptor, accepted per E-RE-SAP235-TSC6A-001).  Every
 * constant below is that capture: source format 0x17, sampling 1, stride
 * 180, 60x60 (15 block rows of 180 B = 2700 B span); target RGB565,
 * stride 480, 240x240 (115200 B span); codeptr 0x941e8000 with matmult 0;
 * IMEM (0,004e0002,804b1286); matrix bits (3f800000,0,c32b0001;
 * 0,3f800000,c2b40000); observed ordered clip (0,81)-(240,162); observed
 * 60x60 axis-aligned quad (171,90)-(231,150); tint 0xffffffff; draw
 * color 0xff555555.  The mapping and clip use the existing helpers; no
 * value here is invented.
 *
 * Ticket 788 (E-SAP-0041-EXT4 sibling census, twice-derived from the
 * natural-terminal window): that pinned instant is one member of a
 * 103-event family — the same asset repainted by an eased horizontal
 * bounce animation, one repaint tick per ~10.08 ms in 6 bursts aligned
 * with setup-walk steps 25..30.  The animated predicates (draw color
 * pair, left-clip-only rect widths, mm02 = 60 - rect_x1 translation law)
 * are pinned by tsc6a_compressed_asset_law below; everything else stays
 * bit-pinned to the capture in tsc6a_compressed_asset_shape.
 */
#define TSC6A_CROSSHAIR_W 60u
#define TSC6A_CROSSHAIR_H 60u
#define TSC6A_CROSSHAIR_SRC_BYTES 2700u
#define TSC6A_TARGET_RGB_BYTES (480u * 240u * 2u)
#define TSC6A_TINT_IDENTITY UINT32_C(0xffffffff)
#define TSC6A_CROSSHAIR_DRAW_COLOR UINT32_C(0xff555555)
#define TSC6A_CROSSHAIR_DRAW_COLOR_ALT UINT32_C(0xff000000)
#define TSC6A_MATRIX_ONE UINT32_C(0x3f800000)
#define TSC6A_MATRIX_TX0 UINT32_C(0xc32b0001)
/* One-ULP binary32 roundings the pinned composer emits at specific
 * bounce phases (twice-reproduced residual census, 2026-09-27 walk:
 * mm11 0x3f7fffff on the width-43 wall draws; every other scale word
 * stays exact).  Observed bit values only - no rounding formula is
 * assumed.  The mm12 translation is NOT shape-pinned: the composer's
 * set-matrix API (RE 0xc1b5e, E-SAP-0041-EXT6 derivation notes) emits
 * MM02 = -dstX, MM12 = -dstY with the quad as the strip/clip cut, so
 * mm12 is validated as a law in tsc6a_compressed_asset_law (the
 * horizontal bounce pins dstY = 90; the ticket-710 vertical-scroll
 * family animates dstY through 151/192/220, census
 * /tmp/sap235-vscroll twice byte-identical). */
#define TSC6A_MATRIX_ONE_ALT UINT32_C(0x3f7fffff)
/* 2^-15 slack of the mm02 = 60 - rect_x1 law in 16.16 units (2 * 65536 *
 * 2^-15): the pinned instant's observed mm02 0xc32b0001 sits exactly one
 * binary32 ULP (2^-16 = 1 unit) off -171.0 at rect_x1 = 231, and every
 * census translation is an exact integer.  The vertical-scroll family
 * (ticket 710 instance, census /tmp/sap235-vscroll twice byte-identical)
 * shows the same one-ULP class on mm12 (0xc3170001 against -151.0), so
 * the mm12 band law below admits exactly that observed one-unit bias -
 * a two-ULP translation still refuses. */
#define TSC6A_ASSET_TX_SLACK_FP16 2
#define TSC6A_ASSET_TY_SLACK_FP16 1

/* Second accepted state of this resolve: the capture-pinned compressed
 * tuple with the ticket-788 animated predicates factored out into
 * tsc6a_compressed_asset_law.  bus must be present because the source
 * bytes are read from bounded guest SRAM; NULL bus fails closed into the
 * existing refusal. */
static int tsc6a_compressed_asset_shape(const nema_draw_snapshot *s,
                                        semu_bus *bus)
{
    if (bus == NULL || !s->src_present || s->draw_cmd != NEMA_DRAW_QUAD ||
        s->src_format != NEMA_FMT_TSC6A || s->src_sampling != 1u ||
        s->src_stride != 180u || s->src_width != TSC6A_CROSSHAIR_W ||
        s->src_height != TSC6A_CROSSHAIR_H ||
        !tsc6a_bounded_sram(s->src_base, TSC6A_CROSSHAIR_SRC_BYTES) ||
        s->target_format != NEMA_FMT_RGB565 || s->target_stride != 480u ||
        s->target_width != 240u || s->target_height != 240u ||
        !tsc6a_bounded_sram(s->target_base, TSC6A_TARGET_RGB_BYTES) ||
        /* Refuse span overlap before any read (nema_rgba4444.c style).
         * Both spans are inside the bounded SRAM window at this point,
         * so neither sum can overflow. */
        (s->src_base < s->target_base + TSC6A_TARGET_RGB_BYTES &&
         s->target_base < s->src_base + TSC6A_CROSSHAIR_SRC_BYTES) ||
        s->codeptr != TSC6A_RESOLVE_CODE || s->matmult != 0u ||
        s->imem_addr != TSC6A_IMEM_ADDRESS ||
        s->imem_datah != TSC6A_IMEM_DATAH ||
        s->imem_datal != TSC6A_IMEM_DATAL ||
        !s->matrix_present || s->mm00 != TSC6A_MATRIX_ONE ||
        s->mm01 != 0u || s->mm10 != 0u ||
        (s->mm11 != TSC6A_MATRIX_ONE &&
         s->mm11 != TSC6A_MATRIX_ONE_ALT) ||
        !tsc6a_ordered_clip(s, 240u, 240u) ||
        s->tex_color != TSC6A_TINT_IDENTITY) {
        return 0;
    }
    return 1;
}

/* Ticket 788/710 admission law for the animated siblings of the pinned
 * instant (E-SAP-0041-EXT4/EXT6 census, twice-derived; offline RE of
 * the hash-pinned application with two agreeing decoders).  Beyond the
 * pinned shape the observed families animate these predicates:
 *   - the draw color pair {0xff555555, 0xff000000} (89 / 14 census
 *     draws),
 *   - the rect: width 1..60 with width < 60 only while left-clipped
 *     (rect_x0 == 0; all 31 clipped witnesses), height 1..60 with
 *     height < 60 only as a composer clip cut (one quad end on a clip
 *     edge; the vertical-scroll witnesses D1..D4, ticket 710 census
 *     /tmp/sap235-vscroll twice byte-identical), and
 *   - the matrix translations: mm02 = 60 - rect_x1 with a 2^-15 slack
 *     that covers the pinned instant's observed 2^-16 bias (mm02
 *     0xc32b0001 = TSC6A_MATRIX_TX0 at rect_x1 = 231), and the vertical
 *     band law for mm12 = -dstY (the composer's set-matrix API, RE
 *     0xc1b5e, translates the source top onto the strip): the quad
 *     must map inside the 60-row source, v(rect_y0) >= -1 and
 *     v(rect_y1) <= 60*65536 + 1 in 16.16 units (exactly the observed
 *     one-ULP bias class); the horizontal bounce keeps dstY = 90
 *     (v(y0) = 0, v(y1) = 60) and the vertical family animates dstY =
 *     151/192/220.
 * mm00=mm11=1.0 and mm01=mm10=0 stay bit-pinned in the shape.
 * rect_x1 is bounded to ±2048 by tsc6a_rectangle, so |60 - rect_x1| <
 * 2^11 is exactly representable in binary32 and the comparison runs on
 * the shared integer 16.16 conversion with int64 intermediates - no
 * host FP.
 * Returns SEMU_OK when admitted; otherwise names the first failing
 * predicate, refuses with SEMU_ERR_UNSUPPORTED and zero writes (no
 * source memory is read here). */
static semu_status tsc6a_compressed_asset_law(const nema_draw_snapshot *s,
                                              int rect_x0, int rect_y0,
                                              int rect_x1, int rect_y1,
                                              semu_error *error)
{
    int32_t matrix_tx;
    int32_t matrix_ty;
    int64_t expected_tx;
    int64_t band_top;
    int64_t band_bottom;
    semu_status st;

    if (s->draw_color != TSC6A_CROSSHAIR_DRAW_COLOR &&
        s->draw_color != TSC6A_CROSSHAIR_DRAW_COLOR_ALT) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
            "nema_tsc6a: compressed asset draw_color 0x%08x is outside the "
            "observed 0xff555555/0xff000000 pair", s->draw_color);
        return SEMU_ERR_UNSUPPORTED;
    }
    if ((rect_y1 - rect_y0) < 1 ||
        (rect_y1 - rect_y0) > (int)TSC6A_CROSSHAIR_H ||
        ((rect_y1 - rect_y0) < (int)TSC6A_CROSSHAIR_H &&
         rect_y0 != (int)s->clip_min_y && rect_y1 != (int)s->clip_max_y)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
            "nema_tsc6a: compressed asset rect (%d,%d)-(%d,%d) is outside "
            "the observed 60-bounded clip-cut height family",
            rect_x0, rect_y0, rect_x1, rect_y1);
        return SEMU_ERR_UNSUPPORTED;
    }
    if ((rect_x1 - rect_x0) < 1 ||
        (rect_x1 - rect_x0) > (int)TSC6A_CROSSHAIR_W ||
        ((rect_x1 - rect_x0) < (int)TSC6A_CROSSHAIR_W && rect_x0 != 0)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
            "nema_tsc6a: compressed asset rect (%d,%d)-(%d,%d) is outside "
            "the observed 60-bounded left-clip-only width family",
            rect_x0, rect_y0, rect_x1, rect_y1);
        return SEMU_ERR_UNSUPPORTED;
    }
    st = tsc6a_float_to_fp16(s->mm02, &matrix_tx, error);
    if (st != SEMU_OK) {
        return st;
    }
    expected_tx = (int64_t)((int)TSC6A_CROSSHAIR_W - rect_x1) *
                  TSC6A_FP16_ONE;
    if (matrix_tx < expected_tx - TSC6A_ASSET_TX_SLACK_FP16 ||
        matrix_tx > expected_tx + TSC6A_ASSET_TX_SLACK_FP16) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
            "nema_tsc6a: compressed asset matrix mm02 0x%08x is outside the "
            "observed 60-rect_x1 translation law",
            s->mm02);
        return SEMU_ERR_UNSUPPORTED;
    }
    st = tsc6a_float_to_fp16(s->mm12, &matrix_ty, error);
    if (st != SEMU_OK) {
        return st;
    }
    band_top = (int64_t)rect_y0 * TSC6A_FP16_ONE + matrix_ty;
    band_bottom = (int64_t)rect_y1 * TSC6A_FP16_ONE + matrix_ty;
    if (band_top < -(int64_t)TSC6A_ASSET_TY_SLACK_FP16 ||
        band_bottom >
            (int64_t)TSC6A_CROSSHAIR_H * TSC6A_FP16_ONE +
            TSC6A_ASSET_TY_SLACK_FP16) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
            "nema_tsc6a: compressed asset matrix mm12 0x%08x is outside "
            "the observed strip-clip band law",
            s->mm12);
        return SEMU_ERR_UNSUPPORTED;
    }
    return SEMU_OK;
}

/* Execute the accepted compressed state.  Validate fully before mutating
 * (nema_rgba4444.c rule): the bounded source span is read byte-by-byte
 * with the memory-only bus API the shadow mask draws already use, then
 * all 225 blocks expand into a local buffer; any read or expansion
 * failure refuses with SEMU_ERR_UNSUPPORTED and zero target writes.
 * Target pixels map through the pinned matrix with the existing helpers
 * at pixel centers (the mapping form of nema_tsc6a_resolve above, which
 * makes the captured identity-scale matrix cover the 60x60 quad 1:1),
 * range-check into the asset, and SRC_OVER through the existing shadow
 * blend with tex_color as the identity tint. */
static semu_status tsc6a_resolve_compressed_asset(
    semu_bus *bus, const nema_draw_snapshot *s, uint8_t *rgb565_le,
    uint32_t stride, int x0, int y0, int x1, int y1, semu_error *error)
{
    uint8_t source[TSC6A_CROSSHAIR_SRC_BYTES];
    uint8_t texels[TSC6A_CROSSHAIR_H][TSC6A_CROSSHAIR_W][4];
    tsc6a_fixed_matrix matrix;
    unsigned by, bx, py, px, offset;
    int x, y;
    semu_status st;

    for (offset = 0u; offset < TSC6A_CROSSHAIR_SRC_BYTES; ++offset) {
        st = semu_bus_copy_out(bus, s->src_base + offset,
                               &source[offset], 1u, error);
        if (st != SEMU_OK) {
            return st; /* zero writes: refusal before mutation */
        }
    }
    for (by = 0u; by < TSC6A_CROSSHAIR_H / 4u; ++by) {
        for (bx = 0u; bx < TSC6A_CROSSHAIR_W / 4u; ++bx) {
            uint8_t block[16][4];
            if (!tsc6a_expand_block(source + (size_t)by * 180u +
                                    (size_t)bx * 12u, block)) {
                semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                    "nema_tsc6a: compressed TSC6A block %u,%u sets the "
                    "unverified auxiliary bits; refusing with zero writes",
                    bx, by);
                return SEMU_ERR_UNSUPPORTED;
            }
            for (py = 0u; py < 4u; ++py) {
                for (px = 0u; px < 4u; ++px) {
                    memcpy(texels[by * 4u + py][bx * 4u + px],
                           block[py * 4u + px], 4u);
                }
            }
        }
    }
    st = tsc6a_snapshot_matrix(s, &matrix, error);
    if (st != SEMU_OK) {
        return st;
    }
    if (x0 < (int)s->clip_min_x) x0 = (int)s->clip_min_x;
    if (y0 < (int)s->clip_min_y) y0 = (int)s->clip_min_y;
    if (x1 > (int)s->clip_max_x) x1 = (int)s->clip_max_x;
    if (y1 > (int)s->clip_max_y) y1 = (int)s->clip_max_y;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > 240) x1 = 240;
    if (y1 > 240) y1 = 240;
    for (y = y0; y < y1; ++y) {
        for (x = x0; x < x1; ++x) {
            int64_t cx = (int64_t)x * TSC6A_FP16_ONE + TSC6A_FP16_ONE / 2;
            int64_t cy = (int64_t)y * TSC6A_FP16_ONE + TSC6A_FP16_ONE / 2;
            int64_t u = ((int64_t)matrix.mm00 * cx) / TSC6A_FP16_ONE +
                        ((int64_t)matrix.mm01 * cy) / TSC6A_FP16_ONE +
                        matrix.mm02;
            int64_t v = ((int64_t)matrix.mm10 * cx) / TSC6A_FP16_ONE +
                        ((int64_t)matrix.mm11 * cy) / TSC6A_FP16_ONE +
                        matrix.mm12;
            int64_t sx = tsc6a_floor_div_fp16(u);
            int64_t sy = tsc6a_floor_div_fp16(v);
            uint32_t argb;

            if (sx < 0 || sy < 0 || sx >= (int64_t)TSC6A_CROSSHAIR_W ||
                sy >= (int64_t)TSC6A_CROSSHAIR_H) {
                continue;
            }
            argb = ((uint32_t)texels[sy][sx][3] << 24u) |
                   ((uint32_t)texels[sy][sx][0] << 16u) |
                   ((uint32_t)texels[sy][sx][1] << 8u) |
                   (uint32_t)texels[sy][sx][2];
            blend_shadow_pixel(argb, s->tex_color,
                               rgb565_le + (size_t)y * stride +
                               (size_t)x * 2u);
        }
    }
    return SEMU_OK;
}

semu_status nema_tsc6a_resolve_mask(const nema_tsc6a *surface,
                                    semu_bus *bus,
                                    const nema_draw_snapshot *s,
                                    uint8_t *rgb565_le, uint32_t stride,
                                    semu_error *error)
{
    tsc6a_fixed_matrix matrix;
    int x0, y0, x1, y1, x, y;
    semu_status st;

    if (surface == NULL || s == NULL || rgb565_le == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "nema_tsc6a: null mask resolve argument");
        return SEMU_ERR_ARGUMENT;
    }
    /* The compressed state writes with the captured 480 B pitch and no
     * other caller pitch (the shadow branch keeps its stride >= 480 form
     * inside the existing cascade below). */
    if (stride == 480u && tsc6a_compressed_asset_shape(s, bus) &&
        tsc6a_rectangle(s, &x0, &y0, &x1, &y1)) {
        st = tsc6a_compressed_asset_law(s, x0, y0, x1, y1, error);
        if (st != SEMU_OK) {
            /* Census-shaped but outside the ticket-788 law: the named
             * refusal above, zero writes, no source-memory read. */
            return st;
        }
        return tsc6a_resolve_compressed_asset(bus, s, rgb565_le, stride,
                                              x0, y0, x1, y1, error);
    }
    if (!tsc6a_bounded_sram(s->src_base, 1u) ||
        !tsc6a_bounded_sram(s->target_base, 480u * 240u * 2u) ||
        s->src_format != NEMA_FMT_TSC6A || s->src_sampling != 1u ||
        s->src_stride != 0x05a0u || s->src_width != NEMA_TSC6A_WIDTH ||
        s->src_height != NEMA_TSC6A_HEIGHT ||
        s->target_format != NEMA_FMT_RGB565 || s->target_stride != 0x01e0u ||
        s->target_width != 240u || s->target_height != 240u ||
        !(s->codeptr == TSC6A_RESOLVE_CODE ||
          s->codeptr == TSC6A_RESOLVE_CODE_ALT ||
          (s->codeptr == TSC6A_OBSERVED_CODE &&
           s->matmult == TSC6A_OBSERVED_MATMULT)) ||
        !tsc6a_ordered_clip(s, 240u, 240u) || stride < 480u ||
        !s->matrix_present || !tsc6a_rectangle(s, &x0, &y0, &x1, &y1)) {
        /* E-EMU-SAP235-MAIN-TSC6A-001: distinguish a compressed asset
         * from malformed state for the supported semantic shadow.
         * This diagnostic does not decode or read the source memory. */
        if (s->src_format == NEMA_FMT_TSC6A &&
            (s->src_width != NEMA_TSC6A_WIDTH ||
             s->src_height != NEMA_TSC6A_HEIGHT ||
             s->src_stride != 0x05a0u)) {
            /* The pinned prefix keeps the compressed-source grep stable;
             * the appended fields expose the shape predicates the gpu
             * submission line does not log (matrix words, matmult,
             * target stride) so the residual census derives from the
             * transcript alone.  This diagnostic still does not decode
             * or read the source memory. */
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                "nema_tsc6a: compressed source %ux%u stride %u is unsupported; "
                "only the 480x480 semantic shadow is modeled; "
                "mat=%u/%08x/%08x/%08x/%08x/%08x/%08x matmult=%u "
                "target_stride=%u",
                (unsigned)s->src_width, (unsigned)s->src_height,
                (unsigned)s->src_stride,
                (unsigned)s->matrix_present, s->mm00, s->mm01, s->mm02,
                s->mm10, s->mm11, s->mm12, (unsigned)s->matmult,
                (unsigned)s->target_stride);
        } else {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "nema_tsc6a: unsupported mask resolve state");
        }
        return SEMU_ERR_UNSUPPORTED;
    }
    st = tsc6a_snapshot_matrix(s, &matrix, error);
    if (st != SEMU_OK) return st;
    if (x0 < (int)s->clip_min_x) x0 = (int)s->clip_min_x;
    if (y0 < (int)s->clip_min_y) y0 = (int)s->clip_min_y;
    if (x1 > (int)s->clip_max_x) x1 = (int)s->clip_max_x;
    if (y1 > (int)s->clip_max_y) y1 = (int)s->clip_max_y;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > 240) x1 = 240;
    if (y1 > 240) y1 = 240;
    for (y = y0; y < y1; ++y) {
        for (x = x0; x < x1; ++x) {
            int64_t u = (int64_t)matrix.mm00 * x +
                        (int64_t)matrix.mm01 * y + matrix.mm02;
            int64_t v = (int64_t)matrix.mm10 * x +
                        (int64_t)matrix.mm11 * y + matrix.mm12;
            int64_t sx = tsc6a_floor_div_fp16(u);
            int64_t sy = tsc6a_floor_div_fp16(v);
            size_t off;
            if (sx < 0 || sy < 0 || sx >= (int64_t)NEMA_TSC6A_WIDTH ||
                sy >= (int64_t)NEMA_TSC6A_HEIGHT) continue;
            off = (size_t)y * stride + (size_t)x * 2u;
            blend_shadow_pixel(surface->pixels[(size_t)sy * NEMA_TSC6A_WIDTH +
                                               (size_t)sx], s->tex_color,
                               rgb565_le + off);
        }
    }
    return SEMU_OK;
}

semu_status nema_tsc6a_draw_rgb565_triangle(
    const nema_draw_snapshot *s, uint8_t *rgb565_le, uint32_t width,
    uint32_t height, uint32_t stride, int edge_antialias, semu_error *error)
{
    int64_t ax, ay, bx, by, cx, cy, area;
    int x0, y0, x1, y1, x, y;
    uint32_t source;

    if (s == NULL || rgb565_le == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "nema_tsc6a: null triangle argument");
        return SEMU_ERR_ARGUMENT;
    }
    if (!tsc6a_rgb_target_state(s) || !tsc6a_triangle_points_ok(s) ||
        (s->draw_cmd != NEMA_DRAW_TRI_SOLID &&
         s->draw_cmd != NEMA_DRAW_TRI_AA)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "nema_tsc6a: unsupported RGB565 triangle state");
        return SEMU_ERR_UNSUPPORTED;
    }
    if (width == 0u || height == 0u || width > UINT32_MAX / 2u ||
        stride < width * 2u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "nema_tsc6a: invalid RGB565 target");
        return SEMU_ERR_ARGUMENT;
    }
    ax = tsc6a_signed_fp16(s->point0_x);
    ay = tsc6a_signed_fp16(s->point0_y);
    bx = tsc6a_signed_fp16(s->point1_x);
    by = tsc6a_signed_fp16(s->point1_y);
    cx = tsc6a_signed_fp16(s->point2_x);
    cy = tsc6a_signed_fp16(s->point2_y);
    area = tsc6a_edge(ax, ay, bx, by, cx, cy);
    if (area == 0) return SEMU_OK;
    x0 = (int)tsc6a_floor_div_fp16(ax);
    if ((int)tsc6a_floor_div_fp16(bx) < x0) x0 = (int)tsc6a_floor_div_fp16(bx);
    if ((int)tsc6a_floor_div_fp16(cx) < x0) x0 = (int)tsc6a_floor_div_fp16(cx);
    y0 = (int)tsc6a_floor_div_fp16(ay);
    if ((int)tsc6a_floor_div_fp16(by) < y0) y0 = (int)tsc6a_floor_div_fp16(by);
    if ((int)tsc6a_floor_div_fp16(cy) < y0) y0 = (int)tsc6a_floor_div_fp16(cy);
    x1 = (int)tsc6a_ceil_div_fp16(ax);
    if ((int)tsc6a_ceil_div_fp16(bx) > x1) x1 = (int)tsc6a_ceil_div_fp16(bx);
    if ((int)tsc6a_ceil_div_fp16(cx) > x1) x1 = (int)tsc6a_ceil_div_fp16(cx);
    y1 = (int)tsc6a_ceil_div_fp16(ay);
    if ((int)tsc6a_ceil_div_fp16(by) > y1) y1 = (int)tsc6a_ceil_div_fp16(by);
    if ((int)tsc6a_ceil_div_fp16(cy) > y1) y1 = (int)tsc6a_ceil_div_fp16(cy);
    if (x0 < (int)s->clip_min_x) x0 = (int)s->clip_min_x;
    if (y0 < (int)s->clip_min_y) y0 = (int)s->clip_min_y;
    if (x1 > (int)s->clip_max_x) x1 = (int)s->clip_max_x;
    if (y1 > (int)s->clip_max_y) y1 = (int)s->clip_max_y;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)width) x1 = (int)width;
    if (y1 > (int)height) y1 = (int)height;
    source = s->draw_color;
    for (y = y0; y < y1; ++y) {
        for (x = x0; x < x1; ++x) {
            static const int64_t aa[2] = {
                INT64_C(16384), INT64_C(49152)
            };
            unsigned sx, sy, count = 0u;
            unsigned samples = edge_antialias ? 4u : 1u;
            for (sy = 0u; sy < (edge_antialias ? 2u : 1u); ++sy) {
                for (sx = 0u; sx < (edge_antialias ? 2u : 1u); ++sx) {
                    int64_t px = (int64_t)x * TSC6A_FP16_ONE +
                        (edge_antialias ? aa[sx] : TSC6A_FP16_ONE / 2);
                    int64_t py = (int64_t)y * TSC6A_FP16_ONE +
                        (edge_antialias ? aa[sy] : TSC6A_FP16_ONE / 2);
                    int inside = (area > 0)
                        ? tsc6a_edge(ax, ay, bx, by, px, py) >= 0 &&
                          tsc6a_edge(bx, by, cx, cy, px, py) >= 0 &&
                          tsc6a_edge(cx, cy, ax, ay, px, py) >= 0
                        : tsc6a_edge(ax, ay, bx, by, px, py) <= 0 &&
                          tsc6a_edge(bx, by, cx, cy, px, py) <= 0 &&
                          tsc6a_edge(cx, cy, ax, ay, px, py) <= 0;
                    count += (unsigned)inside;
                }
            }
            if (count != 0u) {
                uint32_t coverage = edge_antialias
                    ? (count * 255u) / samples : 255u;
                uint32_t alpha = ((source >> 24u) * coverage) / 255u;
                size_t off = (size_t)y * stride + (size_t)x * 2u;
                uint16_t dst = (uint16_t)(rgb565_le[off] |
                                      ((uint16_t)rgb565_le[off + 1u] << 8u));
                rgb8 src_rgb, dst_rgb;
                uint32_t inv, r, g, b;
                blend_unpack_rgb565(dst, &dst_rgb);
                src_rgb.r = (uint8_t)(source >> 16u);
                src_rgb.g = (uint8_t)(source >> 8u);
                src_rgb.b = (uint8_t)source;
                inv = 255u - alpha;
                r = ((uint32_t)src_rgb.r * alpha + dst_rgb.r * inv + 127u) / 255u;
                g = ((uint32_t)src_rgb.g * alpha + dst_rgb.g * inv + 127u) / 255u;
                b = ((uint32_t)src_rgb.b * alpha + dst_rgb.b * inv + 127u) / 255u;
                dst = pack_round(r, g, b);
                rgb565_le[off] = (uint8_t)dst;
                rgb565_le[off + 1u] = (uint8_t)(dst >> 8u);
            }
        }
    }
    return SEMU_OK;
}
