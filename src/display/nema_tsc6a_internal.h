#ifndef SEMU_NEMA_TSC6A_INTERNAL_H
#define SEMU_NEMA_TSC6A_INTERNAL_H

#include "nema_tsc6a.h"

#include <stdint.h>

#define TSC6A_SRAM_START UINT32_C(0x10000000)
#define TSC6A_SRAM_END   UINT32_C(0x10180000)
#define TSC6A_TARGET_FSTRIDE UINT32_C(0x170005a0)
#define TSC6A_TARGET_FSTRIDE_SAMPLED UINT32_C(0x170105a0)
#define TSC6A_RESOLUTION UINT32_C(0x01e001e0)
#define TSC6A_RGB_FSTRIDE UINT32_C(0x040001e0)
#define TSC6A_RGB_RESOLUTION UINT32_C(0x00f000f0)
#define TSC6A_OBSERVED_MATMULT UINT32_C(0x90000000)
#define TSC6A_OBSERVED_CODE UINT32_C(0x941eb400)
#define TSC6A_RESOLVE_CODE UINT32_C(0x941e8000)
#define TSC6A_RESOLVE_CODE_ALT UINT32_C(0x941d8000)
#define TSC6A_IMEM_ADDRESS 0u
#define TSC6A_IMEM_DATAH UINT32_C(0x004e0002)
#define TSC6A_IMEM_DATAL UINT32_C(0x804b1286)
#define TSC6A_MAX_COORD 2048
#define TSC6A_FP16_ONE INT64_C(65536)
#define TSC6A_FP8_ONE 256u

struct nema_tsc6a {
    uint32_t *pixels;
};

typedef struct {
    int32_t mm00, mm01, mm02;
    int32_t mm10, mm11, mm12;
} tsc6a_fixed_matrix;

int tsc6a_bounded_sram(uint32_t base, uint32_t size);
int tsc6a_ordered_clip(const nema_draw_snapshot *snapshot, uint32_t width,
                       uint32_t height);
int64_t tsc6a_signed_fp16(uint32_t value);
int64_t tsc6a_floor_div_fp16(int64_t value);
int64_t tsc6a_ceil_div_fp16(int64_t value);
int tsc6a_coordinate_ok(int64_t value);
semu_status tsc6a_snapshot_matrix(const nema_draw_snapshot *snapshot,
                                  tsc6a_fixed_matrix *out,
                                  semu_error *error);
/* Convert a finite binary32 value (raw bits) to signed 16.16 without host
 * FP; refuses non-finite or out-of-range matrices.  Shared by the matrix
 * snapshot and the ticket-788 compressed-asset translation law. */
semu_status tsc6a_float_to_fp16(uint32_t bits, int32_t *out, semu_error *error);
int tsc6a_target_state(const nema_draw_snapshot *snapshot);
int tsc6a_rgb_target_state(const nema_draw_snapshot *snapshot);
int tsc6a_resolve_state(const nema_draw_snapshot *snapshot);
int tsc6a_triangle_points_ok(const nema_draw_snapshot *snapshot);
int tsc6a_rectangle(const nema_draw_snapshot *snapshot, int *x0, int *y0,
                    int *x1, int *y1);
int64_t tsc6a_edge(int64_t ax, int64_t ay, int64_t bx, int64_t by,
                   int64_t px, int64_t py);
uint32_t tsc6a_blend_argb(uint32_t source, uint32_t destination,
                          uint32_t coverage);

/* Pure TSC6A format-17 block expansion (nema_tsc6a_expand.c, ticket 793).
 * Returns 0 (fail closed, output untouched) when any auxiliary bit
 * 75..95 of the block is nonzero; otherwise writes 16 RGBA8888 texels
 * (pixel p = 4*r + c, row 0 first) and returns 1.  Law:
 * E-RE-SAP235-TSC6A-001. */
int tsc6a_expand_block(const uint8_t blk[12], uint8_t out[16][4]);

semu_status tsc6a_draw_triangle(nema_tsc6a *surface,
                                const nema_draw_snapshot *snapshot,
                                semu_error *error);
semu_status tsc6a_draw_mask(nema_tsc6a *surface, semu_bus *bus,
                            const nema_draw_snapshot *snapshot,
                            semu_error *error);

#endif
