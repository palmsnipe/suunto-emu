#include "nema_rgba4444.h"
#include "semu/fpu_math.h"
#include <stdlib.h>
#include <string.h>

#define SOURCE_BYTES 46080u
#define TARGET_BYTES 115200u
#define ONE UINT32_C(0x3f800000)
#define HALF UINT32_C(0x3f000000)
typedef struct { unsigned r, g, b, a; } rgba;

/* Every operation rounds separately, matching the lane's binary32 expression.
 * FPSCR never comes from, or flows back into, guest CPU state. */
static uint32_t add(uint32_t a, uint32_t b)
{ return semu_fpu_add_bits(a, b, 0u).bits; }
static uint32_t sub(uint32_t a, uint32_t b)
{ return semu_fpu_sub_bits(a, b, 0u).bits; }
static uint32_t mul(uint32_t a, uint32_t b)
{ return semu_fpu_mul_bits(a, b, 0u).bits; }
static uint32_t from_int(int value)
{ uint32_t flags = 0; return semu_fpu_from_int_bits((uint32_t)value, 0u, &flags); }
static int finite(uint32_t bits)
{ return (bits & UINT32_C(0x7f800000)) != UINT32_C(0x7f800000); }
static int bounded(uint32_t base, uint32_t size)
{ return base >= 0x10000000u && base <= 0x10180000u - size; }
static int fixed_edge(uint32_t bits, int ceiling)
{
    int64_t value = bits <= INT32_MAX ? (int64_t)bits :
        (int64_t)bits - INT64_C(4294967296);
    int whole = (int)(value / 65536);
    if (value % 65536 != 0) whole += ceiling ? (value > 0) : -(value < 0);
    return whole;
}
static int validate(const nema_draw_snapshot *s, int *x0, int *y0,
    int *x1, int *y1)
{
    if (s->draw_cmd != NEMA_DRAW_QUAD || !s->src_present || !s->matrix_present ||
        s->src_format != NEMA_FMT_RGBA4444 || s->src_sampling != 1u ||
        s->src_stride != 480u || s->src_width != 240u || s->src_height != 96u ||
        s->target_format != NEMA_FMT_RGB565 || s->target_sampling != 0u ||
        s->target_stride != 480u || s->target_width != 240u || s->target_height != 240u ||
        s->matmult != 0u || s->codeptr != 0x941e8000u || s->imem_addr != 0u ||
        s->imem_datah != 0x004e0002u || s->imem_datal != 0x804b1286u ||
        s->tex_color != 0xffffffffu || !bounded(s->src_base, SOURCE_BYTES) ||
        !bounded(s->target_base, TARGET_BYTES) ||
        (s->src_base < s->target_base + TARGET_BYTES &&
         s->target_base < s->src_base + SOURCE_BYTES) ||
        s->clip_min_x > s->clip_max_x || s->clip_min_y > s->clip_max_y ||
        s->clip_max_x > 240u || s->clip_max_y > 240u ||
        s->point1_x != s->point2_x || s->point1_y != s->point0_y ||
        s->point3_x != s->point0_x || s->point3_y != s->point2_y ||
        !finite(s->mm00) || !finite(s->mm01) || !finite(s->mm02) ||
        !finite(s->mm10) || !finite(s->mm11) || !finite(s->mm12)) return 0;
    *x0 = fixed_edge(s->point0_x, 0); *y0 = fixed_edge(s->point0_y, 0);
    *x1 = fixed_edge(s->point2_x, 1); *y1 = fixed_edge(s->point2_y, 1);
    if (*x0 < -1024 || *y0 < -1024 || *x1 > 1024 || *y1 > 1024 ||
        *x1 <= *x0 || *y1 <= *y0) return 0;
    if (*x0 < (int)s->clip_min_x) *x0 = (int)s->clip_min_x;
    if (*y0 < (int)s->clip_min_y) *y0 = (int)s->clip_min_y;
    if (*x1 > (int)s->clip_max_x) *x1 = (int)s->clip_max_x;
    if (*y1 > (int)s->clip_max_y) *y1 = (int)s->clip_max_y;
    return 1;
}
static rgba texel(const uint8_t *source, int x, int y)
{
    rgba result = {0, 0, 0, 0}; unsigned p; size_t offset;
    if (x < 0 || y < 0 || x >= 240 || y >= 96) return result;
    offset = (size_t)y * 480u + (size_t)x * 2u;
    p = source[offset] | (unsigned)source[offset + 1u] << 8u;
    result.r = ((p >> 12u) & 15u) * 17u;
    result.g = ((p >> 8u) & 15u) * 17u;
    result.b = ((p >> 4u) & 15u) * 17u;
    result.a = (p & 15u) * 17u;
    return result;
}
/* Reject coordinates with no contributing texel before integer conversion.
 * The remaining domain is [-1, extent), so conversion cannot overflow. */
