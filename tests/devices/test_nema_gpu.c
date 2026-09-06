#include "../../src/devices/sapporo_nema_gpu.h"
#include "../../src/display/nema_backend.h"
#include "../../src/display/nema_completion.h"
#include "test.h"

#include "semu/bus.h"
#include "semu/scheduler.h"
#include "semu/types.h"

#include <string.h>

#define SRAM_BASE 0x10000000u
#define SRAM_SIZE 0x00100000u
#define CMD_BASE  (SRAM_BASE + 0x10000u)
#define TEX_BASE  (SRAM_BASE + 0x20000u)

#define FSTRIDE_RGB565_240  0x040001E0u
#define RESXY_240x240       0x00F000F0u

#define NEMA_REG_STATUS      0x0fcu
#define NEMA_REG_CMDRINGSTOP 0x0ecu
#define NEMA_REG_MODULE_ID   0x1ecu

static int cb_invoked;
static uint64_t cb_generation;

static void test_frame_cb(void *context, const semu_frame *frame)
{
    (void)context;
    cb_invoked = 1;
    cb_generation = frame->generation;
}

static int irq_level;
static unsigned irq_number;

static void test_irq_sink(void *context, unsigned irq, int level)
{
    (void)context;
    irq_number = irq;
    irq_level = level;
}

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
    buf[off]      = (uint8_t)val;
    buf[off + 1u] = (uint8_t)(val >> 8u);
    buf[off + 2u] = (uint8_t)(val >> 16u);
    buf[off + 3u] = (uint8_t)(val >> 24u);
}

static uint32_t build_clear_cmd(uint8_t *cmd)
{
    uint32_t w = 0u;
    put_u32(cmd, w * 4u, NEMA_REG_TEX0_BASE);      w += 2u;
    put_u32(cmd, (w - 1u) * 4u, TEX_BASE);
    put_u32(cmd, w * 4u, NEMA_REG_TEX0_FSTRIDE);   w += 2u;
    put_u32(cmd, (w - 1u) * 4u, FSTRIDE_RGB565_240);
    put_u32(cmd, w * 4u, NEMA_REG_TEX0_RESXY);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, RESXY_240x240);
    put_u32(cmd, w * 4u, NEMA_REG_CLIPMIN);        w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0u);
    put_u32(cmd, w * 4u, NEMA_REG_CLIPMAX);        w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x00F000F0u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT0_X);       w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT0_Y);       w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT1_X);       w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x00F00000u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT1_Y);       w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT2_X);       w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x00F00000u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT2_Y);       w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x00F00000u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT3_X);       w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0u);
    put_u32(cmd, w * 4u, NEMA_REG_POINT3_Y);       w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x00F00000u);
    put_u32(cmd, w * 4u, NEMA_REG_DRAW_COLOR);     w += 2u;
    put_u32(cmd, (w - 1u) * 4u, 0x001Fu);
    put_u32(cmd, w * 4u, NEMA_REG_DRAW_CMD);       w += 2u;
    put_u32(cmd, (w - 1u) * 4u, NEMA_DRAW_QUAD);
    return w;
}

static void load_small_child(semu_bus *bus, semu_error *error)
{
    uint8_t child[16u];
    put_u32(child, 0u, 0x00000110u);
    put_u32(child, 4u, 0u);
    put_u32(child, 8u, 0x00000114u);
    put_u32(child, 12u, 0u);
    semu_bus_load(bus, TEX_BASE, child, sizeof(child), error);
}

static void configure_ring(semu_bus *bus, semu_error *error)
{
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDADDR,
                   4u, CMD_BASE, error);
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDSIZE,
                   4u, 64u * 4u, error);
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDRINGSTOP,
                   4u, CMD_BASE | 0x6u, error);
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_STATUS,
                   4u, 0u, error);
}

