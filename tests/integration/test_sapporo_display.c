#include "test.h"

#include "../display/nema_backend.h"
#include "../display/panel.h"
#include "../frontends/input_replay.c"

#include "semu/bus.h"
#include "semu/types.h"

#include <string.h>

#define SRAM_BASE 0x10000000u
#define SRAM_SIZE 0x00180000u
#define CMD_BASE  (SRAM_BASE + 0x10000u)
#define OBSERVED_TARGET_BASE 0x1011CF40u
#define INVALID_SOURCE_BASE 0x1017FF00u

#define FSTRIDE_RGB565_240  0x040001E0u
#define RESXY_240x240       0x00F000F0u

static semu_bus *make_bus(semu_error *err)
{
    semu_bus *bus = semu_bus_create(err);
    if (bus != NULL) {
        semu_bus_map_ram(bus, "sram", SRAM_BASE, SRAM_SIZE, err);
    }
    return bus;
}

static void put_u32(uint8_t *buf, uint32_t off, uint32_t val)
{
    buf[off]     = (uint8_t)val;
    buf[off + 1u] = (uint8_t)(val >> 8u);
    buf[off + 2u] = (uint8_t)(val >> 16u);
    buf[off + 3u] = (uint8_t)(val >> 24u);
}

static void append_pair(uint8_t *cmd, uint32_t *word_count,
                        uint32_t reg, uint32_t value)
{
    put_u32(cmd, *word_count * 4u, reg);
    put_u32(cmd, (*word_count + 1u) * 4u, value);
    *word_count += 2u;
}

static uint32_t build_observed_clear(uint8_t *cmd, uint32_t draw_color,
                                     uint32_t codeptr, int with_source)
{
    uint32_t words = 0u;

    memset(cmd, 0, 256u);
    if (with_source) {
        append_pair(cmd, &words, NEMA_REG_TEX1_BASE,
                    INVALID_SOURCE_BASE);
        append_pair(cmd, &words, NEMA_REG_TEX1_FSTRIDE,
                    0x28010010u);
        append_pair(cmd, &words, NEMA_REG_TEX1_RESXY, 0x00400040u);
        append_pair(cmd, &words, NEMA_REG_TEX_COLOR, 0xFF000000u);
    }
    append_pair(cmd, &words, NEMA_REG_TEX0_BASE, OBSERVED_TARGET_BASE);
    append_pair(cmd, &words, NEMA_REG_TEX0_FSTRIDE,
                FSTRIDE_RGB565_240);
    append_pair(cmd, &words, NEMA_REG_TEX0_RESXY, RESXY_240x240);
    append_pair(cmd, &words, NEMA_REG_CLIPMIN, 0u);
    append_pair(cmd, &words, NEMA_REG_CLIPMAX, RESXY_240x240);
    append_pair(cmd, &words, NEMA_REG_POINT0_X, 0u);
    append_pair(cmd, &words, NEMA_REG_POINT0_Y, 0u);
    append_pair(cmd, &words, NEMA_REG_POINT1_X, 0x00F00000u);
    append_pair(cmd, &words, NEMA_REG_POINT1_Y, 0u);
    append_pair(cmd, &words, NEMA_REG_POINT2_X, 0x00F00000u);
    append_pair(cmd, &words, NEMA_REG_POINT2_Y, 0x00F00000u);
    append_pair(cmd, &words, NEMA_REG_POINT3_X, 0u);
    append_pair(cmd, &words, NEMA_REG_POINT3_Y, 0x00F00000u);
    append_pair(cmd, &words, NEMA_REG_MATMULT, 0x90000000u);
    append_pair(cmd, &words, NEMA_REG_CODEPTR, codeptr);
    append_pair(cmd, &words, NEMA_REG_DRAW_COLOR, draw_color);
    append_pair(cmd, &words, NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD);
    return words;
}

static uint16_t first_pixel(const semu_frame *frame)
{
    return (uint16_t)(frame->pixels[0] | (frame->pixels[1] << 8));
}

typedef struct {
    semu_input_event events[8u];
    size_t count;
} replay_sink_ctx;

static int replay_sink(void *context, const semu_input_event *event,
    uint64_t time_ns, uint32_t ordinal)
{
    replay_sink_ctx *ctx = (replay_sink_ctx *)context;
    (void)time_ns;
    (void)ordinal;
    if (ctx->count < 8u) {
        ctx->events[ctx->count] = *event;
    }
    ++ctx->count;
    return 0;
}

