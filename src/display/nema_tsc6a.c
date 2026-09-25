/*
 * Evidence-gated semantic TSC6A renderer for Sapporo onboarding.
 *
 * This file owns the bounded state/validation layer.  Raster and resolve
 * loops live in nema_tsc6a_raster.c; neither file interprets compressed
 * guest TSC6A bytes.
 */

#include "nema_tsc6a_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

int tsc6a_bounded_sram(uint32_t base, uint32_t size)
{
    return base >= TSC6A_SRAM_START && base < TSC6A_SRAM_END &&
           size <= TSC6A_SRAM_END - base;
}

int tsc6a_ordered_clip(const nema_draw_snapshot *s, uint32_t width,
                       uint32_t height)
{
    return s->clip_min_x <= s->clip_max_x &&
           s->clip_min_y <= s->clip_max_y &&
           s->clip_max_x <= width && s->clip_max_y <= height;
}

int64_t tsc6a_signed_fp16(uint32_t value)
{
    return (int64_t)(int32_t)value;
}

int64_t tsc6a_floor_div_fp16(int64_t value)
{
    if (value >= 0) return value / TSC6A_FP16_ONE;
    return -((-value + TSC6A_FP16_ONE - 1) / TSC6A_FP16_ONE);
}

int64_t tsc6a_ceil_div_fp16(int64_t value)
{
    if (value >= 0) return (value + TSC6A_FP16_ONE - 1) / TSC6A_FP16_ONE;
    return -((-value) / TSC6A_FP16_ONE);
}

int tsc6a_coordinate_ok(int64_t value)
{
    return value >= -(int64_t)TSC6A_MAX_COORD * TSC6A_FP16_ONE &&
           value <= (int64_t)TSC6A_MAX_COORD * TSC6A_FP16_ONE;
}

/* Convert a finite binary32 value to signed 16.16 without host FP. */
static semu_status float_to_fp16(uint32_t bits, int32_t *out,
                                 semu_error *error)
{
    uint32_t exponent = (bits >> 23u) & 0xffu;
    uint32_t fraction = bits & 0x7fffffu;
    uint64_t significand;
    uint64_t magnitude;
    uint64_t limit;
    int exponent_value;
    int shift;
    int negative = (bits >> 31u) != 0u;

    if (exponent == 0xffu) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "nema_tsc6a: non-finite matrix 0x%08x", bits);
        return SEMU_ERR_UNSUPPORTED;
    }
    if (exponent == 0u) {
        if (fraction == 0u) {
            *out = 0;
            return SEMU_OK;
        }
        exponent_value = -126;
        significand = fraction;
    } else {
        exponent_value = (int)exponent - 127;
        significand = UINT64_C(0x800000) | fraction;
    }
    shift = exponent_value - 7;
    if (shift >= 64) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "nema_tsc6a: matrix out of range 0x%08x", bits);
        return SEMU_ERR_RANGE;
    }
    if (shift >= 0) {
        if (significand > (UINT64_MAX >> (unsigned)shift)) {
            semu_error_set(error, SEMU_ERR_RANGE,
                           "nema_tsc6a: matrix out of range 0x%08x", bits);
            return SEMU_ERR_RANGE;
        }
        magnitude = significand << (unsigned)shift;
    } else {
        unsigned right = (unsigned)(-shift);
        if (right >= 64u) {
            magnitude = 0u;
        } else {
            uint64_t rounding = UINT64_C(1) << (right - 1u);
            magnitude = (significand + rounding) >> right;
        }
    }
    limit = negative ? UINT64_C(2147483648) : UINT64_C(2147483647);
    if (magnitude > limit) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "nema_tsc6a: matrix out of range 0x%08x", bits);
        return SEMU_ERR_RANGE;
    }
    *out = negative && magnitude == UINT64_C(2147483648)
        ? INT32_MIN : negative ? -(int32_t)magnitude : (int32_t)magnitude;
    return SEMU_OK;
}

semu_status tsc6a_snapshot_matrix(const nema_draw_snapshot *s,
                                  tsc6a_fixed_matrix *out,
                                  semu_error *error)
{
    semu_status st;
    if (!s->matrix_present) {
        out->mm00 = INT32_C(0x00010000);
        out->mm01 = 0;
        out->mm02 = 0;
        out->mm10 = 0;
        out->mm11 = INT32_C(0x00010000);
        out->mm12 = 0;
        return SEMU_OK;
    }
    st = float_to_fp16(s->mm00, &out->mm00, error);
    if (st != SEMU_OK) return st;
    st = float_to_fp16(s->mm01, &out->mm01, error);
    if (st != SEMU_OK) return st;
    st = float_to_fp16(s->mm02, &out->mm02, error);
    if (st != SEMU_OK) return st;
    st = float_to_fp16(s->mm10, &out->mm10, error);
    if (st != SEMU_OK) return st;
    st = float_to_fp16(s->mm11, &out->mm11, error);
    if (st != SEMU_OK) return st;
    return float_to_fp16(s->mm12, &out->mm12, error);
}

