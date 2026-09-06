#include "../../src/display/nema_backend.h"
#include "test.h"
#include <string.h>
#include <stdlib.h>
#include "../../src/display/nema_backend_internal.h"
#include "../../src/display/nema_tsc6a_internal.h"

/* Compile the production transaction path with a deterministic allocation
 * fault. This is not a second implementation or a runtime allocator hook. */
static unsigned allocation_calls;
static int refuse_allocation;
static void *test_malloc(size_t size)
{ ++allocation_calls; return refuse_allocation ? NULL : malloc(size); }
#define malloc test_malloc
#define semu_nema_backend_ops fault_ops
#include "../../src/display/nema_backend_transaction.c"
#undef semu_nema_backend_ops
#undef malloc

#define BASE 0x10000000u
typedef struct { semu_bus *bus; semu_nema_backend *backend; unsigned frames; } fixture;
static void frame(void *context, const semu_frame *value)
{ fixture *f = context; (void)value; ++f->frames; }
static int init(fixture *f)
{
    semu_error e;
    memset(f, 0, sizeof(*f));
    f->bus = semu_bus_create(&e); f->backend = semu_nema_backend_create(&e);
    return f->bus && f->backend &&
        semu_bus_map_ram(f->bus, "synthetic", BASE, 4096u, &e) == SEMU_OK;
}
static void finish(fixture *f)
{ semu_nema_backend_destroy(f->backend); semu_bus_destroy(f->bus); }
static void pair(fixture *f, uint32_t offset, uint32_t reg, uint32_t value)
{
    semu_error e;
    (void)semu_bus_write(f->bus, BASE + offset, 4u, reg, &e);
    (void)semu_bus_write(f->bus, BASE + offset + 4u, 4u, value, &e);
}
static uint32_t clear_list(fixture *f)
{
    static const uint32_t words[] = {
        NEMA_REG_TEX0_BASE, BASE + 0x800u, NEMA_REG_TEX0_FSTRIDE, 0x040001e0u,
        NEMA_REG_TEX0_RESXY, 0x00f000f0u, NEMA_REG_CLIPMIN, 0u,
        NEMA_REG_CLIPMAX, 0x00010001u, NEMA_REG_POINT0_X, 0u,
        NEMA_REG_POINT0_Y, 0u, NEMA_REG_POINT1_X, 0x10000u,
        NEMA_REG_POINT1_Y, 0u, NEMA_REG_POINT2_X, 0x10000u,
        NEMA_REG_POINT2_Y, 0x10000u, NEMA_REG_POINT3_X, 0u,
        NEMA_REG_POINT3_Y, 0x10000u, NEMA_REG_DRAW_COLOR, 0x001fu,
        NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD
    };
    size_t i;
    for (i = 0u; i < SEMU_ARRAY_LEN(words); i += 2u)
        pair(f, (uint32_t)i * 4u, words[i], words[i + 1u]);
    return (uint32_t)SEMU_ARRAY_LEN(words);
}
static semu_transaction_result submit(fixture *f, uint32_t offset,
    uint32_t words, semu_error *e)
{ return semu_nema_backend_submit(f->backend, f->bus, BASE + offset, words,
    0u, frame, f, e); }
static void test_refused_list_preserves_inherited_state(semu_test_context *context)
{
    fixture f; semu_error e; uint8_t before[NEMA_BACKEND_PANEL_BYTES];
    const semu_frame *p;
    SEMU_TEST_ASSERT(context, init(&f));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, 0u, clear_list(&f), &e));
    p = semu_nema_backend_frame(f.backend); memcpy(before, p->pixels, sizeof(before));
    pair(&f, 256u, NEMA_REG_DRAW_COLOR, 0xf800u);
    pair(&f, 264u, 0x55000004u, 0u);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, submit(&f, 256u, 4u, &e));
    SEMU_TEST_ASSERT(context, memcmp(before, p->pixels, sizeof(before)) == 0);
    SEMU_TEST_EQ_U64(context, 1u, f.frames);
    pair(&f, 256u, NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, 256u, 2u, &e));
    SEMU_TEST_ASSERT(context, memcmp(before, p->pixels, sizeof(before)) == 0);
    finish(&f);
}
static void test_syntax_refusal_has_diagnostic(semu_test_context *context)
{
    fixture f; semu_error e;
    SEMU_TEST_ASSERT(context, init(&f));
    pair(&f, 0u, 0x55000004u, 0u); semu_error_clear(&e);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, submit(&f, 0u, 2u, &e));
    SEMU_TEST_ASSERT(context, e.code != SEMU_OK && e.text[0] != '\0');
    finish(&f);
}
static void test_draw_refusal_retains_original_error(semu_test_context *context)
{
    fixture f; semu_error e; size_t i;
    SEMU_TEST_ASSERT(context, init(&f));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, 0u, clear_list(&f), &e));
    pair(&f, 256u, NEMA_REG_TEX0_FSTRIDE, 0x170005a0u);
    pair(&f, 264u, NEMA_REG_DRAW_CMD, NEMA_DRAW_TRI_SOLID);
    for (i = 0u; i < NEMA_DIAG_MAX_RECORDS + 1u; ++i) {
        semu_error_clear(&e);
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, submit(&f, 256u, 4u, &e));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, e.code);
        SEMU_TEST_ASSERT(context, strcmp(e.text,
            "nema_tsc6a: unsupported target draw 0x00000004") == 0);
    }
    SEMU_TEST_EQ_U64(context, 1u, f.frames);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, submit(&f, 256u, 4u, NULL));
    finish(&f);
}
typedef struct { fixture *f; unsigned count; uint64_t generation[2];
    uint16_t pixel[2]; int conflict; } publications;
