#include "test.h"
#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "../../src/devices/sapporo_nema_gpu.h"
#include "../../src/display/nema_backend.h"
#include "../../src/display/nema_framing.h"
#include <stdlib.h>

/* Link these production translation units into this test with only allocation
 * calls replaced. The archive therefore supplies no duplicate definitions.
 * Exercise the real MMIO -> parser -> renderer -> completion -> scheduler path. */
static unsigned fail_allocation, allocation_count;
static int fail_now(void)
{ return fail_allocation != 0u && ++allocation_count == fail_allocation; }
static void *checked_malloc(size_t size)
{ return fail_now() ? NULL : malloc(size); }
static void *checked_calloc(size_t count, size_t size)
{ return fail_now() ? NULL : calloc(count, size); }
static void *checked_realloc(void *p, size_t size)
{ return fail_now() ? NULL : realloc(p, size); }
#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#include "../../src/display/nema_framing.c"
#include "../../src/display/nema_backend_transaction.c"
#include "../../src/core/scheduler_batch.c"
#undef malloc
#undef calloc
#undef realloc

#define RING 0x10000000u
#define STOP (NEMA_GPU_BASE + 0xecu)
static int write_word(semu_cpu_fixture *f, uint32_t address, uint32_t value)
{ return semu_bus_write(f->bus, address, 4u, value, &f->error) == SEMU_OK; }

