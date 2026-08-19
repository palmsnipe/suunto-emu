/*
 * Affine A2LE masked draw (ticket 512).
 * Implements the observed NEMA binary32 matrix path with fixed-point
 * validation, transparent out-of-range source samples, and atomic output.
 */

#include "sampling.h"

#include <limits.h>
#include <stdlib.h>

typedef struct {
    int32_t mm00, mm01, mm02;
    int32_t mm10, mm11, mm12;
} fixed_affine_matrix;

/* Convert a finite binary32 value to signed 16.16 without host FP. */
static semu_status matrix_float_to_fixed(uint32_t bits, int32_t *out,
                                         semu_error *error)
{
    uint32_t exponent = (bits >> 23u) & 0xFFu;
    uint32_t fraction = bits & 0x7FFFFFu;
    uint64_t significand;
    uint64_t magnitude;
    uint64_t limit;
    int exponent_value;
    int shift;
    int negative = (bits >> 31u) != 0u;

    if (exponent == 0xFFu) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "draw_mask_affine: non-finite matrix 0x%08x", bits);
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
        significand = (uint64_t)(0x800000u | fraction);
    }

    shift = exponent_value - 7;
    if (shift >= 64) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "draw_mask_affine: matrix out of range 0x%08x",
                       bits);
        return SEMU_ERR_RANGE;
    }
    if (shift >= 0) {
        if (significand > (UINT64_MAX >> (unsigned)shift)) {
            semu_error_set(error, SEMU_ERR_RANGE,
                           "draw_mask_affine: matrix out of range 0x%08x",
                           bits);
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
                       "draw_mask_affine: matrix out of range 0x%08x",
                       bits);
        return SEMU_ERR_RANGE;
    }
    if (negative) {
        *out = (magnitude == UINT64_C(2147483648))
            ? INT32_MIN : -(int32_t)magnitude;
    } else {
        *out = (int32_t)magnitude;
    }
    return SEMU_OK;
}

static semu_status matrix_to_fixed(const nema_affine_matrix *matrix,
                                   fixed_affine_matrix *out,
                                   semu_error *error)
{
    semu_status st;

    st = matrix_float_to_fixed(matrix->mm00, &out->mm00, error);
    if (st != SEMU_OK) return st;
    st = matrix_float_to_fixed(matrix->mm01, &out->mm01, error);
    if (st != SEMU_OK) return st;
    st = matrix_float_to_fixed(matrix->mm02, &out->mm02, error);
    if (st != SEMU_OK) return st;
    st = matrix_float_to_fixed(matrix->mm10, &out->mm10, error);
    if (st != SEMU_OK) return st;
    st = matrix_float_to_fixed(matrix->mm11, &out->mm11, error);
    if (st != SEMU_OK) return st;
    return matrix_float_to_fixed(matrix->mm12, &out->mm12, error);
}

static int64_t floor_fp16(int64_t value)
{
    if (value >= 0) return value / 65536;
    return -((-value + 65535) / 65536);
}

static semu_status affine_alpha_at(semu_bus *bus,
                                   const nema_texture_desc *mask,
                                   int64_t x, int64_t y, uint8_t *alpha,
                                   semu_error *error)
{
    if (x < 0 || y < 0 || x >= (int64_t)mask->width ||
        y >= (int64_t)mask->height) {
        *alpha = 0u;
        return SEMU_OK;
    }
    return nema_a2le_sample(bus, mask->base, mask->stride, mask->width,
                            mask->height, (uint32_t)x, (uint32_t)y,
                            alpha, error);
}

static semu_status affine_sample_alpha(semu_bus *bus,
                                       const nema_texture_desc *mask,
                                       int64_t x_fp16, int64_t y_fp16,
                                       uint8_t *alpha, semu_error *error)
{
    int64_t x0 = floor_fp16(x_fp16);
    int64_t y0 = floor_fp16(y_fp16);
    int64_t x1 = x0 + 1;
    int64_t y1 = y0 + 1;
    uint32_t fx = (uint32_t)(x_fp16 - x0 * 65536);
    uint32_t fy = (uint32_t)(y_fp16 - y0 * 65536);
    uint64_t ix = 65536u - fx;
    uint64_t iy = 65536u - fy;
    uint8_t a00, a10, a01, a11;
    uint64_t weighted;
    semu_status st;

    st = affine_alpha_at(bus, mask, x0, y0, &a00, error);
    if (st != SEMU_OK) return st;
    st = affine_alpha_at(bus, mask, x1, y0, &a10, error);
    if (st != SEMU_OK) return st;
    st = affine_alpha_at(bus, mask, x0, y1, &a01, error);
    if (st != SEMU_OK) return st;
    st = affine_alpha_at(bus, mask, x1, y1, &a11, error);
    if (st != SEMU_OK) return st;

    weighted = (uint64_t)a00 * ix * iy + (uint64_t)a10 * fx * iy +
               (uint64_t)a01 * ix * fy + (uint64_t)a11 * fx * fy;
    weighted = (weighted + UINT64_C(0x80000000)) >> 32u;
    *alpha = (weighted > 255u) ? 255u : (uint8_t)weighted;
    return SEMU_OK;
}

