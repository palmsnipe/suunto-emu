#include "nema_ulsan.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

#define SIDE ((int)NEMA_ULSAN_SIDE)
#define SRAM_BASE UINT32_C(0x10000000)
#define SRAM_END UINT32_C(0x10267000)
#define PANEL_BYTES (NEMA_ULSAN_PIXELS * 2u)

typedef struct { float x, y; } point;
typedef struct { int x0, y0, x1, y1; } rect;

static int in_sram(uint32_t address, uint32_t size)
{
    return address >= SRAM_BASE && address < SRAM_END &&
           size != 0u && size <= SRAM_END - address;
}

static semu_status refuse(semu_error *error, const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "Race S Nema: %s", reason);
    return SEMU_ERR_UNSUPPORTED;
}

static float fixed16(uint32_t raw)
{
    float value = raw <= INT32_MAX ? (float)raw : -(float)(UINT32_MAX - raw + 1u);
    return value / 65536.0f;
}

static int floor_int(float value)
{
    int integer = (int)value;
    return integer - (value < (float)integer);
}

static int ceil_int(float value)
{
    return -floor_int(-value);
}

static int maximum(int a, int b) { return a > b ? a : b; }
static int minimum(int a, int b) { return a < b ? a : b; }

static void clip_rect(rect *r, const nema_draw_snapshot *d)
{
    r->x0 = maximum(r->x0, (int)d->clip_min_x);
    r->y0 = maximum(r->y0, (int)d->clip_min_y);
    r->x1 = minimum(r->x1, (int)d->clip_max_x);
    r->y1 = minimum(r->y1, (int)d->clip_max_y);
}

/* E-ULS-0084. The reference renderer expands by integer division and
 * rounds both the blend and the RGB565 quantization to nearest. */
static unsigned blend_channel(unsigned src, unsigned dst, unsigned max,
                              unsigned alpha)
{
    unsigned expanded = dst * 255u / max;
    unsigned blended = (src * alpha + expanded * (255u - alpha) + 127u) / 255u;
    return (blended * max + 127u) / 255u;
}

static void blend_pixel(uint8_t *p, uint32_t color, unsigned alpha)
{
    unsigned old = (unsigned)p[0] | (unsigned)p[1] << 8u;
    unsigned red = blend_channel((color >> 16u) & 255u, (old >> 11u) & 31u, 31u, alpha);
    unsigned green = blend_channel((color >> 8u) & 255u, (old >> 5u) & 63u, 63u, alpha);
    unsigned blue = blend_channel(color & 255u, old & 31u, 31u, alpha);
    unsigned packed = red << 11u | green << 5u | blue;
    p[0] = (uint8_t)packed;
    p[1] = (uint8_t)(packed >> 8u);
}

static int dirty_pixel(const nema_ulsan *panel, unsigned index)
{
    return (panel->dirty[index / 8u] & (1u << (index & 7u))) != 0u;
}

static void mark_pixel(nema_ulsan *panel, unsigned index)
{
    panel->dirty[index / 8u] |= (uint8_t)(1u << (index & 7u));
}

static float edge(point a, point b, float x, float y)
{
    return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
}

static int triangle_sample(const point p[3], float area, float x, float y)
{
    float a = edge(p[0], p[1], x, y), b = edge(p[1], p[2], x, y);
    float c = edge(p[2], p[0], x, y);
    return area > 0.0f ? a >= 0.0f && b >= 0.0f && c >= 0.0f :
                        a <= 0.0f && b <= 0.0f && c <= 0.0f;
}

static int a2_source(const nema_draw_snapshot *d)
{
    return d->src_present && d->src_format == NEMA_FMT_A2LE &&
           d->src_sampling == 1u && d->src_width > 0u && d->src_width <= NEMA_ULSAN_SIDE &&
           d->src_height > 0u && d->src_height <= NEMA_ULSAN_SIDE &&
           d->src_stride >= (d->src_width + 3u) / 4u &&
           d->src_stride <= (NEMA_ULSAN_SIDE + 3u) / 4u &&
           in_sram(d->src_base, d->src_stride * d->src_height);
}