static void test_backend_wired(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    semu_nema_backend *backend;
    const semu_frame *frame;
    uint8_t cmd[128u];
    uint32_t w = 0u;
    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    backend = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, backend != NULL);

    memset(cmd, 0, sizeof(cmd));
    put_u32(cmd, w * 4u, NEMA_REG_TEX0_BASE);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, SRAM_BASE + 0x20000u);
    put_u32(cmd, w * 4u, NEMA_REG_TEX0_FSTRIDE);  w += 2u;
    put_u32(cmd, (w - 1u) * 4u, FSTRIDE_RGB565_240);
    put_u32(cmd, w * 4u, NEMA_REG_TEX0_RESXY);    w += 2u;
    put_u32(cmd, (w - 1u) * 4u, RESXY_240x240);
    put_u32(cmd, w * 4u, NEMA_REG_CLIPMIN);      w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0u);
    put_u32(cmd, w * 4u, NEMA_REG_CLIPMAX);       w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x00F000F0u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT0_X);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT0_Y);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT1_X);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x00F00000u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT1_Y);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT2_X);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x00F00000u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT2_Y);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x00F00000u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT3_X);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT3_Y);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x00F00000u);
    put_u32(cmd, w * 4u, NEMA_REG_DRAW_COLOR);   w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x001Fu);
    put_u32(cmd, w * 4u, NEMA_REG_DRAW_CMD);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, NEMA_DRAW_QUAD);

    semu_bus_load(bus, CMD_BASE, cmd, w * 4u, &err);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        semu_nema_backend_submit(backend, bus, CMD_BASE, w, 0u,
                                   NULL, NULL, &err));
    frame = semu_nema_backend_frame(backend);
    SEMU_TEST_ASSERT(context, frame != NULL);
    SEMU_TEST_EQ_U64(context, 240u, frame->width);
    SEMU_TEST_EQ_U64(context, 240u, frame->height);
    SEMU_TEST_EQ_U64(context, 115200u, frame->size);
    {
        uint16_t px = (uint16_t)(frame->pixels[0] |
                                   (frame->pixels[1] << 8));
        SEMU_TEST_EQ_U64(context, 0x001Fu, px);
    }
    semu_nema_backend_destroy(backend);
    semu_bus_destroy(bus);
}

static void test_inherited_source_clear_and_refusal(
    semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    semu_nema_backend *backend;
    const semu_frame *frame;
    uint8_t cmd[256u];
    uint32_t words;

    semu_error_clear(&err);
    bus = make_bus(&err);
    backend = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, bus != NULL && backend != NULL);

    words = build_observed_clear(cmd, 0x001Fu, 0x941EB400u, 0);
    semu_bus_load(bus, CMD_BASE, cmd, words * 4u, &err);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        semu_nema_backend_submit(backend, bus, CMD_BASE, words, 0u,
                                   NULL, NULL, &err));
    frame = semu_nema_backend_frame(backend);
    SEMU_TEST_EQ_U64(context, 0x001Fu, first_pixel(frame));

    /* The native clear must win even when TEX1 was inherited. */
    words = build_observed_clear(cmd, 0xFF000000u, 0x941EB400u, 1);
    semu_bus_load(bus, CMD_BASE, cmd, words * 4u, &err);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        semu_nema_backend_submit(backend, bus, CMD_BASE, words, 0u,
                                   NULL, NULL, &err));
    SEMU_TEST_EQ_U64(context, 0u, first_pixel(frame));
    semu_nema_backend_destroy(backend);
    semu_bus_destroy(bus);

    /* A near miss remains a refused, pixel-atomic transaction. */
    bus = make_bus(&err);
    backend = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, bus != NULL && backend != NULL);
    words = build_observed_clear(cmd, 0x001Fu, 0x941EB400u, 0);
    semu_bus_load(bus, CMD_BASE, cmd, words * 4u, &err);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        semu_nema_backend_submit(backend, bus, CMD_BASE, words, 0u,
                                   NULL, NULL, &err));
    words = build_observed_clear(cmd, 0xFF000000u, 0x941EB401u, 1);
    semu_bus_load(bus, CMD_BASE, cmd, words * 4u, &err);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        semu_nema_backend_submit(backend, bus, CMD_BASE, words, 0u,
                                   NULL, NULL, &err));
    frame = semu_nema_backend_frame(backend);
    SEMU_TEST_EQ_U64(context, 0x001Fu, first_pixel(frame));
    semu_nema_backend_destroy(backend);
    semu_bus_destroy(bus);
}

