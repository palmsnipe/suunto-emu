#include "../../src/devices/sapporo_nema_gpu.h"
#include "../../src/display/nema_backend.h"
#include "../../src/display/nema_completion.h"
#include "test.h"

#include "semu/bus.h"
#include "semu/scheduler.h"
#include "semu/types.h"

#include <stdio.h>
#include <string.h>

#define SRAM_BASE 0x10000000u
#define SRAM_SIZE 0x00100000u
#define CMD_BASE  (SRAM_BASE + 0x10000u)
#define TEX_BASE  (SRAM_BASE + 0x20000u)

#define FSTRIDE_RGB565_240  0x040001E0u
#define RESXY_240x240       0x00F000F0u

#define NEMA_REG_STATUS      0x0fcu
#define NEMA_REG_CMDRINGSTOP 0x0ecu
#define NEMA_REG_FRAME_GEN   0x1f4u
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

/* --- ticket 794: GPU-side ring-kick draw refusal --------------------------
 * E-EMU-SAP235-RINGKICK-CPU-INVISIBLE-001: an unsupported draw state met
 * during ring execution must not propagate an error to the CMDRINGSTOP
 * store (lane census c575c2dce1b15664…, Q4 proof: the refused store commits
 * at bus level, no fault, no IRQ).  The refused child performs zero
 * pixel/frame writes and schedules no completion; exactly one named
 * subsystem=gpu event=draw-refused line is logged per refused child.
 * True argument/range errors keep refusing. */

#define REFUSED_CHILD_ADDR (TEX_BASE + 0x1000u)
#define REFUSED_CHILD2_ADDR (TEX_BASE + 0x2000u)
#define REFUSED_TARGET_BASE 0x10040000u
#define REFUSED_SRC_BASE    0x10060000u

/* 22-word-pair child whose DRAW=2 TSC6A resolve passes the presence check
 * (nema_state validate_draw) and then falls outside the resolve law at the
 * IMEM triple — the refused-witness form of the ord-7989 child 0x100d2800
 * (census /tmp/sap235-794census; 176 bytes, refused pair at offset 168). */
static uint32_t build_refused_resolve_child(uint8_t *child)
{
    uint32_t w = 0u;
#define PUT_PAIR(reg, val) do { \
        put_u32(child, w * 4u, (reg)); \
        put_u32(child, (w + 1u) * 4u, (val)); \
        w += 2u; \
    } while (0)
    PUT_PAIR(NEMA_REG_TEX0_BASE, REFUSED_TARGET_BASE);
    PUT_PAIR(NEMA_REG_TEX0_FSTRIDE, FSTRIDE_RGB565_240);
    PUT_PAIR(NEMA_REG_TEX0_RESXY, RESXY_240x240);
    PUT_PAIR(NEMA_REG_TEX1_BASE, REFUSED_SRC_BASE);
    PUT_PAIR(NEMA_REG_TEX1_FSTRIDE, 0x170105a0u);  /* TSC6A, stride 0x5a0 */
    PUT_PAIR(NEMA_REG_TEX1_RESXY, 0x01e001e0u);     /* 480x480 */
    PUT_PAIR(NEMA_REG_TEX_COLOR, 0xff55ff00u);
    PUT_PAIR(NEMA_REG_DRAW_COLOR, 0xff55ff00u);
    PUT_PAIR(NEMA_REG_CLIPMIN, 0u);
    PUT_PAIR(NEMA_REG_CLIPMAX, RESXY_240x240);
    PUT_PAIR(NEMA_REG_MATMULT, 0u);
    PUT_PAIR(NEMA_REG_CODEPTR, 0x941e8000u);        /* accepted code */
    PUT_PAIR(NEMA_REG_IMEM_ADDR, 0x1fu);            /* outside the law */
    PUT_PAIR(NEMA_REG_IMEM_DATAH, 0x08000002u);
    PUT_PAIR(NEMA_REG_IMEM_DATAL, 0x80000009u);
    PUT_PAIR(NEMA_REG_MM00, 0x3f800000u);
    PUT_PAIR(NEMA_REG_MM01, 0u);
    PUT_PAIR(NEMA_REG_MM02, 0u);
    PUT_PAIR(NEMA_REG_MM10, 0u);
    PUT_PAIR(NEMA_REG_MM11, 0x3f800000u);
    PUT_PAIR(NEMA_REG_MM12, 0u);
    PUT_PAIR(NEMA_REG_DRAW_CMD, 0x00000002u);       /* TSC6A resolve */
