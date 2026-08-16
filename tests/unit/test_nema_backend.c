#include "../../src/display/nema_backend.h"
#include "../../src/display/nema_state.h"
#include "test.h"

#include "semu/bus.h"
#include "semu/types.h"

#include <string.h>

#define SRAM_BASE 0x10000000u
#define SRAM_SIZE 0x00100000u
#define CMD_BASE  (SRAM_BASE + 0x10000u)
#define TEX_BASE  (SRAM_BASE + 0x20000u)

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

static void load_cmd(semu_bus *bus, const uint8_t *buf, uint32_t bytes,
                     semu_error *err)
{
    semu_bus_load(bus, CMD_BASE, buf, bytes, err);
}

static void test_null_path(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    semu_nema_backend *backend;
    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    backend = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, backend != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        semu_nema_backend_submit(backend, bus, CMD_BASE, 0u, 0u,
                                   NULL, NULL, &err));
    semu_nema_backend_destroy(backend);
    semu_bus_destroy(bus);
}

static void test_clear_draw(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    semu_nema_backend *backend;
    const semu_frame *frame;
    uint8_t cmd[128u];
    uint32_t cmd_words = 0u;
    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    backend = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, backend != NULL);

    memset(cmd, 0, sizeof(cmd));
    put_u32(cmd, cmd_words * 4u, NEMA_REG_TEX0_BASE);
    put_u32(cmd, (cmd_words + 1u) * 4u, TEX_BASE);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_TEX0_FSTRIDE);
    put_u32(cmd, (cmd_words + 1u) * 4u, FSTRIDE_RGB565_240);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_TEX0_RESXY);
    put_u32(cmd, (cmd_words + 1u) * 4u, RESXY_240x240);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_CLIPMIN);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_CLIPMAX);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x00F000F0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT0_X);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT0_Y);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT1_X);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x00F00000u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT1_Y);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT2_X);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x00F00000u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT2_Y);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x00F00000u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT3_X);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT3_Y);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x00F00000u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_DRAW_COLOR);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x001Fu);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_DRAW_CMD);
    put_u32(cmd, (cmd_words + 1u) * 4u, NEMA_DRAW_QUAD);
    cmd_words += 2u;

    load_cmd(bus, cmd, cmd_words * 4u, &err);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        semu_nema_backend_submit(backend, bus, CMD_BASE, cmd_words,
                                   0u, NULL, NULL, &err));
    frame = semu_nema_backend_frame(backend);
    SEMU_TEST_ASSERT(context, frame != NULL);
    SEMU_TEST_EQ_U64(context, 0u, frame->generation);
    {
        uint16_t px = (uint16_t)(frame->pixels[0] |
                                   (frame->pixels[1] << 8));
        SEMU_TEST_EQ_U64(context, 0x001Fu, px);
    }
    semu_nema_backend_destroy(backend);
    semu_bus_destroy(bus);
}

static void test_refusal_no_partial(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    semu_nema_backend *backend;
    const semu_frame *frame;
    uint8_t cmd[8u];
    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    backend = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, backend != NULL);

    /* Bad prefix: 0x55 is neither 0x00 nor 0xFF */
    memset(cmd, 0, sizeof(cmd));
    put_u32(cmd, 0u, 0x55000004u);
    put_u32(cmd, 4u, 0x12345678u);
    load_cmd(bus, cmd, 8u, &err);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        semu_nema_backend_submit(backend, bus, CMD_BASE, 2u,
                                   0u, NULL, NULL, &err));
    frame = semu_nema_backend_frame(backend);
    SEMU_TEST_ASSERT(context, frame != NULL);
    /* Surface should be unchanged (all zeros from creation) */
    SEMU_TEST_EQ_U64(context, 0u, frame->pixels[0]);
    semu_nema_backend_destroy(backend);
    semu_bus_destroy(bus);
}

static void test_reset(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    semu_nema_backend *backend;
    const semu_frame *frame;
    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    backend = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, backend != NULL);

    frame = semu_nema_backend_frame(backend);
    SEMU_TEST_ASSERT(context, frame != NULL);
    SEMU_TEST_EQ_U64(context, 0u, frame->generation);

    semu_nema_backend_reset(backend);
    SEMU_TEST_EQ_U64(context, 0u, frame->generation);
    SEMU_TEST_EQ_U64(context, 0u, frame->pixels[0]);

    semu_nema_backend_destroy(backend);
    semu_bus_destroy(bus);
}

