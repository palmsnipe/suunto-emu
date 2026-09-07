#include "../../src/devices/sapporo_nema_gpu.h"
#include "../../src/display/nema_backend.h"
#include "../../src/display/nema_completion.h"
#include "test.h"
#include <string.h>

#define RING 0x10000000u
#define STOP (NEMA_GPU_BASE + 0xecu)
typedef struct { semu_bus *bus; semu_scheduler *scheduler;
    semu_nema_backend *backend; semu_nema_gpu *gpu; unsigned frames, irqs;
    semu_test_context *reentrant; } fixture;
static void frame(void *context, const semu_frame *p)
{
    fixture *f = context; (void)p; ++f->frames;
    if (f->reentrant != NULL) {
        semu_test_context *context = f->reentrant;
        semu_error e; semu_snapshot_writer w; semu_snapshot_reader r;
        semu_snapshot_writer_init(&w); semu_snapshot_reader_init(&r, NULL, 0u);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT, semu_nema_gpu_reset(f->gpu));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
            semu_bus_write(f->bus, STOP, 4u, RING + 32u, &e));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
            semu_nema_gpu_snapshot_write(f->gpu, &w, &e));
        SEMU_TEST_EQ_U64(context, 0u, w.size);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
            semu_nema_gpu_snapshot_read(f->gpu, &r, &e));
        semu_snapshot_writer_destroy(&w);
    }
}
static void irq(void *context, unsigned line, int level)
{ fixture *f = context; if (line == 28u && level) ++f->irqs; }
static void put(fixture *f, uint32_t address, uint32_t value)
{ semu_error e; (void)semu_bus_write(f->bus, address, 4u, value, &e); }
static void pair(fixture *f, uint32_t offset, uint32_t reg, uint32_t value)
{ put(f, RING + offset, reg); put(f, RING + offset + 4u, value); }
static int init_config(fixture *f, const semu_display_backend_ops *ops, int scheduled)
{
    semu_error e; memset(f, 0, sizeof(*f));
    f->bus = semu_bus_create(&e); f->scheduler = semu_scheduler_create(&e);
    f->backend = semu_nema_backend_create(&e);
    if (!f->bus || !f->scheduler || !f->backend || semu_bus_map_ram(f->bus,
        "synthetic", RING, 4096u, &e) != SEMU_OK) return 0;
    f->gpu = semu_nema_gpu_create(f->bus, ops, f->backend,
        frame, f, irq, f, scheduled ? f->scheduler : NULL, &e);
    if (!f->gpu || semu_nema_gpu_attach(f->gpu, &e) != SEMU_OK) return 0;
    put(f, NEMA_GPU_BASE + NEMA_REG_CMDADDR, RING);
    put(f, NEMA_GPU_BASE + NEMA_REG_CMDSIZE, 256u);
    put(f, STOP, RING | 6u); put(f, NEMA_GPU_BASE + 0xfcu, 0u);
    return 1;
}
static int init_ops(fixture *f, const semu_display_backend_ops *ops)
{ return init_config(f, ops, 1); }
static int init(fixture *f) { return init_ops(f, &semu_nema_backend_ops); }
static void finish(fixture *f)
{
    semu_nema_gpu_destroy(f->gpu); semu_nema_backend_destroy(f->backend);
    semu_scheduler_destroy(f->scheduler); semu_bus_destroy(f->bus);
}
static void children(fixture *f)
{
    pair(f, 0u, NEMA_REG_CMDADDR, RING + 256u);
    pair(f, 8u, NEMA_CL_PUSH | NEMA_REG_CMDSIZE, 4u);
    pair(f, 16u, NEMA_REG_CMDADDR, RING + 512u);
    pair(f, 24u, NEMA_CL_PUSH | NEMA_REG_CMDSIZE, 2u);
    pair(f, 256u, NEMA_REG_CLIPMIN, 0u); pair(f, 264u, NEMA_REG_CLIPMAX, 0u);
    pair(f, 512u, NEMA_REG_DRAW_CMD, 0xdeadbeefu);
}
static void unchanged(semu_test_context *context, fixture *f,
    const semu_snapshot_writer *before)
{
    semu_error e; semu_snapshot_writer after;
    semu_snapshot_writer_init(&after);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_snapshot_write(f->gpu, &after, &e));
    SEMU_TEST_EQ_U64(context, before->size, after.size);
    SEMU_TEST_ASSERT(context, memcmp(before->data, after.data, after.size) == 0);
    semu_snapshot_writer_destroy(&after);
}
static void test_later_child_refusal_and_retry(semu_test_context *context)
{
    fixture f; semu_error e; semu_snapshot_writer before;
    SEMU_TEST_ASSERT(context, init(&f)); children(&f);
    semu_snapshot_writer_init(&before);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_snapshot_write(f.gpu, &before, &e));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        semu_bus_write(f.bus, STOP, 4u, RING + 32u, &e));
    SEMU_TEST_ASSERT(context, strcmp(e.text, "nema_state: unsupported draw cmd 0xdeadbeef") == 0);
    SEMU_TEST_EQ_U64(context, 0u, f.frames); unchanged(context, &f, &before);
    pair(&f, 512u, NEMA_REG_DRAW_COLOR, 0x001fu);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(f.bus, STOP, 4u, RING + 32u, &e));
    SEMU_TEST_EQ_U64(context, 2u, f.frames);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(f.bus, STOP, 4u, RING + 32u, &e));
    SEMU_TEST_EQ_U64(context, 2u, f.frames); SEMU_TEST_EQ_U64(context, 0u, f.irqs);
    semu_snapshot_writer_destroy(&before); finish(&f);
}
static void test_completion_failure_does_not_publish(semu_test_context *context)
{
    fixture f; semu_error e; semu_snapshot_writer before;
    SEMU_TEST_ASSERT(context, init(&f)); children(&f);
    pair(&f, 16u, NEMA_REG_CLID, 1u); pair(&f, 24u, NEMA_REG_INTERRUPT, 1u);
    pair(&f, 32u, NEMA_REG_CLID, 2u); pair(&f, 40u, NEMA_REG_INTERRUPT, 1u);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_restore_begin(
        f.scheduler, 0u, 0u, UINT64_MAX - 1u, &e));
    semu_snapshot_writer_init(&before);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_snapshot_write(f.gpu, &before, &e));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        semu_bus_write(f.bus, STOP, 4u, RING + 48u, &e));
    SEMU_TEST_EQ_U64(context, 0u, f.frames);
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(f.scheduler));
    SEMU_TEST_EQ_U64(context, UINT64_MAX - 1u, f.scheduler->next_id);
    unchanged(context, &f, &before);
    semu_snapshot_writer_destroy(&before); finish(&f);
}
static void test_two_markers_retry_and_reset(semu_test_context *context)
{
    fixture f; semu_error e; uint32_t value; semu_snapshot_writer before;
    unsigned mode;
    SEMU_TEST_ASSERT(context, init(&f));
    /* List IDs may resemble ring opcodes; they are payload, not commands. */
    pair(&f, 0u, NEMA_REG_CLID, NEMA_REG_CLID);
    pair(&f, 8u, NEMA_REG_INTERRUPT, 1u);
    pair(&f, 16u, NEMA_REG_CLID, NEMA_REG_CMDADDR);
    pair(&f, 24u, NEMA_REG_INTERRUPT, 1u);
    for (mode = 0u; mode < 2u; ++mode) {
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_restore_begin(f.scheduler,
            mode == 0u ? UINT64_MAX - 10u : 0u, mode == 1u ? UINT64_MAX - 1u : 0u,
            1u, &e));
        semu_snapshot_writer_init(&before);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_snapshot_write(f.gpu, &before, &e));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
            semu_bus_write(f.bus, STOP, 4u, RING + 32u, NULL));
        unchanged(context, &f, &before);
        SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(f.scheduler));
        semu_snapshot_writer_destroy(&before);
    }
    semu_scheduler_reset(f.scheduler);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(f.bus, STOP, 4u, RING + 32u, &e));
    SEMU_TEST_EQ_U64(context, 2u, semu_scheduler_event_count(f.scheduler));
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_get(f.scheduler, 0u)->id);
    SEMU_TEST_EQ_U64(context, 2u, semu_scheduler_event_get(f.scheduler, 1u)->id);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_run_next(f.scheduler, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_read(f.bus, NEMA_GPU_BASE + NEMA_REG_CLID, 4u, &value, &e));
    SEMU_TEST_EQ_U64(context, NEMA_REG_CLID, value);
    SEMU_TEST_EQ_U64(context, 1u, f.irqs);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_reset(f.gpu));
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(f.scheduler));
    SEMU_TEST_EQ_U64(context, 0u, f.frames);
    finish(&f);
}
static unsigned response_mode, commits, aborts;
static semu_transaction_result response(void *context, semu_bus *bus,
    const semu_display_list *lists, size_t count, uint64_t time,
    semu_frame_callback callback, void *frame_context, semu_error *error)
{
    (void)context; (void)bus; (void)lists; (void)count; (void)time;
    (void)callback; (void)frame_context;
    if (response_mode == 0u) semu_error_set(error, SEMU_ERR_IO, "original backend detail");
    return response_mode < 2u ? SEMU_TRANSACTION_REFUSE :
        response_mode == 2u ? SEMU_TRANSACTION_WAIT : (semu_transaction_result)99u;
}
static void count_commit(void *context) { (void)context; ++commits; }
static void count_abort(void *context) { (void)context; ++aborts; }
static void test_callback_results_and_missing_backend(semu_test_context *context)
{
    static const semu_display_backend_ops ops = {response, count_commit, count_abort};
    fixture f; semu_error e; semu_snapshot_writer before;
    commits = aborts = 0u;
    for (response_mode = 0u; response_mode < 5u; ++response_mode) {
        SEMU_TEST_ASSERT(context, init_ops(&f, response_mode == 4u ? NULL : &ops));
        children(&f); pair(&f, 512u, NEMA_REG_DRAW_COLOR, 0u);
        semu_snapshot_writer_init(&before);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_snapshot_write(f.gpu, &before, &e));
        SEMU_TEST_EQ_U64(context, response_mode == 0u ? SEMU_ERR_IO : SEMU_ERR_UNSUPPORTED,
            semu_bus_write(f.bus, STOP, 4u, RING + 32u, &e));
        SEMU_TEST_ASSERT(context, e.text[0] != '\0');
        if (response_mode == 0u)
            SEMU_TEST_ASSERT(context, strcmp(e.text, "original backend detail") == 0);
        unchanged(context, &f, &before);
        semu_snapshot_writer_destroy(&before); finish(&f);
    }
    SEMU_TEST_EQ_U64(context, 0u, commits); SEMU_TEST_EQ_U64(context, 2u, aborts);
}
static void test_reentrant_callbacks(semu_test_context *context)
{
    fixture f; semu_error e; uint32_t value;
    SEMU_TEST_ASSERT(context, init(&f)); children(&f);
    pair(&f, 512u, NEMA_REG_DRAW_COLOR, 0u); f.reentrant = context;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(f.bus, STOP, 4u, RING + 32u, &e));
    SEMU_TEST_EQ_U64(context, 2u, f.frames);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(f.bus, STOP, 4u, &value, &e));
    SEMU_TEST_EQ_U64(context, RING + 32u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(f.bus, STOP, 4u, RING + 32u, &e));
    SEMU_TEST_EQ_U64(context, 2u, f.frames);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_reset(f.gpu)); finish(&f);
}
static void test_missing_scheduler_and_partial_backend(semu_test_context *context)
{
    fixture f; semu_error e; semu_snapshot_writer before;
    semu_display_backend_ops partial = semu_nema_backend_ops;
    SEMU_TEST_ASSERT(context, init_config(&f, &semu_nema_backend_ops, 0)); children(&f);
    pair(&f, 16u, NEMA_REG_CLID, 1u); pair(&f, 24u, NEMA_REG_INTERRUPT, 1u);
    semu_snapshot_writer_init(&before);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_snapshot_write(f.gpu, &before, &e));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_bus_write(f.bus, STOP, 4u, RING + 32u, &e));
    SEMU_TEST_EQ_U64(context, 0u, f.frames); unchanged(context, &f, &before);
    partial.abort = NULL;
    SEMU_TEST_ASSERT(context, semu_nema_gpu_create(f.bus, &partial, f.backend,
        NULL, NULL, NULL, NULL, NULL, &e) == NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT, e.code);
    semu_snapshot_writer_destroy(&before); finish(&f);
}
static void test_bad_wrap_preserves_submission(semu_test_context *context)
{
    fixture f; semu_error e; semu_snapshot_writer before; unsigned mode;
    SEMU_TEST_ASSERT(context, init(&f)); children(&f);
    pair(&f, 16u, NEMA_HOLDCMD | NEMA_REG_CMDADDR, RING);
    pair(&f, 24u, NEMA_HOLDCMD | NEMA_REG_CMDSIZE, 256u);
    semu_snapshot_writer_init(&before);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_snapshot_write(f.gpu, &before, &e));
    for (mode = 0u; mode < 3u; ++mode) {
        uint32_t address = RING + 20u + mode * 4u, value;
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(f.bus, address, 4u, &value, &e));
        put(&f, address, value ^ 4u);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
            semu_bus_write(f.bus, STOP, 4u, RING + 32u, &e));
        unchanged(context, &f, &before);
        SEMU_TEST_EQ_U64(context, 0u, f.frames); SEMU_TEST_EQ_U64(context, 0u, f.irqs);
        SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(f.scheduler));
        put(&f, address, value);
    }
    /* Native marker builders also emit a held jump to the next ring word,
     * not just the bootstrap trailer's jump back to the base. */
    put(&f, RING + 20u, RING + 32u);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(f.bus, STOP, 4u, RING + 32u, &e));
    SEMU_TEST_EQ_U64(context, 1u, f.frames);
    semu_snapshot_writer_destroy(&before); finish(&f);
}
static void test_unsupported_access_widths(semu_test_context *context)
{
    fixture f; semu_error e; semu_snapshot_writer before; unsigned width;
    SEMU_TEST_ASSERT(context, init(&f)); children(&f);
    semu_snapshot_writer_init(&before);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_snapshot_write(f.gpu, &before, &e));
    for (width = 1u; width <= 2u; ++width) {
        uint32_t value = 0x12345678u;
        SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, semu_bus_read(f.bus, STOP, width, &value, &e));
        SEMU_TEST_EQ_U64(context, 0x12345678u, value);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, semu_bus_write(f.bus, STOP, width, 0u, NULL));
        unchanged(context, &f, &before);
    }
    SEMU_TEST_EQ_U64(context, 0u, f.frames); SEMU_TEST_EQ_U64(context, 0u, f.irqs);
    semu_snapshot_writer_destroy(&before); finish(&f);
}
static void test_odd_child_refusal_and_retry(semu_test_context *context)
{
    fixture f; semu_error e; semu_snapshot_writer before; unsigned held;
    SEMU_TEST_ASSERT(context, init(&f)); children(&f);
    pair(&f, 512u, NEMA_REG_DRAW_COLOR, 0xf800u);
    put(&f, RING + 28u, 3u);
    pair(&f, 32u, NEMA_REG_CLID, 7u); pair(&f, 40u, NEMA_REG_INTERRUPT, 1u);
    semu_snapshot_writer_init(&before);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_snapshot_write(f.gpu, &before, &e));
    for (held = 0u; held < 2u; ++held) {
        pair(&f, 520u, (held ? NEMA_HOLDCMD : 0u) | NEMA_REG_CLIPMAX, 0u);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
            semu_bus_write(f.bus, STOP, 4u, RING + 48u, &e));
        SEMU_TEST_ASSERT(context, strstr(e.text, "child entries 3") != NULL);
        unchanged(context, &f, &before);
        SEMU_TEST_EQ_U64(context, 0u, f.frames); SEMU_TEST_EQ_U64(context, 0u, f.irqs);
        SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(f.scheduler));
    }
    put(&f, RING + 28u, 4u);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(f.bus, STOP, 4u, RING + 48u, &e));
    SEMU_TEST_EQ_U64(context, 2u, f.frames);
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_count(f.scheduler));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_run_next(f.scheduler, &e));
    SEMU_TEST_EQ_U64(context, 1u, f.irqs);
    semu_snapshot_writer_destroy(&before); finish(&f);
}
int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_later_child_refusal_and_retry),
        SEMU_TEST_CASE(test_completion_failure_does_not_publish),
        SEMU_TEST_CASE(test_two_markers_retry_and_reset),
        SEMU_TEST_CASE(test_callback_results_and_missing_backend),
        SEMU_TEST_CASE(test_reentrant_callbacks),
        SEMU_TEST_CASE(test_missing_scheduler_and_partial_backend),
        SEMU_TEST_CASE(test_bad_wrap_preserves_submission),
        SEMU_TEST_CASE(test_unsupported_access_widths),
        SEMU_TEST_CASE(test_odd_child_refusal_and_retry)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