#undef PUT_PAIR
    return w;
}

static void put_framed_child_at(uint8_t *ring, uint32_t offset,
    uint32_t child_address, uint32_t child_words)
{
    put_u32(ring, offset + 0u, NEMA_REG_CMDADDR);
    put_u32(ring, offset + 4u, child_address);
    put_u32(ring, offset + 8u, NEMA_CL_PUSH | NEMA_REG_CMDSIZE);
    put_u32(ring, offset + 12u, child_words);
}

static long read_log(FILE *log, char *buf, size_t cap)
{
    long size;
    size_t n;
    fflush(log);
    if (fseek(log, 0L, SEEK_END) != 0) return -1;
    size = ftell(log);
    if (size < 0L) return -1;
    if (fseek(log, 0L, SEEK_SET) != 0) return -1;
    n = (size_t)size < cap - 1u ? (size_t)size : cap - 1u;
    if (fread(buf, 1u, n, log) != n) return -1;
    buf[n] = '\0';
    return (long)n;
}

static size_t count_occurrences(const char *hay, const char *needle)
{
    size_t count = 0u;
    const char *p = hay;
    size_t len = strlen(needle);
    while (*p != '\0') {
        if (strncmp(p, needle, len) == 0) { ++count; p += len; }
        else ++p;
    }
    return count;
}

