#include "../../src/display/nema_state.h"
#include "test.h"

#include <string.h>

typedef struct {
    nema_draw_snapshot snapshots[16u];
    size_t count;
} snap_ctx;

static void on_draw(void *context, const nema_draw_snapshot *snapshot)
{
    snap_ctx *ctx = (snap_ctx *)context;
    if (ctx->count < 16u) {
        ctx->snapshots[ctx->count] = *snapshot;
    }
    ++ctx->count;
}

static semu_status emit(nema_state *st, uint32_t reg, uint32_t value,
                        semu_error *err)
{
    nema_record r;
    r.prefix = 0u;
    r.reg_offset = reg;
    r.value = value;
    r.source_addr = 0u;
    return nema_state_record(st, &r, NULL, NULL, err);
}

static semu_status emit_draw(nema_state *st, uint32_t reg, uint32_t value,
                              nema_draw_fn fn, void *ctx, semu_error *err)
{
    nema_record r;
    r.prefix = 0u;
    r.reg_offset = reg;
    r.value = value;
    r.source_addr = 0u;
    return nema_state_record(st, &r, fn, ctx, err);
}

#define FSTRIDE_RGB565_240  0x040001E0u
#define RESXY_240x240       0x00F000F0u
#define FSTRIDE_A2LE_88     0x28000016u
#define RESXY_88x88         0x00580058u

static void set_target_clip_quad(nema_state *st, semu_error *err)
{
    emit(st, NEMA_REG_TEX0_BASE, 0x10000000u, err);
    emit(st, NEMA_REG_TEX0_FSTRIDE, FSTRIDE_RGB565_240, err);
    emit(st, NEMA_REG_TEX0_RESXY, RESXY_240x240, err);
    emit(st, NEMA_REG_CLIPMIN, 0x00000000u, err);
    emit(st, NEMA_REG_CLIPMAX, 0x007800F0u, err);
    emit(st, NEMA_REG_POINT0_X, 0u, err);
    emit(st, NEMA_REG_POINT0_Y, 0u, err);
    emit(st, NEMA_REG_POINT1_X, 0x00F00000u, err);
    emit(st, NEMA_REG_POINT1_Y, 0u, err);
    emit(st, NEMA_REG_POINT2_X, 0x00F00000u, err);
    emit(st, NEMA_REG_POINT2_Y, 0x00780000u, err);
    emit(st, NEMA_REG_POINT3_X, 0u, err);
    emit(st, NEMA_REG_POINT3_Y, 0x00780000u, err);
}

static void test_preload_no_draw(semu_test_context *context)
{
    semu_error err;
    nema_state *st = NULL;
    semu_status s;

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_state_create(&st, &err) == SEMU_OK);
    nema_state_begin_list(st, 3u);
    emit(st, NEMA_REG_TEX0_BASE, 0x10000000u, &err);
    emit(st, NEMA_REG_CLIPMIN, 0u, &err);
    s = emit(st, NEMA_REG_CONST0, 42u, &err);
    SEMU_TEST_EQ_U64(context, SEMU_OK, s);
    SEMU_TEST_EQ_U64(context, 0u, nema_state_snapshot_count(st));
    nema_state_destroy(st);
}

static void test_clear_draw(semu_test_context *context)
{
    semu_error err;
    nema_state *st = NULL;
    snap_ctx cap = {0};
    semu_status s;

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_state_create(&st, &err) == SEMU_OK);
    nema_state_begin_list(st, 4u);
    set_target_clip_quad(st, &err);
    emit(st, NEMA_REG_DRAW_COLOR, 0u, &err);
    s = emit_draw(st, NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD, on_draw, &cap, &err);
    SEMU_TEST_EQ_U64(context, SEMU_OK, s);
    SEMU_TEST_EQ_U64(context, 1u, cap.count);
    SEMU_TEST_EQ_U64(context, 0x04u, cap.snapshots[0u].target_format);
    SEMU_TEST_EQ_U64(context, 480u, cap.snapshots[0u].target_stride);
    SEMU_TEST_EQ_U64(context, 240u, cap.snapshots[0u].target_width);
    SEMU_TEST_EQ_U64(context, 0u, cap.snapshots[0u].src_present);
    SEMU_TEST_EQ_U64(context, 4u, cap.snapshots[0u].list_id);
    nema_state_destroy(st);
}