static semu_status draw_triangle(nema_ulsan *panel, uint8_t *pixels,
    const nema_draw_snapshot *d, semu_error *error)
{
    const point p[] = {
        {fixed16(d->point0_x), fixed16(d->point0_y)},
        {fixed16(d->point1_x), fixed16(d->point1_y)},
        {fixed16(d->point2_x), fixed16(d->point2_y)}
    };
    const float area = edge(p[0], p[1], p[2].x, p[2].y);
    rect r = {floor_int(p[0].x), floor_int(p[0].y), ceil_int(p[0].x), ceil_int(p[0].y)};
    int opaque_band = d->draw_color == UINT32_C(0xff000000) &&
        d->clip_min_x == 0u && d->clip_max_x == NEMA_ULSAN_SIDE;
    unsigned i;
    int x, y;

    if (d->matmult != UINT32_C(0x90000000) || d->codeptr != UINT32_C(0x941eb400))
        return refuse(error, "triangle program is unobserved");
    for (i = 0u; i < SEMU_ARRAY_LEN(p); ++i) {
        if (p[i].x < -SIDE || p[i].x > SIDE * 2 ||
            p[i].y < -SIDE || p[i].y > SIDE * 2)
            return refuse(error, "triangle coordinates are outside the observed domain");
        opaque_band &= p[i].x >= 0.0f && p[i].x <= SIDE &&
            (p[i].y == (float)d->clip_min_y || p[i].y == (float)d->clip_max_y);
        r.x0 = minimum(r.x0, floor_int(p[i].x));
        r.y0 = minimum(r.y0, floor_int(p[i].y));
        r.x1 = maximum(r.x1, ceil_int(p[i].x));
        r.y1 = maximum(r.y1, ceil_int(p[i].y));
    }
    if (!opaque_band && !a2_source(d))
        return refuse(error, "triangle source state is unobserved");
    if (area > -0.0001f && area < 0.0001f) return SEMU_OK;
    clip_rect(&r, d);
    for (y = r.y0; y < r.y1; ++y) for (x = r.x0; x < r.x1; ++x) {
        unsigned index = (unsigned)y * NEMA_ULSAN_SIDE + (unsigned)x;
        unsigned alpha = d->draw_color >> 24u;
        unsigned covered;
        mark_pixel(panel, index);
        if (opaque_band) {
            covered = (unsigned)triangle_sample(p, area, x + 0.5f, y + 0.5f);
        } else {
            covered = (unsigned)(triangle_sample(p, area, x + 0.25f, y + 0.25f) +
                triangle_sample(p, area, x + 0.75f, y + 0.25f) +
                triangle_sample(p, area, x + 0.25f, y + 0.75f) +
                triangle_sample(p, area, x + 0.75f, y + 0.75f));
            alpha = (alpha * covered + 2u) / 4u;
        }
        if (covered != 0u) blend_pixel(pixels + index * 2u, d->draw_color, alpha);
    }
    return SEMU_OK;
}

static unsigned sample_a2(const uint8_t *source, const nema_draw_snapshot *d, int x, int y)
{
    if (x < 0 || y < 0 || x >= (int)d->src_width || y >= (int)d->src_height)
        return 0u;
    return ((source[(size_t)y * d->src_stride + (unsigned)x / 4u] >>
            ((unsigned)x % 4u * 2u)) & 3u) * 85u;
}

static unsigned bilinear(const uint8_t *source, const nema_draw_snapshot *d, float u, float v)
{
    int x, y;
    float fx, fy, a;
    if (u <= -1.0f || v <= -1.0f || u >= (float)d->src_width || v >= (float)d->src_height)
        return 0u;
    x = floor_int(u);
    y = floor_int(v);
    fx = u - x;
    fy = v - y;
    a = sample_a2(source, d, x, y) * (1.0f - fx) * (1.0f - fy) +
        sample_a2(source, d, x + 1, y) * fx * (1.0f - fy) +
        sample_a2(source, d, x, y + 1) * (1.0f - fx) * fy +
        sample_a2(source, d, x + 1, y + 1) * fx * fy;
    return (unsigned)minimum(255, (int)(a + 0.5f));
}

static semu_status draw_mask(nema_ulsan *panel, semu_bus *bus, uint8_t *pixels,
    const nema_draw_snapshot *d, semu_error *error)
{
    const uint32_t words[] = {d->mm00, d->mm01, d->mm02, d->mm10, d->mm11, d->mm12};
    float m[6];
    uint8_t *source;
    size_t size, i;
    rect r = {floor_int(fixed16(d->point0_x)), floor_int(fixed16(d->point0_y)),
              ceil_int(fixed16(d->point2_x)), ceil_int(fixed16(d->point2_y))};
    int x, y;
    semu_status status;

    if (!a2_source(d) || d->matmult != 0u || !d->matrix_present ||
        (d->codeptr != UINT32_C(0x941d8000) && d->codeptr != UINT32_C(0x941e8000)) ||
        r.x0 < -1024 || r.y0 < -1024 || r.x1 > 1024 || r.y1 > 1024 ||
        r.x0 >= r.x1 || r.y0 >= r.y1 || d->point1_x != d->point2_x ||
        d->point1_y != d->point0_y || d->point3_x != d->point0_x || d->point3_y != d->point2_y)
        return refuse(error, "A2 mask shape or program is unobserved");
    if (sizeof(float) != sizeof(uint32_t) || FLT_RADIX != 2 || FLT_MANT_DIG != 24)
        return refuse(error, "A2 mask requires IEEE binary32");
    memcpy(m, words, sizeof(m));
    for (i = 0u; i < SEMU_ARRAY_LEN(m); ++i)
        if (!(m[i] >= -65536.0f && m[i] <= 65536.0f))
            return refuse(error, "A2 transform is nonfinite or out of range");
    clip_rect(&r, d);
    if (r.x0 >= r.x1 || r.y0 >= r.y1) return SEMU_OK;
    size = (size_t)d->src_stride * d->src_height;
    source = malloc(size);
    if (source == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "Race S Nema: cannot stage A2 texture");
        return SEMU_ERR_NOMEM;
    }
    status = semu_bus_copy_out(bus, d->src_base, source, size, error);
    if (status != SEMU_OK) {
        free(source);
        return status;
    }
    for (i = 0u; i < size; ++i) {
        uint32_t address = d->src_base + (uint32_t)i;
        if (address >= panel->target && address - panel->target < PANEL_BYTES &&
            dirty_pixel(panel, (address - panel->target) / 2u))
            source[i] = pixels[address - panel->target];
    }
    for (y = r.y0; y < r.y1; ++y) for (x = r.x0; x < r.x1; ++x) {
        unsigned index = (unsigned)y * NEMA_ULSAN_SIDE + (unsigned)x;
        float u = m[0] * x + m[1] * y + m[2];
        float v = m[3] * x + m[4] * y + m[5];
        unsigned alpha = (bilinear(source, d, u, v) * (d->tex_color >> 24u) + 127u) / 255u;
        mark_pixel(panel, index);
        if (alpha != 0u) blend_pixel(pixels + index * 2u, d->tex_color, alpha);
    }
    free(source);
    return SEMU_OK;
}