semu_status draw_mask_affine(raster_target *target,
                             const raster_bounds *clip,
                             semu_bus *bus,
                             const nema_texture_desc *mask,
                             uint32_t dst_x, uint32_t dst_y,
                             uint32_t w, uint32_t h,
                             const nema_affine_matrix *matrix,
                             uint32_t blend_mode,
                             uint32_t tex_color,
                             raster_bounds *dirty,
                             semu_error *error)
{
    raster_bounds region;
    int32_t unused_dx, unused_dy;
    fixed_affine_matrix fixed;
    uint32_t rw, rh, row, col;
    uint16_t *stage;
    rgb8 tint;
    uint8_t tint_alpha;
    semu_status st;

    if (bus == NULL || mask == NULL || matrix == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "draw_mask_affine: null");
        return SEMU_ERR_ARGUMENT;
    }
    if (blend_mode != NEMA_BL_SIMPLE) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "draw_mask_affine: blend mode 0x%x unsupported",
                       blend_mode);
        return SEMU_ERR_UNSUPPORTED;
    }
    if (mask->format != NEMA_TEX_FMT_A2LE ||
        mask->sampling != NEMA_TEX_SAMPLING_BILINEAR) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "draw_mask_affine: unsupported A2LE state");
        return SEMU_ERR_UNSUPPORTED;
    }

    st = matrix_to_fixed(matrix, &fixed, error);
    if (st != SEMU_OK) return st;
    st = sampling_compute_clip(target, clip, dst_x, dst_y, w, h, &region,
                               &unused_dx, &unused_dy, error);
    if (st != SEMU_OK) return st;

    rw = region.max_x - region.min_x;
    rh = region.max_y - region.min_y;
    if (rw == 0u || rh == 0u) return SEMU_OK;
    if (rw > SAMPLING_MAX_DIM || rh > SAMPLING_MAX_DIM) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "draw_mask_affine: region %ux%u exceeds max",
                       rw, rh);
        return SEMU_ERR_UNSUPPORTED;
    }

    st = nema_texture_validate(bus, mask, error);
    if (st != SEMU_OK) return st;

    stage = (uint16_t *)calloc((size_t)rw * rh, sizeof(uint16_t));
    if (stage == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "draw_mask_affine: staging alloc");
        return SEMU_ERR_NOMEM;
    }

    tint = blend_tint_from_tex_color(tex_color);
    tint_alpha = (uint8_t)(tex_color >> 24u);
    for (row = 0u; row < rh; ++row) {
        for (col = 0u; col < rw; ++col) {
            uint32_t tx = region.min_x + col;
            uint32_t ty = region.min_y + row;
            int64_t u = (int64_t)fixed.mm00 * tx +
                        (int64_t)fixed.mm01 * ty + fixed.mm02;
            int64_t v = (int64_t)fixed.mm10 * tx +
                        (int64_t)fixed.mm11 * ty + fixed.mm12;
            uint8_t sampled_alpha;
            uint8_t alpha;
            size_t dst_off = (size_t)ty * target->stride + tx * 2u;
            uint16_t dst_pixel;

            dst_pixel = (uint16_t)(target->pixels[dst_off] |
                                    (target->pixels[dst_off + 1u] << 8));
            st = affine_sample_alpha(bus, mask, u, v, &sampled_alpha,
                                     error);
            if (st != SEMU_OK) {
                free(stage);
                return st;
            }
            alpha = (uint8_t)(((uint32_t)sampled_alpha * tint_alpha + 127u) /
                              255u);
            if (alpha == 0u) {
                stage[row * rw + col] = dst_pixel;
            } else {
                rgb8 dst_px;
                blend_unpack_rgb565(dst_pixel, &dst_px);
                st = blend_simple(blend_mode, tint, dst_px, alpha,
                                  &stage[row * rw + col], error);
                if (st != SEMU_OK) {
                    free(stage);
                    return st;
                }
            }
        }
    }

    for (row = 0u; row < rh; ++row) {
        for (col = 0u; col < rw; ++col) {
            uint32_t tx = region.min_x + col;
            uint32_t ty = region.min_y + row;
            size_t off = (size_t)ty * target->stride + tx * 2u;
            uint16_t pixel = stage[row * rw + col];
            target->pixels[off] = (uint8_t)pixel;
            target->pixels[off + 1u] = (uint8_t)(pixel >> 8u);
        }
    }

    free(stage);
    if (dirty != NULL) *dirty = region;
    return SEMU_OK;
}