static void test_refused_resolve_kick_is_gpu_side_only(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    semu_nema_backend *backend;
    semu_nema_gpu *gpu;
    semu_logger logger;
    FILE *log;
    uint8_t child[224u];
    uint8_t clear_child[128u];
    uint8_t ring[32u] = {0};
    uint32_t child_words, clear_words;
    uint32_t val = 0u;
    char text[4096];
    const semu_frame *frame;
    size_t i;

    child_words = build_refused_resolve_child(child);
    clear_words = build_clear_cmd(clear_child);

    semu_error_clear(&err);
    bus = make_bus(&err);
    backend = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, bus != NULL && backend != NULL);
    log = tmpfile();
    SEMU_TEST_ASSERT(context, log != NULL);
    semu_log_init(&logger, log, SEMU_LOG_INFO);
    gpu = semu_nema_gpu_create(bus, &semu_nema_backend_ops, backend,
                                test_frame_cb, NULL, test_irq_sink, NULL,
                                NULL, &err);
    SEMU_TEST_ASSERT(context, gpu != NULL);
    semu_nema_gpu_set_logger(gpu, &logger);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_attach(gpu, &err));
    semu_bus_load(bus, REFUSED_CHILD_ADDR, child, child_words * 4u, &err);
    semu_bus_load(bus, REFUSED_CHILD2_ADDR, clear_child,
                  clear_words * 4u, &err);
    configure_ring(bus, &err);

    put_framed_child_at(ring, 0u, REFUSED_CHILD_ADDR, child_words);
    semu_bus_load(bus, CMD_BASE, ring, 16u, &err);
    cb_invoked = 0;
    irq_level = -1;
    irq_number = 0u;

    /* The refused kick must commit at bus level (no error to the store). */
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus,
        NEMA_GPU_BASE + NEMA_REG_CMDRINGSTOP, 4u, CMD_BASE + 16u, &err));

    /* Zero frame writes for the refused child. */
    SEMU_TEST_EQ_U64(context, 0, cb_invoked);
    frame = semu_nema_backend_frame(backend);
    SEMU_TEST_ASSERT(context, frame != NULL);
    SEMU_TEST_EQ_U64(context, 0u, frame->generation);
    for (i = 0u; i < NEMA_BACKEND_PANEL_BYTES; ++i)
        SEMU_TEST_EQ_U64(context, 0u, frame->pixels[i]);

    /* No completion IRQ anywhere (lane irq=0), registers silent. */
    SEMU_TEST_EQ_U64(context, -1, irq_level);
    semu_bus_read(bus, NEMA_GPU_BASE + NEMA_REG_INTERRUPT, 4u, &val, &err);
    SEMU_TEST_EQ_U64(context, 0u, val);
    semu_bus_read(bus, NEMA_GPU_BASE + NEMA_REG_FRAME_GEN, 4u, &val, &err);
    SEMU_TEST_EQ_U64(context, 0u, val);
    semu_bus_read(bus, NEMA_GPU_BASE + NEMA_REG_CMDRINGSTOP, 4u, &val, &err);
    SEMU_TEST_EQ_U64(context, CMD_BASE + 16u, val); /* store committed */

    /* Exactly one named refusal line for the refused child. */
    SEMU_TEST_ASSERT(context, read_log(log, text, sizeof(text)) >= 0);
    SEMU_TEST_EQ_U64(context, 1u, count_occurrences(text, "event=draw-refused"));
    SEMU_TEST_ASSERT(context, strstr(text, "subsystem=gpu") != NULL);
    SEMU_TEST_ASSERT(context, strstr(text, "ord=1 ") != NULL);
    SEMU_TEST_ASSERT(context, strstr(text, "child=0x10021000") != NULL);
    SEMU_TEST_ASSERT(context, strstr(text, "offset=168") != NULL);
    SEMU_TEST_ASSERT(context, strstr(text, "draw=0x00000002") != NULL);
    SEMU_TEST_ASSERT(context, strstr(text,
        "reason=\"nema_tsc6a: unsupported resolve state\"") != NULL);
    SEMU_TEST_ASSERT(context, strstr(text,
        "imem=31,0x08000002,0x80000009") != NULL);
    SEMU_TEST_ASSERT(context, strstr(text, "code=0x941e8000") != NULL);

    /* Normal operation continues: the next accepted child still renders. */
    memset(ring, 0, sizeof(ring));
    put_framed_child_at(ring, 0u, REFUSED_CHILD2_ADDR, clear_words);
    semu_bus_load(bus, CMD_BASE + 16u, ring, 16u, &err);
    cb_invoked = 0;
    irq_level = -1;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus,
        NEMA_GPU_BASE + NEMA_REG_CMDRINGSTOP, 4u, CMD_BASE + 32u, &err));
    SEMU_TEST_EQ_U64(context, 1, cb_invoked);
    SEMU_TEST_ASSERT(context, read_log(log, text, sizeof(text)) >= 0);
    SEMU_TEST_EQ_U64(context, 1u, count_occurrences(text, "event=draw-refused"));

    fclose(log);
    semu_nema_gpu_destroy(gpu);
    semu_nema_backend_destroy(backend);
    semu_bus_destroy(bus);
}