static void test_repeat_hash(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus1;
    semu_bus *bus2;
    semu_nema_backend *b1;
    semu_nema_backend *b2;
    const semu_frame *f1;
    const semu_frame *f2;
    uint8_t cmd[128u];
    uint32_t cmd_words = 0u;
    uint32_t i;
    uint32_t h1 = 0u, h2 = 0u;
    semu_error_clear(&err);

    memset(cmd, 0, sizeof(cmd));
    put_u32(cmd, cmd_words * 4u, NEMA_REG_TEX0_BASE);
    put_u32(cmd, (cmd_words + 1u) * 4u, TEX_BASE);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_TEX0_FSTRIDE);
    put_u32(cmd, (cmd_words + 1u) * 4u, FSTRIDE_RGB565_240);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_TEX0_RESXY);
    put_u32(cmd, (cmd_words + 1u) * 4u, RESXY_240x240);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_CLIPMIN);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_CLIPMAX);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x00F000F0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT0_X);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT0_Y);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT1_X);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x00F00000u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT1_Y);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT2_X);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x00F00000u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT2_Y);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x00F00000u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT3_X);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT3_Y);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x00F00000u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_DRAW_COLOR);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x07E0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_DRAW_CMD);
    put_u32(cmd, (cmd_words + 1u) * 4u, NEMA_DRAW_QUAD);
    cmd_words += 2u;

    bus1 = make_bus(&err);
    bus2 = make_bus(&err);
    b1 = semu_nema_backend_create(&err);
    b2 = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, b1 != NULL && b2 != NULL);

    load_cmd(bus1, cmd, cmd_words * 4u, &err);
    load_cmd(bus2, cmd, cmd_words * 4u, &err);
    semu_nema_backend_submit(b1, bus1, CMD_BASE, cmd_words, 0u, NULL, NULL,
                               &err);
    semu_nema_backend_submit(b2, bus2, CMD_BASE, cmd_words, 0u, NULL, NULL,
                               &err);
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

static int cb_invoked;
static uint64_t cb_generation;

static void test_frame_cb(void *context, const semu_frame *frame)
{
    (void)context;
    cb_invoked = 1;
    cb_generation = frame->generation;
}

static void test_frame_callback_invoked(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    semu_nema_backend *backend;
    const semu_frame *frame;
    uint8_t cmd[128u];
    uint32_t cmd_words = 0u;
    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    backend = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, backend != NULL);

    memset(cmd, 0, sizeof(cmd));
    put_u32(cmd, cmd_words * 4u, NEMA_REG_TEX0_BASE);
    put_u32(cmd, (cmd_words + 1u) * 4u, TEX_BASE);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_TEX0_FSTRIDE);
    put_u32(cmd, (cmd_words + 1u) * 4u, FSTRIDE_RGB565_240);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_TEX0_RESXY);
    put_u32(cmd, (cmd_words + 1u) * 4u, RESXY_240x240);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_CLIPMIN);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_CLIPMAX);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x00F000F0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT0_X);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT0_Y);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT1_X);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x00F00000u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT1_Y);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT2_X);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x00F00000u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT2_Y);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x00F00000u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT3_X);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_POINT3_Y);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x00F00000u);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_DRAW_COLOR);
    put_u32(cmd, (cmd_words + 1u) * 4u, 0x001Fu);
    cmd_words += 2u;
    put_u32(cmd, cmd_words * 4u, NEMA_REG_DRAW_CMD);
    put_u32(cmd, (cmd_words + 1u) * 4u, NEMA_DRAW_QUAD);
    cmd_words += 2u;

    load_cmd(bus, cmd, cmd_words * 4u, &err);
    cb_invoked = 0;
    cb_generation = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        semu_nema_backend_submit(backend, bus, CMD_BASE, cmd_words,
                                   0u, test_frame_cb, NULL, &err));
    SEMU_TEST_EQ_U64(context, 1, cb_invoked);
    SEMU_TEST_EQ_U64(context, 1u, cb_generation);
    frame = semu_nema_backend_frame(backend);
    SEMU_TEST_ASSERT(context, frame != NULL);
    SEMU_TEST_EQ_U64(context, 1u, frame->generation);
    semu_nema_backend_destroy(backend);
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_null_path),
        SEMU_TEST_CASE(test_clear_draw),
        SEMU_TEST_CASE(test_refusal_no_partial),
        SEMU_TEST_CASE(test_reset),
        SEMU_TEST_CASE(test_repeat_hash),
        SEMU_TEST_CASE(test_frame_callback_invoked)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
