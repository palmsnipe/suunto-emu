#include "../../src/display/nema_completion.h"
#include "../../src/display/nema_backend.h"
#include "../../src/devices/sapporo_nema_gpu.h"
#include "test.h"
#include <string.h>

static void equal_snapshot(semu_test_context *context, nema_completion *completion,
    const semu_snapshot_writer *before)
{
    semu_error error; semu_snapshot_writer after;
    semu_snapshot_writer_init(&after);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_snapshot_write(completion, &after, &error));
    SEMU_TEST_EQ_U64(context, before->size, after.size);
    SEMU_TEST_ASSERT(context, !memcmp(before->data, after.data, before->size));
    semu_snapshot_writer_destroy(&after);
}

static void test_completion_batch_failure_preserves_queue(semu_test_context *context)
{
    for (unsigned mode = 0u; mode < 4u; ++mode) {
        semu_error error;
        semu_scheduler *scheduler = semu_scheduler_create(&error);
        nema_completion *completion = NULL; semu_snapshot_writer before;
        const uint32_t ids[] = {123u, 124u};
        uint64_t next_id, next_sequence; size_t queued;
        SEMU_TEST_ASSERT(context, scheduler != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_create(&completion, &error));
        for (unsigned i = 0u; i < (mode == 3u ? 63u : 1u); ++i)
            SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule(completion,
                scheduler, i, NULL, NULL, NULL, NULL, &error));
        if (mode == 0u) scheduler->next_id = UINT64_MAX - 1u;
        if (mode == 1u) scheduler->next_sequence = UINT64_MAX - 1u;
        if (mode == 2u) scheduler->now_ns = UINT64_MAX - NEMA_COMPLETION_DELAY_NS + 1u;
        next_id = scheduler->next_id; next_sequence = scheduler->next_sequence;
        queued = semu_scheduler_event_count(scheduler);
        semu_scheduled_event saved[64];
        memcpy(saved, scheduler->events, queued * sizeof(saved[0]));
        semu_snapshot_writer_init(&before);
        SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_snapshot_write(completion, &before, &error));
        SEMU_TEST_ASSERT(context, nema_completion_schedule_batch(completion, scheduler,
            ids, 2u, NULL, NULL, NULL, NULL, &error) != SEMU_OK);
        SEMU_TEST_ASSERT(context, error.code != SEMU_OK && error.text[0] != '\0');
        SEMU_TEST_EQ_U64(context, next_id, scheduler->next_id);
        SEMU_TEST_EQ_U64(context, next_sequence, scheduler->next_sequence);
        SEMU_TEST_EQ_U64(context, queued, semu_scheduler_event_count(scheduler));
        SEMU_TEST_ASSERT(context, !memcmp(saved, scheduler->events, queued * sizeof(saved[0])));
        equal_snapshot(context, completion, &before);
        semu_snapshot_writer_destroy(&before);
        nema_completion_destroy(completion); semu_scheduler_destroy(scheduler);
    }
}

typedef struct { uint32_t values[9]; size_t count; } capture;
static void register_write(void *context, uint32_t offset, uint32_t value)
{
    capture *result = context;
    if (result->count < 9u) result->values[result->count++] = offset == NEMA_REG_CLID ? value : 100u;
}
static void irq(void *context, unsigned line, int asserted)
{
    capture *result = context;
    if (result->count < 9u) result->values[result->count++] = asserted ? line : 0u;
}

static void unrelated_event(void *context, uint64_t now)
{ unsigned *calls = context; (void)now; ++*calls; }

static void test_completion_cancel_does_not_claim_reused_id(semu_test_context *context)
{
    semu_error error; unsigned calls = 0u;
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    nema_completion *completion = NULL; semu_event_id id;
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_create(&completion, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule(completion, scheduler,
        7u, NULL, NULL, NULL, NULL, &error));
    semu_scheduler_reset(scheduler);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_schedule(scheduler, 1u,
        unrelated_event, &calls, &id, &error));
    SEMU_TEST_EQ_U64(context, 1u, id);
    nema_completion_cancel(completion);
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_count(scheduler));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(scheduler, 1u, &error));
    SEMU_TEST_EQ_U64(context, 1u, calls);
    nema_completion_destroy(completion); semu_scheduler_destroy(scheduler);
}