int tsc6a_target_state(const nema_draw_snapshot *s)
{
    return tsc6a_bounded_sram(s->target_base, 1u) &&
           s->target_format == NEMA_FMT_TSC6A &&
           s->target_stride == 0x05a0u &&
           s->target_width == NEMA_TSC6A_WIDTH &&
           s->target_height == NEMA_TSC6A_HEIGHT &&
           tsc6a_ordered_clip(s, NEMA_TSC6A_WIDTH, NEMA_TSC6A_HEIGHT);
}

int tsc6a_rgb_target_state(const nema_draw_snapshot *s)
{
    return tsc6a_bounded_sram(s->target_base, 480u * 240u * 2u) &&
           s->target_format == NEMA_FMT_RGB565 &&
           s->target_stride == 0x01e0u &&
           s->target_width == 240u && s->target_height == 240u &&
           s->matmult == TSC6A_OBSERVED_MATMULT &&
           s->codeptr == TSC6A_OBSERVED_CODE &&
           tsc6a_ordered_clip(s, 1024u, 1024u);
}

int tsc6a_resolve_state(const nema_draw_snapshot *s)
{
    uint32_t accent;
    if (!tsc6a_bounded_sram(s->src_base, 1u) ||
        !tsc6a_bounded_sram(s->target_base, 480u * 240u * 2u) ||
        s->src_format != NEMA_FMT_TSC6A || s->src_sampling != 1u ||
        s->src_stride != 0x05a0u || s->src_width != NEMA_TSC6A_WIDTH ||
        s->src_height != NEMA_TSC6A_HEIGHT ||
        s->target_format != NEMA_FMT_RGB565 ||
        s->target_stride != 0x01e0u || s->target_width != 240u ||
        s->target_height != 240u || s->matmult != 0u ||
        (s->codeptr != TSC6A_RESOLVE_CODE &&
         s->codeptr != TSC6A_RESOLVE_CODE_ALT) ||
        s->imem_addr != TSC6A_IMEM_ADDRESS ||
        s->imem_datah != TSC6A_IMEM_DATAH ||
        s->imem_datal != TSC6A_IMEM_DATAL || !s->matrix_present ||
        !tsc6a_ordered_clip(s, 240u, 240u) || s->draw_color != s->tex_color) {
        return 0;
    }
    accent = s->tex_color & 0x00ffffffu;
    return accent == 0x0055ff00u || accent == 0x0000ffffu ||
           accent == 0x0055aaffu;
}

int tsc6a_triangle_points_ok(const nema_draw_snapshot *s)
{
    return tsc6a_coordinate_ok(tsc6a_signed_fp16(s->point0_x)) &&
           tsc6a_coordinate_ok(tsc6a_signed_fp16(s->point0_y)) &&
           tsc6a_coordinate_ok(tsc6a_signed_fp16(s->point1_x)) &&
           tsc6a_coordinate_ok(tsc6a_signed_fp16(s->point1_y)) &&
           tsc6a_coordinate_ok(tsc6a_signed_fp16(s->point2_x)) &&
           tsc6a_coordinate_ok(tsc6a_signed_fp16(s->point2_y));
}

int tsc6a_rectangle(const nema_draw_snapshot *s, int *x0, int *y0,
                    int *x1, int *y1)
{
    int64_t p0x = tsc6a_signed_fp16(s->point0_x);
    int64_t p0y = tsc6a_signed_fp16(s->point0_y);
    int64_t p1x = tsc6a_signed_fp16(s->point1_x);
    int64_t p1y = tsc6a_signed_fp16(s->point1_y);
    int64_t p2x = tsc6a_signed_fp16(s->point2_x);
    int64_t p2y = tsc6a_signed_fp16(s->point2_y);
    int64_t p3x = tsc6a_signed_fp16(s->point3_x);
    int64_t p3y = tsc6a_signed_fp16(s->point3_y);
    if (!tsc6a_coordinate_ok(p0x) || !tsc6a_coordinate_ok(p0y) ||
        !tsc6a_coordinate_ok(p1x) || !tsc6a_coordinate_ok(p1y) ||
        !tsc6a_coordinate_ok(p2x) || !tsc6a_coordinate_ok(p2y) ||
        !tsc6a_coordinate_ok(p3x) || !tsc6a_coordinate_ok(p3y) ||
        s->point1_x != s->point2_x || s->point1_y != s->point0_y ||
        s->point3_x != s->point0_x || s->point3_y != s->point2_y) {
        return 0;
    }
    *x0 = (int)tsc6a_floor_div_fp16(p0x);
    *y0 = (int)tsc6a_floor_div_fp16(p0y);
    *x1 = (int)tsc6a_ceil_div_fp16(p2x);
    *y1 = (int)tsc6a_ceil_div_fp16(p2y);
    return *x1 > *x0 && *y1 > *y0 && *x0 >= -TSC6A_MAX_COORD &&
           *y0 >= -TSC6A_MAX_COORD && *x1 <= TSC6A_MAX_COORD &&
           *y1 <= TSC6A_MAX_COORD;
}