static void put_framed_child(uint8_t *ring, uint32_t offset,
                             uint32_t child_size)
{
    put_u32(ring, offset + 0u, NEMA_REG_CMDADDR);
    put_u32(ring, offset + 4u, TEX_BASE);
    put_u32(ring, offset + 8u, NEMA_CL_PUSH | NEMA_REG_CMDSIZE);
    put_u32(ring, offset + 12u, child_size);
}

static void put_completion_marker(uint8_t *ring, uint32_t offset,
                                  uint32_t list_id)
{
    put_u32(ring, offset + 0u, NEMA_REG_CLID);
    put_u32(ring, offset + 4u, list_id);
    put_u32(ring, offset + 8u, NEMA_REG_INTERRUPT);
    put_u32(ring, offset + 12u, 1u);
}

static void test_module_id(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    semu_nema_gpu *gpu;
    uint32_t val = 0u;
    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    gpu = semu_nema_gpu_create(bus, NULL, NULL, NULL, NULL,
                                NULL, NULL, NULL, &err);
    SEMU_TEST_ASSERT(context, gpu != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_nema_gpu_attach(gpu, &err));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_read(bus, NEMA_GPU_BASE + NEMA_REG_MODULE_ID,
                       4u, &val, &err));
    SEMU_TEST_EQ_U64(context, 0x86362000u, val);
    semu_nema_gpu_destroy(gpu);
    semu_bus_destroy(bus);
}

static void test_render_trigger(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    semu_nema_backend *backend;
    semu_nema_gpu *gpu;
    uint8_t cmd[128u];
    uint32_t cmd_words;
    uint32_t ring_bytes;
    uint32_t ring_stop;
    uint32_t val = 0u;
    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    backend = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, backend != NULL);
    gpu = semu_nema_gpu_create(bus, &semu_nema_backend_ops, backend,
                                test_frame_cb, NULL,
                                test_irq_sink, NULL, NULL, &err);
    SEMU_TEST_ASSERT(context, gpu != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_attach(gpu, &err));

    cmd_words = build_clear_cmd(cmd);
    ring_bytes = cmd_words * 4u;
    semu_bus_load(bus, CMD_BASE, cmd, ring_bytes, &err);

    /* Configure ring: CMDADDR and CMDSIZE (ring larger than data) */
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDADDR,
                    4u, CMD_BASE, &err);
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDSIZE,
                    4u, ring_bytes * 2u, &err);
    /* Bootstrap ring stop */
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDRINGSTOP,
                    4u, CMD_BASE | 0x6u, &err);
    /* Initialize: write 0 to STATUS */
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_STATUS,
                    4u, 0u, &err);

    /* Trigger: write new ring stop (no bootstrap flag) */
    ring_stop = CMD_BASE + ring_bytes;
    cb_invoked = 0;
    cb_generation = 0u;
    irq_level = -1;
    irq_number = 0u;
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDRINGSTOP,
                    4u, ring_stop, &err);

    SEMU_TEST_EQ_U64(context, 1, cb_invoked);
    SEMU_TEST_EQ_U64(context, 1u, cb_generation);
    SEMU_TEST_EQ_U64(context, 28u, irq_number);
    SEMU_TEST_EQ_U64(context, 1, irq_level);

    /* Interrupt register should be set */
    semu_bus_read(bus, NEMA_GPU_BASE + NEMA_REG_INTERRUPT,
                   4u, &val, &err);
    SEMU_TEST_EQ_U64(context, 1u, val);

    /* Clear interrupt lowers IRQ */
    irq_level = -1;
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_INTERRUPT,
                    4u, 0u, &err);
    SEMU_TEST_EQ_U64(context, 0, irq_level);

    semu_nema_gpu_destroy(gpu);
    semu_nema_backend_destroy(backend);
    semu_bus_destroy(bus);
}

