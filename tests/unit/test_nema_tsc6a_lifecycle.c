#include "../../src/display/nema_backend_internal.h"
#include "../../src/display/nema_tsc6a_internal.h"
#include "semu/hash.h"
#include "test.h"
#include <stdlib.h>
#include <string.h>

#define BASE UINT32_C(0x10000000)
#define SURFACE (BASE + 4096u)
#define MISSING (BASE + 0x3ff00u)
typedef struct { semu_bus *bus; semu_nema_backend *gpu; unsigned frames; } fixture;
static void frame(void *context, const semu_frame *value)
{ (void)value; ++((fixture *)context)->frames; }
static int init(fixture *f)
{
    semu_error e;
    memset(f, 0, sizeof(*f));
    f->bus = semu_bus_create(&e); f->gpu = semu_nema_backend_create(&e);
    return f->bus && f->gpu &&
        semu_bus_map_ram(f->bus, "surface and commands", BASE, 0x40000u, &e) == SEMU_OK;
}
static void finish(fixture *f)
{ semu_nema_backend_destroy(f->gpu); semu_bus_destroy(f->bus); }
static semu_display_list words(fixture *f, uint32_t offset,
    const uint32_t *values, size_t count)
{
    size_t i; semu_error e;
    for (i = 0; i < count; ++i)
        (void)semu_bus_write(f->bus, BASE + offset + (uint32_t)i * 4u, 4u, values[i], &e);
    return (semu_display_list){BASE + offset, (uint32_t)count, 0u};
}
static semu_display_list stroke(fixture *f, uint32_t base)
{
    const uint32_t v[] = {
        NEMA_REG_TEX0_BASE,base,NEMA_REG_TEX0_FSTRIDE,TSC6A_TARGET_FSTRIDE,
        NEMA_REG_TEX0_RESXY,TSC6A_RESOLUTION,NEMA_REG_CLIPMIN,0u,NEMA_REG_CLIPMAX,0x10001u,
        NEMA_REG_POINT0_X,0u,NEMA_REG_POINT0_Y,0u,
        NEMA_REG_POINT1_X,0x10000u,NEMA_REG_POINT1_Y,0u,
        NEMA_REG_POINT2_X,0x10000u,NEMA_REG_POINT2_Y,0x10000u,
        NEMA_REG_MATMULT,TSC6A_OBSERVED_MATMULT,NEMA_REG_CODEPTR,TSC6A_OBSERVED_CODE,
        NEMA_REG_DRAW_COLOR,0xff55ff00u,NEMA_REG_DRAW_CMD,NEMA_DRAW_TRI_AA
    };
    return words(f, 0u, v, SEMU_ARRAY_LEN(v));
}
static semu_display_list resolve(fixture *f, uint32_t base)
{
    const uint32_t v[] = {
        NEMA_REG_TEX0_BASE,SURFACE,NEMA_REG_TEX0_FSTRIDE,TSC6A_RGB_FSTRIDE,
        NEMA_REG_TEX0_RESXY,TSC6A_RGB_RESOLUTION,NEMA_REG_TEX1_BASE,base,
        NEMA_REG_TEX1_FSTRIDE,TSC6A_TARGET_FSTRIDE_SAMPLED,NEMA_REG_TEX1_RESXY,TSC6A_RESOLUTION,
        NEMA_REG_MATMULT,0u,NEMA_REG_CODEPTR,TSC6A_RESOLVE_CODE,
        NEMA_REG_IMEM_ADDR,TSC6A_IMEM_ADDRESS,NEMA_REG_IMEM_DATAH,TSC6A_IMEM_DATAH,
        NEMA_REG_IMEM_DATAL,TSC6A_IMEM_DATAL,NEMA_REG_TEX_COLOR,0xff55ff00u,
        NEMA_REG_DRAW_COLOR,0xff55ff00u,NEMA_REG_CLIPMIN,0u,NEMA_REG_CLIPMAX,0x10001u,
        NEMA_REG_MM00,0x3f800000u,NEMA_REG_MM01,0u,NEMA_REG_MM02,0u,
        NEMA_REG_MM10,0u,NEMA_REG_MM11,0x3f800000u,NEMA_REG_MM12,0u,
        NEMA_REG_DRAW_CMD,NEMA_DRAW_TSC6A_RESOLVE
    };
    return words(f, 256u, v, SEMU_ARRAY_LEN(v));
}
static semu_transaction_result prepare(fixture *f, const semu_display_list *l,
    size_t count, semu_error *e)
{ return semu_nema_backend_ops.prepare(f->gpu, f->bus, l, count, 0u, frame, f, e); }
static semu_transaction_result submit(fixture *f, semu_display_list l, semu_error *e)
{
    semu_transaction_result r = prepare(f, &l, 1u, e);
    if (r == SEMU_TRANSACTION_OK) semu_nema_backend_ops.commit(f->gpu);
    return r;
}
static uint32_t shadow_crc(fixture *f)
{ return semu_crc32(0u, f->gpu->tsc6a->pixels, NEMA_TSC6A_PIXELS * sizeof(uint32_t)); }
static void red_block(fixture *f)
{
    semu_error e;
    const uint8_t block[12] = {0,0,0,0,0,0xf0,0,0xf0,0xff,7,0,0};
    (void)semu_bus_load(f->bus, SURFACE, block, sizeof(block), &e);
}
static void test_aborted_stroke_preserves_frame_start(semu_test_context *context)
{
    fixture f; semu_error e; semu_display_list l; uint32_t before;
    SEMU_TEST_ASSERT(context, init(&f));
    before = shadow_crc(&f); l = stroke(&f, SURFACE);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, prepare(&f, &l, 1u, &e));
    semu_nema_backend_ops.abort(f.gpu);
    SEMU_TEST_EQ_U64(context, 0u, f.gpu->shadow_fresh);
    SEMU_TEST_EQ_U64(context, before, shadow_crc(&f));
    SEMU_TEST_EQ_U64(context, 0u, f.frames);
    red_block(&f);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, l, &e));
    SEMU_TEST_EQ_U64(context, 1u, f.gpu->shadow_fresh);
    /* Pixel (3,3) is outside the stroke, but inside the rewritten block. */
    SEMU_TEST_EQ_U64(context, 0xffff0000u, f.gpu->tsc6a->pixels[3u * 480u + 3u]);
    finish(&f);
}
static void test_resolve_abort_and_later_child_refusal(semu_test_context *context)
{
    fixture f; semu_error e; semu_display_list l[2]; uint32_t before;
    const uint32_t bad[] = {0x55000004u,0u};
    SEMU_TEST_ASSERT(context, init(&f));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, stroke(&f, SURFACE), &e));
    before = shadow_crc(&f); l[0] = resolve(&f, SURFACE);
    l[1] = words(&f, 512u, bad, SEMU_ARRAY_LEN(bad));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, prepare(&f, l, 2u, &e));
    SEMU_TEST_EQ_U64(context, before, shadow_crc(&f));
    SEMU_TEST_EQ_U64(context, 1u, f.gpu->shadow_fresh);
    SEMU_TEST_EQ_U64(context, 1u, f.frames);
    SEMU_TEST_EQ_U64(context, 1u, nema_state_snapshot_count(f.gpu->state));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, prepare(&f, l, 1u, &e));
    semu_nema_backend_ops.abort(f.gpu);
    SEMU_TEST_EQ_U64(context, before, shadow_crc(&f));
    SEMU_TEST_EQ_U64(context, 1u, f.gpu->shadow_fresh);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, l[0], &e));
    SEMU_TEST_EQ_U64(context, 0u, f.gpu->shadow_fresh);
    SEMU_TEST_EQ_U64(context, 0u, f.gpu->tsc6a->pixels[0]);
    SEMU_TEST_EQ_U64(context, 2u, f.frames);
    SEMU_TEST_ASSERT(context, semu_nema_backend_frame(f.gpu)->pixels[0] != 0u);
    finish(&f);
}
static void test_unmapped_baseline_is_a_transaction_refusal(semu_test_context *context)
{
    fixture f; semu_error e; uint32_t before;
    SEMU_TEST_ASSERT(context, init(&f));
    before = shadow_crc(&f);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, submit(&f, stroke(&f, MISSING), &e));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, e.code);
    SEMU_TEST_ASSERT(context, strstr(e.text, "mapped memory") != NULL);
    SEMU_TEST_EQ_U64(context, before, shadow_crc(&f));
    SEMU_TEST_EQ_U64(context, 0u, f.frames);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, stroke(&f, SURFACE), &e));
    before = shadow_crc(&f);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, submit(&f, resolve(&f, MISSING), &e));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, e.code);
    SEMU_TEST_EQ_U64(context, before, shadow_crc(&f));
    SEMU_TEST_EQ_U64(context, 1u, f.gpu->shadow_fresh);
    SEMU_TEST_EQ_U64(context, 1u, f.frames);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, resolve(&f, SURFACE), &e));
    finish(&f);
}
static void test_snapshot_load_discards_previous_cache(semu_test_context *context)
{
    fixture f; semu_error e; uint8_t *data = NULL; size_t size = 0u;
    SEMU_TEST_ASSERT(context, init(&f));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_backend_snapshot_ops.save(f.gpu, &data, &size, &e));
    red_block(&f);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, stroke(&f, SURFACE), &e));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT, semu_nema_backend_snapshot_ops.load(f.gpu, data, size - 1u, &e));
    SEMU_TEST_EQ_U64(context, 1u, f.gpu->baseline_valid);
    SEMU_TEST_EQ_U64(context, 1u, f.gpu->shadow_fresh);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_backend_snapshot_ops.load(f.gpu, data, size, &e));
    SEMU_TEST_EQ_U64(context, 0u, f.gpu->baseline_valid);
    SEMU_TEST_EQ_U64(context, 0u, f.gpu->shadow_fresh);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, stroke(&f, SURFACE), &e));
    SEMU_TEST_EQ_U64(context, 0xffff0000u, f.gpu->tsc6a->pixels[3u * 480u + 3u]);
    free(data); finish(&f);
}
static void test_baseline_cache_rolls_back_with_submission(semu_test_context *context)
{
    fixture f; semu_error e; semu_display_list l[2];
    const uint32_t bad[] = {0x55000004u,0u};
    uint8_t *before; uint32_t baseline_crc; size_t pass;
    SEMU_TEST_ASSERT(context, init(&f)); red_block(&f);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, resolve(&f, SURFACE), &e));
    before = malloc(NEMA_TSC6A_SPAN_BYTES);
    SEMU_TEST_ASSERT(context, before != NULL);
    memcpy(before, f.gpu->guest_span, NEMA_TSC6A_SPAN_BYTES);
    baseline_crc = semu_crc32(0u, f.gpu->baseline->pixels, NEMA_TSC6A_PIXELS * 4u);
    /* A valid shifted span changes both the base and decoded baseline. */
    l[0] = resolve(&f, SURFACE + 12u); l[1] = words(&f, 512u, bad, SEMU_ARRAY_LEN(bad));
    for (pass = 0u; pass < 2u; ++pass) {
        if (pass == 0u) {
            SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, prepare(&f, l, 1u, &e));
            semu_nema_backend_ops.abort(f.gpu);
        } else {
            SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, prepare(&f, l, 2u, &e));
        }
        SEMU_TEST_EQ_U64(context, 1u, f.gpu->baseline_valid);
        SEMU_TEST_EQ_U64(context, SURFACE, f.gpu->guest_span_base);
        SEMU_TEST_EQ_U64(context, baseline_crc,
            semu_crc32(0u, f.gpu->baseline->pixels, NEMA_TSC6A_PIXELS * 4u));
        SEMU_TEST_ASSERT(context, memcmp(before, f.gpu->guest_span, NEMA_TSC6A_SPAN_BYTES) == 0);
        SEMU_TEST_EQ_U64(context, 1u, f.frames);
    }
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, l[0], &e));
    SEMU_TEST_EQ_U64(context, SURFACE + 12u, f.gpu->guest_span_base);
    SEMU_TEST_EQ_U64(context, 0u, f.gpu->baseline->pixels[0]);
    free(before); finish(&f);
}
static void test_snapshot_preserves_cached_history(semu_test_context *context)
{
    fixture f, restored; semu_error e; uint8_t *data=NULL, *again=NULL;
    size_t size=0u, n=0u; uint32_t expected;
    SEMU_TEST_ASSERT(context, init(&f) && init(&restored));
    red_block(&f); red_block(&restored);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, resolve(&f, SURFACE), &e));
    /* The current model retains a previously decoded block on an auxiliary
     * rewrite. Persistence must retain that history without inventing a codec. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(f.bus, SURFACE+9u, 1u, 15u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(restored.bus, SURFACE+9u, 1u, 15u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, resolve(&f, SURFACE), &e));
    SEMU_TEST_EQ_U64(context, 0xffff0000u, f.gpu->baseline->pixels[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_backend_snapshot_ops.save(f.gpu, &data, &size, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_backend_snapshot_ops.load(restored.gpu, data, size, &e));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&f, resolve(&f, SURFACE), &e));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, submit(&restored, resolve(&restored, SURFACE), &e));
    expected=shadow_crc(&f);
    SEMU_TEST_EQ_U64(context, expected, shadow_crc(&restored));
    SEMU_TEST_EQ_U64(context, semu_nema_backend_frame(f.gpu)->generation,
        semu_nema_backend_frame(restored.gpu)->generation);
    free(data); data=NULL;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_backend_snapshot_ops.save(f.gpu, &data, &size, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_backend_snapshot_ops.save(restored.gpu, &again, &n, &e));
    SEMU_TEST_ASSERT(context, size==n && memcmp(data,again,n)==0);
    free(data);free(again);finish(&f);finish(&restored);
}
int main(void)
{
    const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_aborted_stroke_preserves_frame_start),
        SEMU_TEST_CASE(test_resolve_abort_and_later_child_refusal),
        SEMU_TEST_CASE(test_unmapped_baseline_is_a_transaction_refusal),
        SEMU_TEST_CASE(test_snapshot_load_discards_previous_cache),
        SEMU_TEST_CASE(test_baseline_cache_rolls_back_with_submission),
        SEMU_TEST_CASE(test_snapshot_preserves_cached_history)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