int64_t tsc6a_edge(int64_t ax, int64_t ay, int64_t bx, int64_t by,
                   int64_t px, int64_t py)
{
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

uint32_t tsc6a_blend_argb(uint32_t source, uint32_t destination,
                          uint32_t coverage)
{
    uint32_t source_alpha = ((source >> 24u) * coverage) / 255u;
    uint32_t source_rgb = source & 0x00ffffffu;
    uint32_t destination_rgb = destination & 0x00ffffffu;
    uint32_t inv;
    uint32_t r, g, b, a;

    if (source_alpha == 0u) return destination;
    if ((destination >> 24u) != 0u && source_rgb == destination_rgb) {
        a = (destination >> 24u) + source_alpha;
        if (a > 255u) a = 255u;
        return (a << 24u) | source_rgb;
    }
    inv = 255u - source_alpha;
    r = ((((source >> 16u) & 0xffu) * source_alpha) +
         (((destination >> 16u) & 0xffu) * inv) + 127u) / 255u;
    g = ((((source >> 8u) & 0xffu) * source_alpha) +
         (((destination >> 8u) & 0xffu) * inv) + 127u) / 255u;
    b = (((source & 0xffu) * source_alpha) +
         ((destination & 0xffu) * inv) + 127u) / 255u;
    a = source_alpha + (((destination >> 24u) * inv) + 127u) / 255u;
    return (a << 24u) | (r << 16u) | (g << 8u) | b;
}

semu_status nema_tsc6a_create(nema_tsc6a **out, semu_error *error)
{
    nema_tsc6a *surface;
    if (out == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "nema_tsc6a: null output");
        return SEMU_ERR_ARGUMENT;
    }
    surface = (nema_tsc6a *)calloc(1u, sizeof(*surface));
    if (surface == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "nema_tsc6a: alloc failed");
        return SEMU_ERR_NOMEM;
    }
    surface->pixels = (uint32_t *)calloc(NEMA_TSC6A_PIXELS,
                                         sizeof(surface->pixels[0]));
    if (surface->pixels == NULL) {
        free(surface);
        semu_error_set(error, SEMU_ERR_NOMEM, "nema_tsc6a: pixels alloc failed");
        return SEMU_ERR_NOMEM;
    }
    *out = surface;
    return SEMU_OK;
}

void nema_tsc6a_destroy(nema_tsc6a *surface)
{
    if (surface != NULL) {
        free(surface->pixels);
        free(surface);
    }
}

void nema_tsc6a_reset(nema_tsc6a *surface)
{
    if (surface != NULL) {
        memset(surface->pixels, 0,
               NEMA_TSC6A_PIXELS * sizeof(surface->pixels[0]));
    }
}

void nema_tsc6a_copy(nema_tsc6a *destination, const nema_tsc6a *source)
{
    if (destination != NULL && source != NULL) {
        memcpy(destination->pixels, source->pixels,
               NEMA_TSC6A_PIXELS * sizeof(destination->pixels[0]));
    }
}

semu_status nema_tsc6a_draw_target(nema_tsc6a *surface, semu_bus *bus,
                                   const nema_draw_snapshot *snapshot,
                                   semu_error *error)
{
    if (surface == NULL || bus == NULL || snapshot == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "nema_tsc6a: null draw argument");
        return SEMU_ERR_ARGUMENT;
    }
    if (snapshot->draw_cmd == NEMA_DRAW_TRI_AA) {
        return tsc6a_draw_triangle(surface, snapshot, error);
    }
    if (snapshot->draw_cmd == NEMA_DRAW_QUAD) {
        return tsc6a_draw_mask(surface, bus, snapshot, error);
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "nema_tsc6a: unsupported target draw 0x%08x",
                   snapshot->draw_cmd);
    return SEMU_ERR_UNSUPPORTED;
}
