/*
 * Integer raster and resolve loops for the evidence-gated TSC6A model.
 * Compressed guest TSC6A storage is never decoded here.
 */

#include "nema_tsc6a_internal.h"

#include "blend.h"
#include "nema_texture.h"

#include <stddef.h>

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

semu_status nema_tsc6a_resolve_mask(const nema_tsc6a *surface,
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
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                "nema_tsc6a: compressed source %ux%u stride %u is unsupported; "
                "only the 480x480 semantic shadow is modeled",
                (unsigned)s->src_width, (unsigned)s->src_height,
                (unsigned)s->src_stride);
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