static void test_inherited_a2le(semu_test_context *context)
{
    semu_error err;
    nema_state *st = NULL;
    snap_ctx cap = {0};
    semu_status s;

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_state_create(&st, &err) == SEMU_OK);
    nema_state_begin_list(st, 1u);
    set_target_clip_quad(st, &err);
    emit(st, NEMA_REG_TEX1_BASE, 0x10098EF0u, &err);
    emit(st, NEMA_REG_TEX1_FSTRIDE, FSTRIDE_A2LE_88, &err);
    emit(st, NEMA_REG_TEX1_RESXY, RESXY_88x88, &err);
    emit(st, NEMA_REG_TEX_COLOR, 0xFFFFFFFFu, &err);
    emit(st, NEMA_REG_MM00, 0x3F800000u, &err);
    emit(st, NEMA_REG_MM01, 0u, &err);
    emit(st, NEMA_REG_MM02, 0xC2680000u, &err);
    emit(st, NEMA_REG_MM10, 0u, &err);
    emit(st, NEMA_REG_MM11, 0x3F800000u, &err);
    emit(st, NEMA_REG_MM12, 0xC2C80000u, &err);
    nema_state_begin_list(st, 2u);
    s = emit_draw(st, NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD, on_draw, &cap, &err);
    SEMU_TEST_EQ_U64(context, SEMU_OK, s);
    SEMU_TEST_EQ_U64(context, 1u, cap.count);
    SEMU_TEST_EQ_U64(context, 1u, cap.snapshots[0u].src_present);
    SEMU_TEST_EQ_U64(context, 0x28u, cap.snapshots[0u].src_format);
    SEMU_TEST_EQ_U64(context, 88u, cap.snapshots[0u].src_width);
    SEMU_TEST_EQ_U64(context, 1u, cap.snapshots[0u].matrix_present);
    SEMU_TEST_EQ_U64(context, 0xC2680000u, cap.snapshots[0u].mm02);
    SEMU_TEST_EQ_U64(context, 2u, cap.snapshots[0u].list_id);
    nema_state_destroy(st);
}

static void test_multiple_draws(semu_test_context *context)
{
    semu_error err;
    nema_state *st = NULL;
    snap_ctx cap = {0};

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_state_create(&st, &err) == SEMU_OK);
    nema_state_begin_list(st, 5u);
    set_target_clip_quad(st, &err);
    emit(st, NEMA_REG_DRAW_COLOR, 0u, &err);
    emit_draw(st, NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD, on_draw, &cap, &err);
    emit(st, NEMA_REG_DRAW_COLOR, 0xFFFFFFFFu, &err);
    emit_draw(st, NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD, on_draw, &cap, &err);
    SEMU_TEST_EQ_U64(context, 2u, cap.count);
    SEMU_TEST_EQ_U64(context, 2u, nema_state_snapshot_count(st));
    nema_state_destroy(st);
}

static void test_missing_target(semu_test_context *context)
{
    semu_error err;
    nema_state *st = NULL;
    semu_status s;

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_state_create(&st, &err) == SEMU_OK);
    nema_state_begin_list(st, 1u);
    emit(st, NEMA_REG_CLIPMIN, 0u, &err);
    emit(st, NEMA_REG_CLIPMAX, 0u, &err);
    s = emit(st, NEMA_REG_DRAW_CMD, NEMA_DRAW_TRI_SOLID, &err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, s);
    SEMU_TEST_EQ_U64(context, 0u, nema_state_snapshot_count(st));
    nema_state_destroy(st);
}

static void test_missing_geometry(semu_test_context *context)
{
    semu_error err;
    nema_state *st = NULL;
    semu_status s;

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_state_create(&st, &err) == SEMU_OK);
    nema_state_begin_list(st, 1u);
    emit(st, NEMA_REG_TEX0_BASE, 0x10000000u, &err);
    emit(st, NEMA_REG_TEX0_FSTRIDE, FSTRIDE_RGB565_240, &err);
    emit(st, NEMA_REG_TEX0_RESXY, RESXY_240x240, &err);
    emit(st, NEMA_REG_CLIPMIN, 0u, &err);
    emit(st, NEMA_REG_CLIPMAX, 0u, &err);
    s = emit(st, NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD, &err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, s);
    nema_state_destroy(st);
}