static void collect(void *context, const semu_frame *p)
{
    publications *out = context;
    semu_error e;
    unsigned n = out->count++;
    if (n < 2u) {
        out->generation[n] = p->generation;
        out->pixel[n] = (uint16_t)(p->pixels[0] | (uint16_t)p->pixels[1] << 8u);
    }
    out->conflict += semu_nema_backend_reset(out->f->backend) == SEMU_ERR_CONFLICT;
    out->conflict += submit(out->f, 0u, 0u, &e) == SEMU_TRANSACTION_REFUSE &&
        e.code == SEMU_ERR_CONFLICT;
    semu_nema_backend_ops.abort(out->f->backend);
    semu_nema_backend_ops.commit(out->f->backend);
}
static void test_batch_abort_commit_and_publication_order(semu_test_context *context)
{
    fixture f, other; semu_error e; semu_display_list lists[2];
    publications out = {0}; uint8_t before[NEMA_BACKEND_PANEL_BYTES];
    SEMU_TEST_ASSERT(context, init(&f) && init(&other));
    lists[0] = (semu_display_list){BASE, clear_list(&f)};
    lists[1] = (semu_display_list){BASE + 256u, 4u};
    pair(&f, 256u, NEMA_REG_DRAW_COLOR, 0xf800u);
    pair(&f, 264u, NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD);
    memcpy(before, semu_nema_backend_frame(f.backend)->pixels, sizeof(before));
    out.f = &f;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, semu_nema_backend_ops.prepare(
        f.backend, f.bus, lists, 2u, 0u, collect, &out, &e));
    SEMU_TEST_EQ_U64(context, 0u, out.count);
    SEMU_TEST_ASSERT(context, memcmp(before, semu_nema_backend_frame(f.backend)->pixels,
        sizeof(before)) == 0);
    SEMU_TEST_EQ_U64(context, 0u, nema_state_snapshot_count(f.backend->state));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT, semu_nema_backend_reset(f.backend));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&other, 0u, clear_list(&other), &e));
    semu_nema_backend_ops.abort(f.backend);
    SEMU_TEST_EQ_U64(context, 0u, nema_state_snapshot_count(f.backend->state));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, semu_nema_backend_ops.prepare(
        f.backend, f.bus, lists, 2u, 0u, collect, &out, &e));
    pair(&f, 256u, NEMA_REG_DRAW_COLOR, 0x07e0u); /* commit must not reread guest */
    semu_nema_backend_ops.commit(f.backend);
    SEMU_TEST_EQ_U64(context, 2u, out.count);
    SEMU_TEST_EQ_U64(context, 4u, out.conflict);
    SEMU_TEST_EQ_U64(context, 1u, out.generation[0]);
    SEMU_TEST_EQ_U64(context, 2u, out.generation[1]);
    SEMU_TEST_EQ_U64(context, 0x001fu, out.pixel[0]);
    SEMU_TEST_EQ_U64(context, 0xf800u, out.pixel[1]);
    SEMU_TEST_EQ_U64(context, 2u, nema_state_snapshot_count(f.backend->state));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_backend_reset(f.backend));
    finish(&f); finish(&other);
}
static void test_later_child_and_allocation_refusal(semu_test_context *context)
{
    fixture f; semu_error e; semu_display_list lists[2]; unsigned i;
    SEMU_TEST_ASSERT(context, init(&f));
    lists[0] = (semu_display_list){BASE, clear_list(&f)};
    lists[1] = (semu_display_list){BASE + 256u, 2u};
    pair(&f, 256u, NEMA_REG_DRAW_CMD, 0xdeadbeefu);
    for (i = 0u; i < 4u; ++i) {
        /* Also refuse after the first draw within the first child. */
        if (i == 3u) {
            pair(&f, 120u, NEMA_REG_DRAW_CMD, 0xdeadbeefu);
            lists[0].word_count = 32u;
        }
        refuse_allocation = i == 1u;
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, fault_ops.prepare(
            f.backend, f.bus, lists, 2u, 0u, frame, &f, &e));
        SEMU_TEST_EQ_U64(context, i == 1u ? SEMU_ERR_NOMEM : SEMU_ERR_UNSUPPORTED, e.code);
        SEMU_TEST_EQ_U64(context, 0u, f.frames);
        SEMU_TEST_EQ_U64(context, 0u, nema_state_snapshot_count(f.backend->state));
        SEMU_TEST_EQ_U64(context, 0u, semu_nema_backend_frame(f.backend)->pixels[0]);
    }
    lists[0].word_count = 30u;
    pair(&f, 256u, NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, fault_ops.prepare(
        f.backend, f.bus, lists, 2u, 0u, frame, &f, &e));
    allocation_calls = 0u; refuse_allocation = 1;
    fault_ops.commit(f.backend);
    SEMU_TEST_EQ_U64(context, 0u, allocation_calls);
    SEMU_TEST_EQ_U64(context, 2u, f.frames);
    refuse_allocation = 0;
    finish(&f);
}
static void test_shadow_abort_refusal_and_commit(semu_test_context *context)
{
    fixture f; semu_error e; size_t i; semu_display_list lists[2];
    SEMU_TEST_ASSERT(context, init(&f));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, 0u, clear_list(&f), &e));
    pair(&f, 256u, NEMA_REG_TEX0_FSTRIDE, TSC6A_TARGET_FSTRIDE);
    pair(&f, 264u, NEMA_REG_TEX0_RESXY, TSC6A_RESOLUTION);
    pair(&f, 272u, NEMA_REG_MATMULT, TSC6A_OBSERVED_MATMULT);
    pair(&f, 280u, NEMA_REG_CODEPTR, TSC6A_OBSERVED_CODE);
    pair(&f, 288u, NEMA_REG_DRAW_COLOR, 0xffff0000u);
    pair(&f, 296u, NEMA_REG_DRAW_CMD, NEMA_DRAW_TRI_AA);
    pair(&f, 512u, NEMA_REG_DRAW_CMD, 0xdeadbeefu);
    lists[0] = (semu_display_list){BASE + 256u, 12u};
    lists[1] = (semu_display_list){BASE + 512u, 2u};
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, semu_nema_backend_ops.prepare(
        f.backend, f.bus, lists, 1u, 0u, NULL, NULL, &e));
    SEMU_TEST_ASSERT(context, f.backend->pending_tsc6a->pixels[0] != 0u);
    semu_nema_backend_ops.abort(f.backend);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, semu_nema_backend_ops.prepare(
        f.backend, f.bus, lists, 2u, 0u, NULL, NULL, &e));
    for (i = 0u; i < NEMA_TSC6A_PIXELS; ++i)
        SEMU_TEST_EQ_U64(context, 0u, f.backend->tsc6a->pixels[i]);
    SEMU_TEST_EQ_U64(context, 1u, nema_state_snapshot_count(f.backend->state));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, semu_nema_backend_ops.prepare(
        f.backend, f.bus, lists, 1u, 0u, NULL, NULL, &e));
    semu_nema_backend_ops.commit(f.backend);
    SEMU_TEST_ASSERT(context, f.backend->tsc6a->pixels[0] != 0u);
    SEMU_TEST_EQ_U64(context, 2u, nema_state_snapshot_count(f.backend->state));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_backend_reset(f.backend));
    for (i = 0u; i < NEMA_TSC6A_PIXELS; ++i)
        SEMU_TEST_EQ_U64(context, 0u, f.backend->tsc6a->pixels[i]);
    finish(&f);
}
static void test_batch_bounds_and_empty(semu_test_context *context)
{
    fixture f; semu_error e;
    semu_display_list list = {BASE + 1u, 2u};
    SEMU_TEST_ASSERT(context, init(&f));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, semu_nema_backend_ops.prepare(
        f.backend, f.bus, &list, 1u, 0u, frame, &f, NULL));
    list = (semu_display_list){0xfffffffcu, 2u};
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, semu_nema_backend_ops.prepare(
        f.backend, f.bus, &list, 1u, 0u, frame, &f, &e));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, semu_nema_backend_ops.prepare(
        f.backend, f.bus, &list, SEMU_DISPLAY_MAX_LISTS + 1u, 0u, frame, &f, &e));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, semu_nema_backend_ops.prepare(
        f.backend, f.bus, NULL, 1u, 0u, frame, &f, &e));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, semu_nema_backend_ops.prepare(
        f.backend, f.bus, NULL, 0u, 0u, frame, &f, NULL));
    semu_nema_backend_ops.commit(f.backend);
    SEMU_TEST_EQ_U64(context, 0u, f.frames);
    finish(&f);
}
int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_refused_list_preserves_inherited_state),
        SEMU_TEST_CASE(test_syntax_refusal_has_diagnostic),
        SEMU_TEST_CASE(test_draw_refusal_retains_original_error),
        SEMU_TEST_CASE(test_batch_abort_commit_and_publication_order),
        SEMU_TEST_CASE(test_later_child_and_allocation_refusal),
        SEMU_TEST_CASE(test_shadow_abort_refusal_and_commit),
        SEMU_TEST_CASE(test_batch_bounds_and_empty)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