static void test_guest_gpu_store_precise_fault(semu_test_context *context)
{
    static const uint8_t program[] = {0x08u, 0x60u, 0x00u, 0xbeu}; /* STR r0,[r1]; BKPT */
    unsigned refuse;
    for (refuse = 0u; refuse < 2u; ++refuse) {
        semu_cpu_fixture f; semu_nema_gpu *gpu; semu_nema_backend *backend;
        semu_snapshot_writer before, after; uint32_t value;
        SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&f, program, sizeof(program)));
        SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&f, 12u, 0x181u));
        SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&f, 0x180u, 0xbe00u));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_ram(f.bus, "ring", RING, 4096u, &f.error));
        backend = semu_nema_backend_create(&f.error);
        SEMU_TEST_ASSERT(context, backend != NULL);
        gpu = semu_nema_gpu_create(f.bus, &semu_nema_backend_ops, backend,
            NULL, NULL, NULL, NULL, f.scheduler, &f.error);
        SEMU_TEST_ASSERT(context, gpu != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_attach(gpu, &f.error));
        SEMU_TEST_ASSERT(context, write_word(&f, RING, NEMA_REG_CMDADDR));
        SEMU_TEST_ASSERT(context, write_word(&f, RING + 4u, RING + 256u));
        SEMU_TEST_ASSERT(context, write_word(&f, RING + 8u, NEMA_CL_PUSH | NEMA_REG_CMDSIZE));
        SEMU_TEST_ASSERT(context, write_word(&f, RING + 12u, 2u));
        SEMU_TEST_ASSERT(context, write_word(&f, RING + 256u,
            refuse ? 0x7770u : NEMA_REG_CLIPMIN));
        SEMU_TEST_ASSERT(context, write_word(&f, NEMA_GPU_BASE + NEMA_REG_CMDADDR, RING));
        SEMU_TEST_ASSERT(context, write_word(&f, NEMA_GPU_BASE + NEMA_REG_CMDSIZE, 256u));
        SEMU_TEST_ASSERT(context, write_word(&f, STOP, RING | 6u));
        SEMU_TEST_ASSERT(context, write_word(&f, NEMA_GPU_BASE + 0xfcu, 0u));
        semu_cpu_get_state_mutable(f.cpu)->r[0] = RING + 16u;
        semu_cpu_get_state_mutable(f.cpu)->r[1] = STOP;
        semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_snapshot_write(gpu, &before, &f.error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&f));
        SEMU_TEST_EQ_U64(context, refuse ? 0x180u : 0x102u, semu_cpu_get_state(f.cpu)->r[15]);
        SEMU_TEST_EQ_U64(context, refuse ? 3u : 0u, semu_cpu_get_state(f.cpu)->xpsr & 0x1ffu);
        if (refuse) {
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(f.bus, 0xe000ed28u, 4u, &value, &f.error));
            SEMU_TEST_EQ_U64(context, 0x8200u, value);
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(f.bus, 0xe000ed38u, 4u, &value, &f.error));
            SEMU_TEST_EQ_U64(context, STOP, value);
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_snapshot_write(gpu, &after, &f.error));
            SEMU_TEST_EQ_U64(context, before.size, after.size);
            SEMU_TEST_ASSERT(context, memcmp(before.data, after.data, before.size) == 0);
        }
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&f));
        SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT, semu_cpu_stop_reason(f.cpu));
        SEMU_TEST_EQ_U64(context, 2u, semu_cpu_get_state(f.cpu)->instructions);
        semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
        semu_nema_gpu_destroy(gpu); semu_nema_backend_destroy(backend);
        semu_cpu_fixture_destroy(&f);
    }
}
typedef struct { unsigned frames, irqs; uint64_t generations[8]; } output;
static void publish(void *context, const semu_frame *frame)
{
    output *out = context;
    if (out->frames < SEMU_ARRAY_LEN(out->generations))
        out->generations[out->frames] = frame->generation;
    ++out->frames;
}
static void interrupt(void *context, unsigned line, int asserted)
{ output *out = context; if (line == NEMA_GPU_IRQ && asserted) ++out->irqs; }
static void unrelated(void *context, uint64_t now) { (void)context; (void)now; }
static void test_whole_submission_allocation_failures(semu_test_context *context)
{
    const uint32_t clear[] = {
        NEMA_REG_TEX0_BASE, RING + 2048u, NEMA_REG_TEX0_FSTRIDE, 0x040001e0u,
        NEMA_REG_TEX0_RESXY, 0x00f000f0u, NEMA_REG_CLIPMIN, 0u, NEMA_REG_CLIPMAX, 0x00010001u,
        NEMA_REG_POINT0_X, 0u, NEMA_REG_POINT0_Y, 0u, NEMA_REG_POINT1_X, 0x10000u,
        NEMA_REG_POINT1_Y, 0u, NEMA_REG_POINT2_X, 0x10000u, NEMA_REG_POINT2_Y, 0x10000u,
        NEMA_REG_POINT3_X, 0u, NEMA_REG_POINT3_Y, 0x10000u,
        NEMA_REG_DRAW_COLOR, 0x001fu, NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD};
    const uint32_t ring[] = {
        NEMA_REG_CMDADDR, RING + 256u, NEMA_CL_PUSH | NEMA_REG_CMDSIZE, 4u,
        NEMA_REG_CMDADDR, RING + 288u, NEMA_CL_PUSH | NEMA_REG_CMDSIZE, 2u,
        NEMA_REG_CLID, 7u, NEMA_REG_INTERRUPT, 1u,
        NEMA_REG_CLID, 8u, NEMA_REG_INTERRUPT, 1u};
    unsigned fail;
    for (fail = 1u; fail <= 3u; ++fail) {
        semu_cpu_fixture f; semu_nema_backend *backend; semu_nema_gpu *gpu;
        static const uint8_t program[] = {0u, 0xbeu};
        output out = {0}; semu_snapshot_writer before, after;
        semu_scheduler queue; semu_scheduled_event events[15];
        uint8_t pixels[NEMA_BACKEND_PANEL_BYTES]; uint32_t value; unsigned i;
        SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&f, program, sizeof(program)));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_ram(f.bus, "ring", RING, 4096u, &f.error));
        backend = semu_nema_backend_create(&f.error); SEMU_TEST_ASSERT(context, backend != NULL);
        gpu = semu_nema_gpu_create(f.bus, &semu_nema_backend_ops, backend,
            publish, &out, interrupt, &out, f.scheduler, &f.error);
        SEMU_TEST_ASSERT(context, gpu != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_attach(gpu, &f.error));
        for (i = 0u; i < SEMU_ARRAY_LEN(clear); ++i)
            SEMU_TEST_ASSERT(context, write_word(&f, RING + 512u + i * 4u, clear[i]));
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, semu_nema_backend_submit(backend, f.bus,
            RING + 512u, SEMU_ARRAY_LEN(clear), 0u, publish, &out, &f.error));
        memcpy(pixels, semu_nema_backend_frame(backend)->pixels, sizeof(pixels));
        for (i = 0u; i < SEMU_ARRAY_LEN(ring); ++i)
            SEMU_TEST_ASSERT(context, write_word(&f, RING + i * 4u, ring[i]));
        SEMU_TEST_ASSERT(context, write_word(&f, RING + 256u, NEMA_REG_DRAW_COLOR));
        SEMU_TEST_ASSERT(context, write_word(&f, RING + 260u, 0xf800u));
        SEMU_TEST_ASSERT(context, write_word(&f, RING + 264u, NEMA_REG_DRAW_CMD));
        SEMU_TEST_ASSERT(context, write_word(&f, RING + 268u, NEMA_DRAW_QUAD));
        SEMU_TEST_ASSERT(context, write_word(&f, RING + 288u, NEMA_REG_DRAW_CMD));
        SEMU_TEST_ASSERT(context, write_word(&f, RING + 292u, NEMA_DRAW_QUAD));
        SEMU_TEST_ASSERT(context, write_word(&f, NEMA_GPU_BASE + NEMA_REG_CMDADDR, RING));
        SEMU_TEST_ASSERT(context, write_word(&f, NEMA_GPU_BASE + NEMA_REG_CMDSIZE, 256u));
        SEMU_TEST_ASSERT(context, write_word(&f, STOP, RING | 6u));
        SEMU_TEST_ASSERT(context, write_word(&f, NEMA_GPU_BASE + 0xfcu, 0u));
        for (i = 0u; i < 15u; ++i)
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_schedule(f.scheduler,
                200000u, unrelated, NULL, NULL, &f.error));
        queue = *f.scheduler; memcpy(events, f.scheduler->events, sizeof(events));
        semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_snapshot_write(gpu, &before, &f.error));
        allocation_count = 0u; fail_allocation = fail;
        SEMU_TEST_EQ_U64(context, SEMU_ERR_NOMEM, semu_bus_write(f.bus, STOP, 4u, RING + 64u, &f.error));
        fail_allocation = 0u;
        SEMU_TEST_EQ_U64(context, fail, allocation_count);
        SEMU_TEST_EQ_U64(context, 1u, out.frames); SEMU_TEST_EQ_U64(context, 0u, out.irqs);
        SEMU_TEST_EQ_U64(context, 1u, semu_nema_backend_frame(backend)->generation);
        SEMU_TEST_ASSERT(context, memcmp(pixels, semu_nema_backend_frame(backend)->pixels, sizeof(pixels)) == 0);
        SEMU_TEST_ASSERT(context, memcmp(&queue, f.scheduler, sizeof(queue)) == 0);
        SEMU_TEST_ASSERT(context, memcmp(events, f.scheduler->events, sizeof(events)) == 0);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_snapshot_write(gpu, &after, &f.error));
        SEMU_TEST_EQ_U64(context, before.size, after.size);
        SEMU_TEST_ASSERT(context, memcmp(before.data, after.data, before.size) == 0);
        /* A subsequent inherited draw must still be blue, not the staged red. */
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, semu_nema_backend_submit(backend, f.bus,
            RING + 288u, 2u, 0u, publish, &out, &f.error));
        SEMU_TEST_ASSERT(context, memcmp(pixels, semu_nema_backend_frame(backend)->pixels, sizeof(pixels)) == 0);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(f.bus, STOP, 4u, RING + 64u, &f.error));
        SEMU_TEST_EQ_U64(context, 4u, out.frames); SEMU_TEST_EQ_U64(context, 17u, f.scheduler->count);
        for (i = 0u; i < 4u; ++i) SEMU_TEST_EQ_U64(context, i + 1u, out.generations[i]);
        for (i = 0u; i < 2u; ++i) {
            SEMU_TEST_EQ_U64(context, 16u + i, semu_scheduler_event_get(f.scheduler, 0u)->id);
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_run_next(f.scheduler, &f.error));
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(f.bus,
                NEMA_GPU_BASE + NEMA_REG_CLID, 4u, &value, &f.error));
            SEMU_TEST_EQ_U64(context, 7u + i, value);
        }
        SEMU_TEST_EQ_U64(context, 2u, out.irqs);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(f.bus, STOP, 4u, RING + 64u, &f.error));
        SEMU_TEST_EQ_U64(context, 4u, out.frames); SEMU_TEST_EQ_U64(context, 15u, f.scheduler->count);
        semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
        semu_nema_gpu_destroy(gpu); semu_nema_backend_destroy(backend); semu_cpu_fixture_destroy(&f);
    }
}
#define SRAM_BASE RING
#define SRAM_SIZE 4096u
#define RING_BASE RING
#define LIST_BASE (RING + 512u)
typedef struct { size_t count, children; } capture_ctx;
static void on_record(void *context, const nema_record *record)
{ capture_ctx *cap = context; (void)record; ++cap->count; }
static void on_child(void *context, uint32_t address, uint32_t count)
{ capture_ctx *cap = context; (void)address; (void)count; ++cap->children; }
static void load_words(semu_bus *bus, uint32_t address, const uint32_t *words,
    size_t count, semu_error *error)
{
    size_t i;
    for (i = 0u; i < count; ++i)
        (void)semu_bus_write(bus, address + (uint32_t)i * 4u, 4u, words[i], error);
}
static void test_nema_framing_wrapped_non_power_of_two(semu_test_context *context)
{
    const uint32_t capacities[] = {20u, 24u, 60u, 64u, 96u};
    semu_error e; semu_bus *bus = semu_bus_create(&e); unsigned k;
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_ram(bus, "sram", SRAM_BASE, SRAM_SIZE, &e));
    for (k = 0u; k < SEMU_ARRAY_LEN(capacities); ++k) {
        uint32_t n = capacities[k], ring[96] = {0}, list[] = {0x110u, 7u};
        capture_ctx cap = {0};
        ring[0] = ring[n - 8u] = NEMA_REG_CMDADDR;
        ring[1] = ring[n - 7u] = LIST_BASE;
        ring[2] = ring[n - 6u] = NEMA_CL_PUSH | NEMA_REG_CMDSIZE;
        ring[3] = ring[n - 5u] = 2u;
        ring[n - 4u] = NEMA_HOLDCMD | NEMA_REG_CMDADDR;
        ring[n - 3u] = RING_BASE;
        ring[n - 2u] = NEMA_HOLDCMD | NEMA_REG_CMDSIZE;
        ring[n - 1u] = n * 4u;
        ring[4] = NEMA_REG_CMDADDR; /* Beyond the stop: must not read it. */
        load_words(bus, RING_BASE, ring, n, &e); load_words(bus, LIST_BASE, list, 2u, &e);
        SEMU_TEST_EQ_U64(context, SEMU_OK, nema_framing_parse(bus, RING_BASE,
            n, n - 8u, 4u, on_child, &cap, on_record, &cap, &e));
        SEMU_TEST_EQ_U64(context, 2u, cap.children); SEMU_TEST_EQ_U64(context, 2u, cap.count);
    }
    semu_bus_destroy(bus);
}
static void test_nema_framing_control_fields_refuse_before_callbacks(semu_test_context *context)
{
    semu_error e; semu_bus *bus = semu_bus_create(&e); unsigned mode;
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_ram(bus, "sram", SRAM_BASE, SRAM_SIZE, &e));
    for (mode = 0u; mode < 7u; ++mode) {
        uint32_t ring[] = {NEMA_REG_CMDADDR, LIST_BASE, NEMA_CL_PUSH | NEMA_REG_CMDSIZE, 2u,
            NEMA_HOLDCMD | NEMA_REG_CMDADDR, RING_BASE, NEMA_HOLDCMD | NEMA_REG_CMDSIZE, 256u};
        uint32_t list[] = {0x110u, 7u}; capture_ctx cap = {0};
        if (mode < 3u) ring[5u + mode] ^= 4u;
        else { ring[4] = NEMA_REG_CLID; ring[5] = 7u; ring[6] = NEMA_REG_INTERRUPT;
            ring[7] = mode == 3u ? 0u : 1u; }
        load_words(bus, RING_BASE, ring, 8u, &e); load_words(bus, LIST_BASE, list, 2u, &e);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, nema_framing_parse(bus, RING_BASE,
            64u, 0u, mode < 4u ? 8u : mode + 1u, on_child, &cap, on_record, &cap, &e));
        SEMU_TEST_EQ_U64(context, 0u, cap.children); SEMU_TEST_EQ_U64(context, 0u, cap.count);
        SEMU_TEST_ASSERT(context, e.text[0] != '\0');
        ring[4] = NEMA_REG_CLID; ring[5] = 7u; ring[6] = NEMA_REG_INTERRUPT; ring[7] = 1u;
        load_words(bus, RING_BASE, ring, 8u, &e);
        SEMU_TEST_EQ_U64(context, SEMU_OK, nema_framing_parse(bus, RING_BASE,
            64u, 0u, 8u, on_child, &cap, on_record, &cap, &e));
        SEMU_TEST_EQ_U64(context, 1u, cap.children);
    }
    semu_bus_destroy(bus);
}
static void test_nema_framing_ring_range_overflow(semu_test_context *context)
{
    semu_error e; semu_bus *bus = semu_bus_create(&e);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT, nema_framing_parse(bus,
        0xfffffffcu, 2u, 0u, 0u, NULL, NULL, NULL, NULL, &e));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT, nema_framing_parse(bus,
        RING_BASE, UINT32_MAX, 0u, 0u, NULL, NULL, NULL, NULL, NULL));
    semu_bus_destroy(bus);
}
static void test_nema_framing_unmatched_tail(semu_test_context *context)
{
    semu_error e; semu_bus *bus = semu_bus_create(&e); unsigned held;
    uint32_t ring[] = {NEMA_REG_CMDADDR, LIST_BASE, NEMA_CL_PUSH | NEMA_REG_CMDSIZE, 2u,
        NEMA_REG_CMDADDR, LIST_BASE + 8u, NEMA_CL_PUSH | NEMA_REG_CMDSIZE, 3u};
    uint32_t list[] = {NEMA_REG_CLIPMIN, 0u, NEMA_REG_DRAW_COLOR, 7u, NEMA_REG_CLIPMAX, 0u};
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_ram(bus, "sram", SRAM_BASE, SRAM_SIZE, &e));
    for (held = 0u; held < 2u; ++held) {
        capture_ctx cap = {0};
        list[4] = (held ? NEMA_HOLDCMD : 0u) | NEMA_REG_CLIPMAX;
        ring[7] = 3u;
        load_words(bus, RING_BASE, ring, 8u, &e); load_words(bus, LIST_BASE, list, 6u, &e);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, nema_framing_parse(bus, RING_BASE,
            64u, 0u, 8u, on_child, &cap, on_record, &cap, &e));
        SEMU_TEST_EQ_U64(context, 0u, cap.children); SEMU_TEST_EQ_U64(context, 0u, cap.count);
        ring[7] = 4u; load_words(bus, RING_BASE, ring, 8u, &e);
        SEMU_TEST_EQ_U64(context, SEMU_OK, nema_framing_parse(bus, RING_BASE,
            64u, 0u, 8u, on_child, &cap, on_record, &cap, &e));
        SEMU_TEST_EQ_U64(context, 2u, cap.children); SEMU_TEST_EQ_U64(context, 3u, cap.count);
    }
    semu_bus_destroy(bus);
}
int main(void)
{
    static const semu_test_case cases[] = {SEMU_TEST_CASE(test_guest_gpu_store_precise_fault),
        SEMU_TEST_CASE(test_whole_submission_allocation_failures),
        SEMU_TEST_CASE(test_nema_framing_wrapped_non_power_of_two),
        SEMU_TEST_CASE(test_nema_framing_control_fields_refuse_before_callbacks),
        SEMU_TEST_CASE(test_nema_framing_ring_range_overflow),
        SEMU_TEST_CASE(test_nema_framing_unmatched_tail)};
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