static void test_refused_resolve_suppresses_completion_marker(
    semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_nema_backend *backend;
    semu_nema_gpu *gpu;
    semu_logger logger;
    FILE *log;
    uint8_t child[224u];
    uint8_t ring[32u] = {0};
    uint32_t child_words;
    uint32_t val = 0u;
    char text[4096];

    child_words = build_refused_resolve_child(child);

    semu_error_clear(&err);
    bus = make_bus(&err);
    scheduler = semu_scheduler_create(&err);
    backend = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, bus != NULL && scheduler != NULL &&
                     backend != NULL);
    log = tmpfile();
    SEMU_TEST_ASSERT(context, log != NULL);
    semu_log_init(&logger, log, SEMU_LOG_INFO);
    gpu = semu_nema_gpu_create(bus, &semu_nema_backend_ops, backend,
                                test_frame_cb, NULL, test_irq_sink, NULL,
                                scheduler, &err);
    SEMU_TEST_ASSERT(context, gpu != NULL);
    semu_nema_gpu_set_logger(gpu, &logger);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_attach(gpu, &err));
    semu_bus_load(bus, REFUSED_CHILD_ADDR, child, child_words * 4u, &err);
    configure_ring(bus, &err);

    put_framed_child_at(ring, 0u, REFUSED_CHILD_ADDR, child_words);
    put_completion_marker(ring, 16u, 9u);
    semu_bus_load(bus, CMD_BASE, ring, 32u, &err);
    cb_invoked = 0;
    irq_level = -1;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus,
        NEMA_GPU_BASE + NEMA_REG_CMDRINGSTOP, 4u, CMD_BASE + 32u, &err));
    SEMU_TEST_EQ_U64(context, 0, cb_invoked);
    semu_scheduler_advance(scheduler, NEMA_COMPLETION_DELAY_NS, &err);
    SEMU_TEST_EQ_U64(context, -1, irq_level); /* lane: completion refused */
    semu_bus_read(bus, NEMA_GPU_BASE + NEMA_REG_CLID, 4u, &val, &err);
    SEMU_TEST_EQ_U64(context, 0u, val);
    semu_bus_read(bus, NEMA_GPU_BASE + NEMA_REG_INTERRUPT, 4u, &val, &err);
    SEMU_TEST_EQ_U64(context, 0u, val);
    SEMU_TEST_ASSERT(context, read_log(log, text, sizeof(text)) >= 0);
    SEMU_TEST_EQ_U64(context, 1u, count_occurrences(text, "event=draw-refused"));

    fclose(log);
    semu_nema_gpu_destroy(gpu);
    semu_nema_backend_destroy(backend);
    semu_scheduler_destroy(scheduler);
    semu_bus_destroy(bus);
}

static void test_invalid_ring_range_still_refuses(semu_test_context *context)
{
    semu_error err;
    semu_bus *bus;
    semu_nema_backend *backend;
    semu_nema_gpu *gpu;
    semu_logger logger;
    FILE *log;
    uint8_t ring[16u] = {0};
    uint32_t val = 0u;
    char text[512];

    semu_error_clear(&err);
    bus = make_bus(&err);
    backend = semu_nema_backend_create(&err);
    SEMU_TEST_ASSERT(context, bus != NULL && backend != NULL);
    log = tmpfile();
    SEMU_TEST_ASSERT(context, log != NULL);
    semu_log_init(&logger, log, SEMU_LOG_INFO);
    gpu = semu_nema_gpu_create(bus, &semu_nema_backend_ops, backend,
                                test_frame_cb, NULL, test_irq_sink, NULL,
                                NULL, &err);
    SEMU_TEST_ASSERT(context, gpu != NULL);
    semu_nema_gpu_set_logger(gpu, &logger);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_attach(gpu, &err));
    configure_ring(bus, &err);
    put_framed_child_at(ring, 0u, REFUSED_CHILD_ADDR, 36u);
    semu_bus_load(bus, CMD_BASE, ring, 16u, &err);
    cb_invoked = 0;
    irq_level = -1;

    /* stop outside the active ring range: a true range error still refuses
     * the store (programming error, not a draw-state refusal). */
    SEMU_TEST_ASSERT(context, semu_bus_write(bus,
        NEMA_GPU_BASE + NEMA_REG_CMDRINGSTOP, 4u,
        CMD_BASE + 64u * 4u + 4u, &err) != SEMU_OK);
    SEMU_TEST_EQ_U64(context, 0, cb_invoked);
    SEMU_TEST_EQ_U64(context, -1, irq_level);
    semu_bus_read(bus, NEMA_GPU_BASE + NEMA_REG_CMDRINGSTOP, 4u, &val, &err);
    SEMU_TEST_EQ_U64(context, CMD_BASE | 6u, val); /* unchanged bootstrap */
    SEMU_TEST_ASSERT(context, read_log(log, text, sizeof(text)) >= 0);
    SEMU_TEST_EQ_U64(context, 0u, count_occurrences(text, "event=draw-refused"));

    fclose(log);
    semu_nema_gpu_destroy(gpu);
    semu_nema_backend_destroy(backend);
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
        SEMU_TEST_CASE(test_ring_completion_refuses_bad_size),
        SEMU_TEST_CASE(test_refused_resolve_kick_is_gpu_side_only),
        SEMU_TEST_CASE(test_refused_resolve_suppresses_completion_marker),
        SEMU_TEST_CASE(test_invalid_ring_range_still_refuses)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