void nema_ulsan_begin(nema_ulsan *panel, semu_bus *bus)
{
    panel->bus = bus;
    panel->target = 0u;
    panel->pending_bands = panel->bands;
    memset(panel->dirty, 0, sizeof(panel->dirty));
}

semu_status nema_ulsan_draw(nema_ulsan *panel, semu_bus *bus, uint8_t *pixels,
    const nema_draw_snapshot *draw, semu_error *error)
{
    if (draw->target_format != NEMA_FMT_RGB565 || draw->target_sampling != 0u ||
        draw->target_stride != NEMA_ULSAN_SIDE * 2u ||
        draw->target_width != NEMA_ULSAN_SIDE || draw->target_height != NEMA_ULSAN_SIDE ||
        (draw->target_base & 1u) != 0u || !in_sram(draw->target_base, PANEL_BYTES) ||
        (panel->target != 0u && panel->target != draw->target_base) ||
        draw->clip_min_x >= draw->clip_max_x || draw->clip_max_x > NEMA_ULSAN_SIDE ||
        draw->clip_min_y >= draw->clip_max_y || draw->clip_max_y > NEMA_ULSAN_SIDE ||
        draw->imem_addr != 0u || draw->imem_datah != UINT32_C(0x004e0002) ||
        draw->imem_datal != UINT32_C(0x804b1286))
        return refuse(error, "target, clip or shader state is unobserved");
    panel->target = draw->target_base;
    panel->list_drew = 1;
    panel->list_x0 = draw->clip_min_x;
    panel->list_y0 = draw->clip_min_y;
    panel->list_x1 = draw->clip_max_x;
    panel->list_y1 = draw->clip_max_y;
    if (draw->draw_cmd == NEMA_DRAW_TRI_SOLID)
        return draw_triangle(panel, pixels, draw, error);
    if (draw->draw_cmd == NEMA_DRAW_QUAD)
        return draw_mask(panel, bus, pixels, draw, error);
    return refuse(error, "draw command is unobserved");
}

int nema_ulsan_end_list(nema_ulsan *panel)
{
    if (!panel->list_drew || panel->list_x0 != 0u || panel->list_x1 != NEMA_ULSAN_SIDE)
        return 0;
    if (panel->list_y0 == 0u && panel->list_y1 == 156u)
        panel->pending_bands = 1u;
    else if (panel->list_y0 == 156u && panel->list_y1 == 312u)
        panel->pending_bands |= 2u;
    else if (panel->list_y0 == 312u && panel->list_y1 == NEMA_ULSAN_SIDE)
        panel->pending_bands |= 4u;
    else {
        panel->pending_bands = 0u;
        return 1;
    }
    return panel->pending_bands == 7u;
}

semu_status nema_ulsan_validate(const nema_ulsan *panel, semu_error *error)
{
    unsigned i;
    if (panel->target == 0u) return SEMU_OK;
    for (i = 0u; i < NEMA_ULSAN_PIXELS; ++i) if (dirty_pixel(panel, i)) {
        semu_status status = semu_bus_validate_write(panel->bus, panel->target + i * 2u, 2u, error);
        if (status != SEMU_OK) return status;
    }
    return SEMU_OK;
}

void nema_ulsan_commit(nema_ulsan *panel, const uint8_t *pixels)
{
    unsigned i;
    panel->bands = panel->pending_bands;
    if (panel->target == 0u) return;
    for (i = 0u; i < NEMA_ULSAN_PIXELS; ++i) if (dirty_pixel(panel, i)) {
        uint32_t word = (uint32_t)pixels[i * 2u] | (uint32_t)pixels[i * 2u + 1u] << 8u;
        (void)semu_bus_write(panel->bus, panel->target + i * 2u, 2u, word, NULL);
    }
}