static void test_completion_batch_success_matches_singles(semu_test_context *context)
{
    semu_error error; nema_completion *batch = NULL, *single = NULL;
    semu_scheduler *a = semu_scheduler_create(&error), *b = semu_scheduler_create(&error);
    const uint32_t ids[] = {7u, 8u, 7u, 9u}; capture ac = {{0}, 0}, bc = {{0}, 0};
    semu_snapshot_writer expected;
    SEMU_TEST_ASSERT(context, a != NULL && b != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_create(&batch, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_create(&single, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule_batch(batch, a, ids, 4u,
        register_write, &ac, irq, &ac, &error));
    for (unsigned i = 0u; i < 4u; ++i)
        SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule(single, b, ids[i],
            register_write, &bc, irq, &bc, &error));
    semu_snapshot_writer_init(&expected);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_snapshot_write(single, &expected, &error));
    equal_snapshot(context, batch, &expected);
    SEMU_TEST_EQ_U64(context, 3u, semu_scheduler_event_count(a));
    /* Repeated pending IDs and the empty batch consume no new identity. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule_batch(batch, a, ids, 4u,
        NULL, NULL, NULL, NULL, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule_batch(batch, a, NULL, 0u,
        NULL, NULL, NULL, NULL, NULL));
    equal_snapshot(context, batch, &expected);
    SEMU_TEST_EQ_U64(context, 4u, a->next_id);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(a, NEMA_COMPLETION_DELAY_NS, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(b, NEMA_COMPLETION_DELAY_NS, &error));
    SEMU_TEST_ASSERT(context, !memcmp(&ac, &bc, sizeof(ac)));
    for (unsigned i = 0u; i < 3u; ++i) {
        SEMU_TEST_EQ_U64(context, 7u + i, ac.values[3u * i]);
        SEMU_TEST_EQ_U64(context, 100u, ac.values[3u * i + 1u]);
        SEMU_TEST_EQ_U64(context, 28u, ac.values[3u * i + 2u]);
    }
    semu_snapshot_writer_destroy(&expected);
    nema_completion_destroy(batch); nema_completion_destroy(single);
    semu_scheduler_destroy(a); semu_scheduler_destroy(b);
}

static void test_completion_cancel_reset_destroy_and_ownership(semu_test_context *context)
{
    semu_error error; nema_completion *completion = NULL;
    semu_scheduler *a = semu_scheduler_create(&error), *b = semu_scheduler_create(&error);
    const uint32_t ids[] = {7u, 8u}; semu_snapshot_writer before;
    SEMU_TEST_ASSERT(context, a != NULL && b != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_create(&completion, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule_batch(completion, a, ids, 2u,
        NULL, NULL, NULL, NULL, &error));
    semu_snapshot_writer_init(&before);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_snapshot_write(completion, &before, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT, nema_completion_schedule_batch(completion, b, ids, 2u,
        NULL, NULL, NULL, NULL, &error));
    equal_snapshot(context, completion, &before);
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(b));
    nema_completion_cancel(completion);
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(a));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule_batch(completion, a, ids, 2u,
        NULL, NULL, NULL, NULL, &error));
    nema_completion_reset(completion);
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(a));
    SEMU_TEST_EQ_U64(context, 0u, nema_completion_count(completion));
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_schedule_batch(completion, b, ids, 2u,
        NULL, NULL, NULL, NULL, &error));
    nema_completion_destroy(completion);
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(b));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(a, NEMA_COMPLETION_DELAY_NS, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(b, NEMA_COMPLETION_DELAY_NS, &error));
    semu_snapshot_writer_destroy(&before); semu_scheduler_destroy(a); semu_scheduler_destroy(b);
}

static void test_completion_failed_admission_preserves_snapshot(semu_test_context *context)
{
    for (unsigned mode = 0u; mode < 3u; ++mode) {
        semu_error error;
        semu_scheduler *scheduler = semu_scheduler_create(&error);
        nema_completion *completion = NULL;
        semu_snapshot_writer before, after;
        SEMU_TEST_ASSERT(context, scheduler != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_create(&completion, &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_restore_begin(scheduler,
            mode == 0u ? UINT64_MAX : 0u, 0u, 1u, &error));
        if (mode == 1u) scheduler->next_id = UINT64_MAX;
        if (mode == 2u) scheduler->next_sequence = UINT64_MAX;
        semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
        SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_snapshot_write(completion, &before, &error));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, nema_completion_schedule(completion,
            scheduler, 123u, NULL, NULL, NULL, NULL, &error));
        SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(scheduler));
        SEMU_TEST_EQ_U64(context, SEMU_OK, nema_completion_snapshot_write(completion, &after, &error));
        SEMU_TEST_EQ_U64(context, before.size, after.size);
        SEMU_TEST_ASSERT(context, memcmp(before.data, after.data, before.size) == 0);
        semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
        nema_completion_destroy(completion); semu_scheduler_destroy(scheduler);
    }
}

typedef struct { unsigned count; uint16_t pixels[4]; } inline_frames;
static void inline_frame(void *context, const semu_frame *frame)
{
    inline_frames *f = context;
    if (f->count < 4u) f->pixels[f->count] = frame->pixels[0] | (uint16_t)frame->pixels[1] << 8u;
    ++f->count;
}
static void test_inline_completion_transaction(semu_test_context *context)
{
    const uint32_t base = 0x10000000u, stop = NEMA_GPU_BASE + 0xecu;
    const uint32_t clear[] = {NEMA_REG_TEX0_BASE, 0x10000800u,
        NEMA_REG_TEX0_FSTRIDE, 0x040001e0u, NEMA_REG_TEX0_RESXY, 0x00f000f0u,
        NEMA_REG_CLIPMIN, 0u, NEMA_REG_CLIPMAX, 0x10001u,
        NEMA_REG_POINT0_X, 0u, NEMA_REG_POINT0_Y, 0u,
        NEMA_REG_POINT1_X, 0x10000u, NEMA_REG_POINT1_Y, 0u,
        NEMA_REG_POINT2_X, 0x10000u, NEMA_REG_POINT2_Y, 0x10000u,
        NEMA_REG_POINT3_X, 0u, NEMA_REG_POINT3_Y, 0x10000u,
        NEMA_REG_DRAW_COLOR, 0x1fu, NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD};
    uint32_t ring[] = {NEMA_REG_DRAW_COLOR, NEMA_REG_CMDADDR,
        NEMA_REG_CMDADDR, base + 512u, NEMA_CL_PUSH | NEMA_REG_CMDSIZE, 2u,
        NEMA_REG_DRAW_COLOR, 0x7e0u,
        NEMA_REG_CMDADDR, base + 512u, NEMA_CL_PUSH | NEMA_REG_CMDSIZE, 2u,
        NEMA_REG_CLID, 7u, NEMA_REG_INTERRUPT, 1u,
        NEMA_REG_DRAW_COLOR, 0xf800u, NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD};
    semu_error e; semu_bus *bus = semu_bus_create(&e);
    semu_scheduler *scheduler = semu_scheduler_create(&e);
    semu_nema_backend *backend = semu_nema_backend_create(&e); semu_nema_gpu *gpu;
    semu_snapshot_writer before, after; inline_frames frames = {0}; unsigned i, mode;
    SEMU_TEST_ASSERT(context, bus && scheduler && backend);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_ram(bus, "ring", base, 4096u, &e));
    for (i = 0u; i < SEMU_ARRAY_LEN(clear); ++i)
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, base + 1024u + 4u * i, 4u, clear[i], &e));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, semu_nema_backend_submit(backend, bus,
        base + 1024u, SEMU_ARRAY_LEN(clear), 0u, inline_frame, &frames, &e));
    gpu = semu_nema_gpu_create(bus, &semu_nema_backend_ops, backend,
        inline_frame, &frames, NULL, NULL, scheduler, &e);
    SEMU_TEST_ASSERT(context, gpu != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_attach(gpu, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDADDR, 4u, base, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, NEMA_GPU_BASE + NEMA_REG_CMDSIZE, 4u, 256u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, stop, 4u, base | 6u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, NEMA_GPU_BASE + 0xfcu, 4u, 0u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, base + 512u, 4u, NEMA_REG_DRAW_CMD, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, base + 516u, 4u, NEMA_DRAW_QUAD, &e));
    semu_snapshot_writer_init(&before);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_snapshot_write(gpu, &before, &e));
    for (mode = 0u; mode < 4u; ++mode) {
        ring[6] = mode == 0u ? 0x7770u : mode == 1u ? 0xff007770u :
            mode == 2u ? NEMA_CL_NOP | 1u : NEMA_REG_DRAW_COLOR;
        ring[19] = mode == 3u ? 0xdeadbeefu : NEMA_DRAW_QUAD;
        ring[1] = 0x1fu;
        for (i = 0u; i < SEMU_ARRAY_LEN(ring); ++i)
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, base + 4u * i, 4u, ring[i], &e));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, semu_bus_write(bus, stop, 4u, base + 80u, &e));
        SEMU_TEST_EQ_U64(context, 1u, frames.count);
        SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(scheduler));
        SEMU_TEST_EQ_U64(context, 0x1fu, semu_nema_backend_frame(backend)->pixels[0]);
        semu_snapshot_writer_init(&after);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_snapshot_write(gpu, &after, &e));
        SEMU_TEST_EQ_U64(context, before.size, after.size);
        SEMU_TEST_ASSERT(context, memcmp(before.data, after.data, before.size) == 0);
        semu_snapshot_writer_destroy(&after);
    }
    ring[6] = NEMA_REG_DRAW_COLOR; ring[1] = NEMA_REG_CMDADDR;
    ring[19] = NEMA_DRAW_QUAD;
    for (i = 0u; i < SEMU_ARRAY_LEN(ring); ++i)
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, base + 4u * i, 4u, ring[i], &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, stop, 4u, base + 80u, &e));
    SEMU_TEST_EQ_U64(context, 3u, frames.count);
    SEMU_TEST_EQ_U64(context, NEMA_REG_CMDADDR, frames.pixels[1]);
    SEMU_TEST_EQ_U64(context, 0x7e0u, frames.pixels[2]);
    /* The final inline draw commits, but is not another child publication. */
    SEMU_TEST_EQ_U64(context, 0xf8u, semu_nema_backend_frame(backend)->pixels[1]);
    SEMU_TEST_EQ_U64(context, 3u, semu_nema_backend_frame(backend)->generation);
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_count(scheduler));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_run_next(scheduler, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, stop, 4u, base + 80u, &e));
    SEMU_TEST_EQ_U64(context, 3u, frames.count);
    semu_snapshot_writer_destroy(&before); semu_nema_gpu_destroy(gpu);
    semu_nema_backend_destroy(backend); semu_scheduler_destroy(scheduler); semu_bus_destroy(bus);
}
int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_completion_failed_admission_preserves_snapshot),
        SEMU_TEST_CASE(test_completion_batch_failure_preserves_queue),
        SEMU_TEST_CASE(test_completion_batch_success_matches_singles),
        SEMU_TEST_CASE(test_completion_cancel_does_not_claim_reused_id),
        SEMU_TEST_CASE(test_completion_cancel_reset_destroy_and_ownership),
        SEMU_TEST_CASE(test_inline_completion_transaction)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