static void test_unknown_register(semu_test_context *context)
{
    semu_error err;
    nema_state *st = NULL;
    semu_status s;

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_state_create(&st, &err) == SEMU_OK);
    nema_state_begin_list(st, 1u);
    s = emit(st, 0x0B0u, 1u, &err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, s);
    nema_state_destroy(st);
}

static void test_unsupported_draw(semu_test_context *context)
{
    semu_error err;
    nema_state *st = NULL;
    semu_status s;

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_state_create(&st, &err) == SEMU_OK);
    nema_state_begin_list(st, 1u);
    set_target_clip_quad(st, &err);
    s = emit(st, NEMA_REG_DRAW_CMD, 2u, &err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, s);
    SEMU_TEST_EQ_U64(context, 0u, nema_state_snapshot_count(st));
    nema_state_destroy(st);
}

static void test_reset(semu_test_context *context)
{
    semu_error err;
    nema_state *st = NULL;
    semu_status s;

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_state_create(&st, &err) == SEMU_OK);
    nema_state_begin_list(st, 1u);
    set_target_clip_quad(st, &err);
    nema_state_reset(st);
    nema_state_begin_list(st, 2u);
    emit(st, NEMA_REG_CLIPMIN, 0u, &err);
    emit(st, NEMA_REG_CLIPMAX, 0u, &err);
    s = emit(st, NEMA_REG_DRAW_CMD, NEMA_DRAW_TRI_SOLID, &err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, s);
    SEMU_TEST_EQ_U64(context, 0u, nema_state_snapshot_count(st));
    nema_state_destroy(st);
}

static void test_two_run_equality(semu_test_context *context)
{
    semu_error err;
    nema_state *st1 = NULL, *st2 = NULL;
    snap_ctx cap1 = {0}, cap2 = {0};
    size_t i;

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_state_create(&st1, &err) == SEMU_OK);
    SEMU_TEST_ASSERT(context, nema_state_create(&st2, &err) == SEMU_OK);

    nema_state_begin_list(st1, 1u);
    set_target_clip_quad(st1, &err);
    emit(st1, NEMA_REG_DRAW_COLOR, 0u, &err);
    emit_draw(st1, NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD, on_draw, &cap1, &err);

    nema_state_begin_list(st2, 1u);
    set_target_clip_quad(st2, &err);
    emit(st2, NEMA_REG_DRAW_COLOR, 0u, &err);
    emit_draw(st2, NEMA_REG_DRAW_CMD, NEMA_DRAW_QUAD, on_draw, &cap2, &err);

    SEMU_TEST_EQ_U64(context, cap1.count, cap2.count);
    for (i = 0u; i < cap1.count; ++i) {
        SEMU_TEST_EQ_U64(context, cap1.snapshots[i].target_base,
                         cap2.snapshots[i].target_base);
        SEMU_TEST_EQ_U64(context, cap1.snapshots[i].target_format,
                         cap2.snapshots[i].target_format);
        SEMU_TEST_EQ_U64(context, cap1.snapshots[i].draw_cmd,
                         cap2.snapshots[i].draw_cmd);
        SEMU_TEST_EQ_U64(context, cap1.snapshots[i].clip_max_x,
                         cap2.snapshots[i].clip_max_x);
    }
    nema_state_destroy(st1);
    nema_state_destroy(st2);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_preload_no_draw),
        SEMU_TEST_CASE(test_clear_draw),
        SEMU_TEST_CASE(test_inherited_a2le),
        SEMU_TEST_CASE(test_multiple_draws),
        SEMU_TEST_CASE(test_missing_target),
        SEMU_TEST_CASE(test_missing_geometry),
        SEMU_TEST_CASE(test_unknown_register),
        SEMU_TEST_CASE(test_unsupported_draw),
        SEMU_TEST_CASE(test_reset),
        SEMU_TEST_CASE(test_two_run_equality)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