static int coordinate(uint32_t bits, int extent, int *whole, uint32_t *fraction)
{
    uint32_t magnitude = bits & 0x7fffffffu, flags = 0;
    if (bits >> 31u) {
        if (magnitude >= ONE) return 0;
        *whole = magnitude == 0u ? 0 : -1;
    } else {
        if (bits >= from_int(extent)) return 0;
        *whole = (int)semu_fpu_to_int_bits(bits, 1u, 1u, &flags);
    }
    *fraction = sub(bits, from_int(*whole));
    return 1;
}
static unsigned interpolate(unsigned a, unsigned b, unsigned c, unsigned d,
    uint32_t fx, uint32_t fy)
{
    uint32_t ix = sub(ONE, fx), iy = sub(ONE, fy), flags = 0, value;
    value = add(mul(mul(from_int((int)a), ix), iy),
                mul(mul(from_int((int)b), fx), iy));
    value = add(value, mul(mul(from_int((int)c), ix), fy));
    value = add(value, mul(mul(from_int((int)d), fx), fy));
    value = semu_fpu_to_int_bits(add(value, HALF), 1u, 1u, &flags);
    return value > 255u ? 255u : value;
}
static rgba sample(const uint8_t *source, uint32_t u, uint32_t v)
{
    rgba a, b, c, d, result = {0, 0, 0, 0}; int x, y; uint32_t fx, fy;
    if (!coordinate(u, 240, &x, &fx) || !coordinate(v, 96, &y, &fy)) return result;
    a = texel(source, x, y);
    if ((fx & 0x7fffffffu) == 0u && (fy & 0x7fffffffu) == 0u) return a;
    b = texel(source, x + 1, y); c = texel(source, x, y + 1);
    d = texel(source, x + 1, y + 1);
    result.r = interpolate(a.r, b.r, c.r, d.r, fx, fy);
    result.g = interpolate(a.g, b.g, c.g, d.g, fx, fy);
    result.b = interpolate(a.b, b.b, c.b, d.b, fx, fy);
    result.a = interpolate(a.a, b.a, c.a, d.a, fx, fy);
    return result;
}
static void blend(uint8_t *pixels, rgba color)
{
    unsigned p = pixels[0] | (unsigned)pixels[1] << 8u, a = color.a;
    unsigned r, g, b;
    if (a == 0u) return;
    r = (color.r * a + (((p >> 11u) & 31u) * 255u / 31u) * (255u - a) + 127u) / 255u;
    g = (color.g * a + (((p >> 5u) & 63u) * 255u / 63u) * (255u - a) + 127u) / 255u;
    b = (color.b * a + ((p & 31u) * 255u / 31u) * (255u - a) + 127u) / 255u;
    p = (((r * 31u + 127u) / 255u) << 11u) |
        (((g * 63u + 127u) / 255u) << 5u) | ((b * 31u + 127u) / 255u);
    pixels[0] = (uint8_t)p; pixels[1] = (uint8_t)(p >> 8u);
}
semu_status nema_rgba4444_draw(semu_bus *bus, const nema_draw_snapshot *s,
    uint8_t *pixels, uint32_t stride, semu_error *error)
{
    uint8_t *scratch, *source, *staged; int x0, y0, x1, y1, x, y;
    semu_status status; uint32_t offset;
    semu_error_clear(error);
    if (!bus || !s || !pixels || stride != 480u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "rgba4444: invalid target");
        return SEMU_ERR_ARGUMENT;
    }
    if (!validate(s, &x0, &y0, &x1, &y1)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "rgba4444: unsupported draw state");
        return SEMU_ERR_UNSUPPORTED;
    }
    scratch = malloc(SOURCE_BYTES + TARGET_BYTES);
    if (!scratch) {
        semu_error_set(error, SEMU_ERR_NOMEM, "rgba4444: cannot stage draw");
        return SEMU_ERR_NOMEM;
    }
    source = scratch; staged = scratch + SOURCE_BYTES;
    /* A whole-range copy could bypass a narrower MMIO overlay. Check each
     * byte with the existing memory-only API, as the other texture paths do. */
    for (offset = 0u; offset < SOURCE_BYTES; ++offset) {
        status = semu_bus_copy_out(bus, s->src_base + offset, source + offset, 1u, error);
        if (status != SEMU_OK) { free(scratch); return status; }
    }
    memcpy(staged, pixels, TARGET_BYTES);
    for (y = y0; y < y1; ++y) for (x = x0; x < x1; ++x) {
        uint32_t xf = from_int(x), yf = from_int(y);
        uint32_t u = add(add(mul(s->mm00, xf), mul(s->mm01, yf)), s->mm02);
        uint32_t v = add(add(mul(s->mm10, xf), mul(s->mm11, yf)), s->mm12);
        if (!finite(u) || !finite(v)) {
            free(scratch);
            semu_error_set(error, SEMU_ERR_RANGE, "rgba4444: nonfinite mapping");
            return SEMU_ERR_RANGE;
        }
        blend(staged + (size_t)y * stride + (size_t)x * 2u, sample(source, u, v));
    }
    memcpy(pixels, staged, TARGET_BYTES);
    free(scratch);
    return SEMU_OK;
}