static void test_bootstrap_skipped(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    semu_nema_backend *backend;
    semu_nema_gpu *gpu;
    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    backend = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, backend != NULL);
    gpu = semu_nema_gpu_create(bus, &semu_nema_backend_ops, backend,
                                test_frame_cb, NULL,
                                test_irq_sink, NULL, NULL, &err);
    SEMU_TEST_ASSERT(context, gpu != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_attach(gpu, &err));

    /* Configure and initialize */
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDADDR,
                    4u, CMD_BASE, &err);
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDSIZE,
                    4u, 0x100u, &err);
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDRINGSTOP,
                    4u, CMD_BASE | 0x6u, &err);
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_STATUS,
                    4u, 0u, &err);

    /* Another bootstrap write should not trigger rendering */
    cb_invoked = 0;
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDRINGSTOP,
                    4u, CMD_BASE | 0x6u, &err);
    SEMU_TEST_EQ_U64(context, 0, cb_invoked);

    semu_nema_gpu_destroy(gpu);
    semu_nema_backend_destroy(backend);
    semu_bus_destroy(bus);
}

static void test_reset(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    semu_nema_gpu *gpu;
    uint32_t val = 0u;
    semu_error_clear(&err);
    bus = make_bus(&err);
    SEMU_TEST_ASSERT(context, bus != NULL);
    gpu = semu_nema_gpu_create(bus, NULL, NULL, NULL, NULL,
                                NULL, NULL, NULL, &err);
    SEMU_TEST_ASSERT(context, gpu != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_attach(gpu, &err));

    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDADDR,
                    4u, 0x10000000u, &err);
    semu_nema_gpu_reset(gpu);
    semu_bus_read(bus, NEMA_GPU_BASE + NEMA_REG_CMDADDR,
                   4u, &val, &err);
    SEMU_TEST_EQ_U64(context, 0u, val);

    semu_nema_gpu_destroy(gpu);
    semu_bus_destroy(bus);
}

static void test_ring_completion_marker(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_nema_backend *backend;
    semu_nema_gpu *gpu;
    uint8_t ring[64u * 4u] = {0};
    uint32_t val = 0u;

    semu_error_clear(&err);
    bus = make_bus(&err);
    scheduler = semu_scheduler_create(&err);
    backend = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, bus != NULL && scheduler != NULL &&
                     backend != NULL);
    gpu = semu_nema_gpu_create(bus, &semu_nema_backend_ops, backend,
                                test_frame_cb, NULL, test_irq_sink, NULL,
                                scheduler, &err);
    SEMU_TEST_ASSERT(context, gpu != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_attach(gpu, &err));
    load_small_child(bus, &err);
    configure_ring(bus, &err);

    put_framed_child(ring, 0u, 4u);
    semu_bus_load(bus, CMD_BASE, ring, 16u, &err);
    cb_invoked = 0;
    irq_level = -1;
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDRINGSTOP,
                   4u, CMD_BASE + 16u, &err);
    SEMU_TEST_EQ_U64(context, 1, cb_invoked);
    SEMU_TEST_EQ_U64(context, -1, irq_level);

    put_completion_marker(ring, 16u, 1u);
    semu_bus_load(bus, CMD_BASE + 16u, ring + 16u, 16u, &err);
    cb_invoked = 0;
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDRINGSTOP,
                   4u, CMD_BASE + 32u, &err);
    SEMU_TEST_EQ_U64(context, 0, cb_invoked);
    SEMU_TEST_EQ_U64(context, -1, irq_level);
    semu_scheduler_advance(scheduler, NEMA_COMPLETION_DELAY_NS, &err);
    SEMU_TEST_EQ_U64(context, 28u, irq_number);
    SEMU_TEST_EQ_U64(context, 1, irq_level);
    semu_bus_read(bus, NEMA_GPU_BASE + NEMA_REG_CLID,
                  4u, &val, &err);
    SEMU_TEST_EQ_U64(context, 1u, val);
    semu_bus_read(bus, NEMA_GPU_BASE + NEMA_REG_INTERRUPT,
                  4u, &val, &err);
    SEMU_TEST_EQ_U64(context, 1u, val);

    semu_nema_gpu_destroy(gpu);
    semu_nema_backend_destroy(backend);
    semu_scheduler_destroy(scheduler);
    semu_bus_destroy(bus);
}