static void test_replay_pump(semu_test_context *context)
{
    semu_error err;
    semu_input_replay *r;
    replay_sink_ctx sc = {0};
    static const char text[] =
        "1000 button upper press\n"
        "2000 button upper release\n"
        "3000 button lower press\n"
        "4000 button lower release\n";
    semu_error_clear(&err);
    r = semu_input_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_input_replay_parse(r, text, sizeof(text) - 1u, &err));
    SEMU_TEST_EQ_U64(context, 4u,
        semu_input_replay_pump(r, 5000u, replay_sink, &sc, NULL));
    SEMU_TEST_EQ_U64(context, SEMU_BUTTON_UPPER, sc.events[0].code);
    SEMU_TEST_EQ_U64(context, 0u, sc.events[0].value);
    SEMU_TEST_EQ_U64(context, SEMU_BUTTON_LOWER, sc.events[2].code);
    semu_input_replay_destroy(r);
}

static void test_panel_refused(semu_test_context *context)
{
    semu_error err;
    semu_panel *panel;
    uint8_t full[PANEL_BYTES];
    semu_error_clear(&err);
    panel = semu_panel_create(&err);
    SEMU_TEST_ASSERT(context, panel != NULL);
    memset(full, 0xFF, sizeof(full));
    semu_panel_commit_region(panel, 0u, 0u, PANEL_WIDTH, PANEL_HEIGHT,
                               full, PANEL_WIDTH * 2u, &err);
    SEMU_TEST_EQ_U64(context, 1, semu_panel_is_complete(panel));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        semu_panel_publish(panel, NULL, NULL, &err));
    semu_panel_destroy(panel);
}

static void test_two_run_hash(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus1;
    semu_bus *bus2;
    semu_nema_backend *b1;
    semu_nema_backend *b2;
    const semu_frame *f1;
    const semu_frame *f2;
    uint8_t cmd[128u];
    uint32_t w = 0u;
    uint32_t i;
    uint32_t h1 = 0u, h2 = 0u;
    semu_error_clear(&err);

    memset(cmd, 0, sizeof(cmd));
    put_u32(cmd, w * 4u, NEMA_REG_TEX0_BASE);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, SRAM_BASE + 0x20000u);
    put_u32(cmd, w * 4u, NEMA_REG_TEX0_FSTRIDE);  w += 2u;
    put_u32(cmd, (w - 1u) * 4u, FSTRIDE_RGB565_240);
    put_u32(cmd, w * 4u, NEMA_REG_TEX0_RESXY);    w += 2u;
    put_u32(cmd, (w - 1u) * 4u, RESXY_240x240);
    put_u32(cmd, w * 4u, NEMA_REG_CLIPMIN);      w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0u);
    put_u32(cmd, w * 4u, NEMA_REG_CLIPMAX);       w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x00F000F0u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT0_X);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT0_Y);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT1_X);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x00F00000u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT1_Y);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT2_X);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x00F00000u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT2_Y);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x00F00000u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT3_X);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT3_Y);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x00F00000u);
    put_u32(cmd, w * 4u, NEMA_REG_DRAW_COLOR);   w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x07E0u);
    put_u32(cmd, w * 4u, NEMA_REG_DRAW_CMD);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, NEMA_DRAW_QUAD);

    bus1 = make_bus(&err);
    bus2 = make_bus(&err);
    b1 = semu_nema_backend_create(&err);
    b2 = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, b1 != NULL && b2 != NULL);
    semu_bus_load(bus1, CMD_BASE, cmd, w * 4u, &err);
    semu_bus_load(bus2, CMD_BASE, cmd, w * 4u, &err);
    semu_nema_backend_submit(b1, bus1, CMD_BASE, w, 0u, NULL, NULL, &err);
    semu_nema_backend_submit(b2, bus2, CMD_BASE, w, 0u, NULL, NULL, &err);
    f1 = semu_nema_backend_frame(b1);
    f2 = semu_nema_backend_frame(b2);
    for (i = 0u; i < f1->size; ++i) {
        h1 = h1 * 31u + f1->pixels[i];
        h2 = h2 * 31u + f2->pixels[i];
    }
    SEMU_TEST_EQ_U64(context, h1, h2);
    semu_nema_backend_destroy(b1);
    semu_nema_backend_destroy(b2);
    semu_bus_destroy(bus1);
    semu_bus_destroy(bus2);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_backend_wired),
        SEMU_TEST_CASE(test_inherited_source_clear_and_refusal),
        SEMU_TEST_CASE(test_replay_pump),
        SEMU_TEST_CASE(test_panel_refused),
        SEMU_TEST_CASE(test_two_run_hash)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