static void test_ring_completion_refuses_marker_mismatch(
    semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_nema_backend *backend;
    semu_nema_gpu *gpu;
    uint8_t marker[16u] = {0};
    uint32_t val = 0u;

    semu_error_clear(&err);
    bus = make_bus(&err);
    scheduler = semu_scheduler_create(&err);
    backend = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, bus != NULL && scheduler != NULL &&
                     backend != NULL);
    gpu = semu_nema_gpu_create(bus, &semu_nema_backend_ops, backend,
                                test_frame_cb, NULL, test_irq_sink, NULL,
                                scheduler, &err);
    SEMU_TEST_ASSERT(context, gpu != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_attach(gpu, &err));
    configure_ring(bus, &err);
    put_completion_marker(marker, 0u, 7u);
    put_u32(marker, 12u, 2u);
    semu_bus_load(bus, CMD_BASE, marker, sizeof(marker), &err);
    cb_invoked = 0;
    irq_level = -1;
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDRINGSTOP,
                   4u, CMD_BASE + sizeof(marker), &err);
    SEMU_TEST_EQ_U64(context, 0, cb_invoked);
    SEMU_TEST_EQ_U64(context, -1, irq_level);
    semu_scheduler_advance(scheduler, NEMA_COMPLETION_DELAY_NS, &err);
    semu_bus_read(bus, NEMA_GPU_BASE + NEMA_REG_CLID,
                  4u, &val, &err);
    SEMU_TEST_EQ_U64(context, 0u, val);
    semu_bus_read(bus, NEMA_GPU_BASE + NEMA_REG_INTERRUPT,
                  4u, &val, &err);
    SEMU_TEST_EQ_U64(context, 0u, val);

    semu_nema_gpu_destroy(gpu);
    semu_nema_backend_destroy(backend);
    semu_scheduler_destroy(scheduler);
    semu_bus_destroy(bus);
}

static void test_ring_completion_refuses_bad_size(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_nema_backend *backend;
    semu_nema_gpu *gpu;
    uint8_t ring[32u] = {0};
    uint32_t val = 0u;

    semu_error_clear(&err);
    bus = make_bus(&err);
    scheduler = semu_scheduler_create(&err);
    backend = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, bus != NULL && scheduler != NULL &&
                     backend != NULL);
    gpu = semu_nema_gpu_create(bus, &semu_nema_backend_ops, backend,
                                test_frame_cb, NULL, test_irq_sink, NULL,
                                scheduler, &err);
    SEMU_TEST_ASSERT(context, gpu != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_attach(gpu, &err));
    load_small_child(bus, &err);
    configure_ring(bus, &err);
    put_framed_child(ring, 0u, NEMA_MAX_LIST_WORDS + 1u);
    put_completion_marker(ring, 16u, 7u);
    semu_bus_load(bus, CMD_BASE, ring, sizeof(ring), &err);
    cb_invoked = 0;
    irq_level = -1;
    semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDRINGSTOP,
                   4u, CMD_BASE + 32u, &err);
    SEMU_TEST_EQ_U64(context, 0, cb_invoked);
    SEMU_TEST_EQ_U64(context, -1, irq_level);
    semu_scheduler_advance(scheduler, NEMA_COMPLETION_DELAY_NS, &err);
    semu_bus_read(bus, NEMA_GPU_BASE + NEMA_REG_CLID,
                  4u, &val, &err);
    SEMU_TEST_EQ_U64(context, 0u, val);

    semu_nema_gpu_destroy(gpu);
    semu_nema_backend_destroy(backend);
    semu_scheduler_destroy(scheduler);
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_module_id),
        SEMU_TEST_CASE(test_render_trigger),
        SEMU_TEST_CASE(test_bootstrap_skipped),
        SEMU_TEST_CASE(test_reset),
        SEMU_TEST_CASE(test_ring_completion_marker),
        SEMU_TEST_CASE(test_ring_completion_refuses_marker_mismatch),
        SEMU_TEST_CASE(test_ring_completion_refuses_bad_size)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
