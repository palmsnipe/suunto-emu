/*
 * Nema register and draw state (ticket 502).
 * Converts framed register/value records into immutable draw-state
 * snapshots at evidenced DRAW_CMD boundaries.
 */

#include "nema_state.h"

#include <stdlib.h>
#include <string.h>

/* Presence-bit indices for the 34 observed registers. */
#define P_TEX0_BASE     0u
#define P_TEX0_FSTRIDE  1u
#define P_TEX0_RESXY    2u
#define P_TEX1_BASE     3u
#define P_TEX1_FSTRIDE  4u
#define P_TEX1_RESXY    5u
#define P_TEX_COLOR     6u
#define P_IMEM_ADDR     7u
#define P_IMEM_DATAH    8u
#define P_IMEM_DATAL    9u
#define P_DRAW_CMD      10u
#define P_DRAW_START_XY 11u
#define P_DRAW_END_XY   12u
#define P_CLIPMIN       13u
#define P_CLIPMAX       14u
#define P_MATMULT       15u
#define P_CODEPTR       16u
#define P_POINT0_X      17u
#define P_POINT0_Y      18u
#define P_DRAW_COLOR    19u
#define P_POINT1_X      20u
#define P_POINT1_Y      21u
#define P_POINT2_X      22u
#define P_POINT2_Y      23u
#define P_POINT3_X      24u
#define P_POINT3_Y      25u
#define P_MM00          26u
#define P_MM01          27u
#define P_MM02          28u
#define P_MM10          29u
#define P_MM11          30u
#define P_MM12          31u
#define P_CONST0        32u
#define P_CONST1        33u

#define REG_COUNT 34u
#define BIT(n) (1ULL << (n))

struct nema_state {
    uint32_t values[REG_COUNT];
    uint64_t presence;
    uint32_t list_id;
    size_t snapshot_count;
};

static int map_register(uint32_t offset, size_t *idx)
{
    switch (offset) {
    case NEMA_REG_TEX0_BASE:     *idx = P_TEX0_BASE;     return 1;
    case NEMA_REG_TEX0_FSTRIDE:  *idx = P_TEX0_FSTRIDE;  return 1;
    case NEMA_REG_TEX0_RESXY:    *idx = P_TEX0_RESXY;    return 1;
    case NEMA_REG_TEX1_BASE:     *idx = P_TEX1_BASE;     return 1;
    case NEMA_REG_TEX1_FSTRIDE:  *idx = P_TEX1_FSTRIDE;  return 1;
    case NEMA_REG_TEX1_RESXY:    *idx = P_TEX1_RESXY;    return 1;
    case NEMA_REG_TEX_COLOR:     *idx = P_TEX_COLOR;     return 1;
    case NEMA_REG_IMEM_ADDR:     *idx = P_IMEM_ADDR;     return 1;
    case NEMA_REG_IMEM_DATAH:    *idx = P_IMEM_DATAH;    return 1;
    case NEMA_REG_IMEM_DATAL:    *idx = P_IMEM_DATAL;    return 1;
    case NEMA_REG_DRAW_CMD:      *idx = P_DRAW_CMD;      return 1;
    case NEMA_REG_DRAW_START_XY: *idx = P_DRAW_START_XY; return 1;
    case NEMA_REG_DRAW_END_XY:   *idx = P_DRAW_END_XY;   return 1;
    case NEMA_REG_CLIPMIN:       *idx = P_CLIPMIN;       return 1;
    case NEMA_REG_CLIPMAX:       *idx = P_CLIPMAX;       return 1;
    case NEMA_REG_MATMULT:       *idx = P_MATMULT;       return 1;
    case NEMA_REG_CODEPTR:       *idx = P_CODEPTR;       return 1;
    case NEMA_REG_POINT0_X:      *idx = P_POINT0_X;      return 1;
    case NEMA_REG_POINT0_Y:      *idx = P_POINT0_Y;      return 1;
    case NEMA_REG_DRAW_COLOR:    *idx = P_DRAW_COLOR;    return 1;
    case NEMA_REG_POINT1_X:      *idx = P_POINT1_X;      return 1;
    case NEMA_REG_POINT1_Y:      *idx = P_POINT1_Y;      return 1;
    case NEMA_REG_POINT2_X:      *idx = P_POINT2_X;      return 1;
    case NEMA_REG_POINT2_Y:      *idx = P_POINT2_Y;      return 1;
    case NEMA_REG_POINT3_X:      *idx = P_POINT3_X;      return 1;
    case NEMA_REG_POINT3_Y:      *idx = P_POINT3_Y;      return 1;
    case NEMA_REG_MM00:          *idx = P_MM00;          return 1;
    case NEMA_REG_MM01:          *idx = P_MM01;          return 1;
    case NEMA_REG_MM02:          *idx = P_MM02;          return 1;
    case NEMA_REG_MM10:          *idx = P_MM10;          return 1;
    case NEMA_REG_MM11:          *idx = P_MM11;          return 1;
    case NEMA_REG_MM12:          *idx = P_MM12;          return 1;
    case NEMA_REG_CONST0:        *idx = P_CONST0;        return 1;
    case NEMA_REG_CONST1:        *idx = P_CONST1;        return 1;
    default: return 0;
    }
}

static int is_supported_draw_cmd(uint32_t cmd)
{
    return cmd == NEMA_DRAW_QUAD ||
           cmd == NEMA_DRAW_TRI_SOLID ||
           cmd == NEMA_DRAW_TRI_AA ||
           cmd == NEMA_DRAW_TSC6A_RESOLVE;
}

static void build_snapshot(const nema_state *st, nema_draw_snapshot *s)
{
    uint32_t fstride, resxy;
    memset(s, 0, sizeof(*s));
    s->list_id = st->list_id;

    s->target_base = st->values[P_TEX0_BASE];
    fstride = st->values[P_TEX0_FSTRIDE];
    s->target_format = (fstride >> 24) & 0xFFu;
    s->target_sampling = (fstride >> 16) & 0xFFu;
    s->target_stride = fstride & 0xFFFFu;
    resxy = st->values[P_TEX0_RESXY];
    s->target_width = resxy & 0xFFFFu;
    s->target_height = (resxy >> 16) & 0xFFFFu;

    if (st->presence & BIT(P_TEX1_BASE)) {
        s->src_present = 1u;
        s->src_base = st->values[P_TEX1_BASE];
        fstride = st->values[P_TEX1_FSTRIDE];
        s->src_format = (fstride >> 24) & 0xFFu;
        s->src_sampling = (fstride >> 16) & 0xFFu;
        s->src_stride = fstride & 0xFFFFu;
        resxy = st->values[P_TEX1_RESXY];
        s->src_width = resxy & 0xFFFFu;
        s->src_height = (resxy >> 16) & 0xFFFFu;
    }

    s->clip_min_x = st->values[P_CLIPMIN] & 0xFFFFu;
    s->clip_min_y = (st->values[P_CLIPMIN] >> 16) & 0xFFFFu;
    s->clip_max_x = st->values[P_CLIPMAX] & 0xFFFFu;
    s->clip_max_y = (st->values[P_CLIPMAX] >> 16) & 0xFFFFu;

    s->point0_x = st->values[P_POINT0_X];
    s->point0_y = st->values[P_POINT0_Y];
    s->point1_x = st->values[P_POINT1_X];
    s->point1_y = st->values[P_POINT1_Y];
    s->point2_x = st->values[P_POINT2_X];
    s->point2_y = st->values[P_POINT2_Y];
    s->point3_x = st->values[P_POINT3_X];
    s->point3_y = st->values[P_POINT3_Y];

    s->draw_cmd = st->values[P_DRAW_CMD];
    s->draw_color = st->values[P_DRAW_COLOR];
    s->tex_color = st->values[P_TEX_COLOR];
    s->matmult = st->values[P_MATMULT];
    s->codeptr = st->values[P_CODEPTR];
    s->imem_addr = st->values[P_IMEM_ADDR];
    s->imem_datah = st->values[P_IMEM_DATAH];
    s->imem_datal = st->values[P_IMEM_DATAL];
    s->matrix_present = ((st->presence &
                          (BIT(P_MM00) | BIT(P_MM01) | BIT(P_MM02) |
                           BIT(P_MM10) | BIT(P_MM11) | BIT(P_MM12))) ==
                         (BIT(P_MM00) | BIT(P_MM01) | BIT(P_MM02) |
                          BIT(P_MM10) | BIT(P_MM11) | BIT(P_MM12))) ? 1u : 0u;
    s->mm00 = st->values[P_MM00];
    s->mm01 = st->values[P_MM01];
    s->mm02 = st->values[P_MM02];
    s->mm10 = st->values[P_MM10];
    s->mm11 = st->values[P_MM11];
    s->mm12 = st->values[P_MM12];
}

static int validate_draw(const nema_state *st, semu_error *error)
{
    uint64_t req = BIT(P_TEX0_BASE) | BIT(P_TEX0_FSTRIDE) |
                   BIT(P_TEX0_RESXY) | BIT(P_CLIPMIN) | BIT(P_CLIPMAX);
    uint32_t cmd = st->values[P_DRAW_CMD];

    if (!is_supported_draw_cmd(cmd)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "nema_state: unsupported draw cmd 0x%08x", cmd);
        return 0;
    }
    if ((st->presence & req) != req) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "nema_state: missing required target/clip state");
        return 0;
    }
    /* Textured draws require TEX1 + TEX_COLOR. */
    if (st->presence & BIT(P_TEX1_BASE)) {
        uint64_t tex_req = BIT(P_TEX1_BASE) | BIT(P_TEX1_FSTRIDE) |
                            BIT(P_TEX1_RESXY) | BIT(P_TEX_COLOR);
        if ((st->presence & tex_req) != tex_req) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "nema_state: incomplete source texture state");
            return 0;
        }
    }
    /* Quad draws require all four points. */
    if (cmd == NEMA_DRAW_QUAD) {
        uint64_t geo = BIT(P_POINT0_X) | BIT(P_POINT0_Y) |
                       BIT(P_POINT1_X) | BIT(P_POINT1_Y) |
                       BIT(P_POINT2_X) | BIT(P_POINT2_Y) |
                       BIT(P_POINT3_X) | BIT(P_POINT3_Y);
        if ((st->presence & geo) != geo) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "nema_state: missing quad geometry");
            return 0;
        }
    }
    if (cmd == NEMA_DRAW_TRI_SOLID || cmd == NEMA_DRAW_TRI_AA) {
        uint64_t geo = BIT(P_POINT0_X) | BIT(P_POINT0_Y) |
                       BIT(P_POINT1_X) | BIT(P_POINT1_Y) |
                       BIT(P_POINT2_X) | BIT(P_POINT2_Y);
        if ((st->presence & geo) != geo) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "nema_state: missing triangle geometry");
            return 0;
        }
    }
    if (cmd == NEMA_DRAW_TSC6A_RESOLVE) {
        uint64_t resolve_req = BIT(P_TEX1_BASE) | BIT(P_TEX1_FSTRIDE) |
                               BIT(P_TEX1_RESXY) | BIT(P_TEX_COLOR) |
                               BIT(P_DRAW_COLOR) | BIT(P_IMEM_ADDR) |
                               BIT(P_IMEM_DATAH) | BIT(P_IMEM_DATAL) |
                               BIT(P_MM00) | BIT(P_MM01) | BIT(P_MM02) |
                               BIT(P_MM10) | BIT(P_MM11) | BIT(P_MM12);
        if ((st->presence & resolve_req) != resolve_req) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "nema_state: incomplete TSC6A resolve state");
            return 0;
        }
    }
    return 1;
}

semu_status nema_state_create(nema_state **out, semu_error *error)
{
    nema_state *st;
    (void)error;
    st = (nema_state *)calloc(1, sizeof(*st));
    if (st == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "nema_state: alloc failed");
        return SEMU_ERR_NOMEM;
    }
    *out = st;
    return SEMU_OK;
}

void nema_state_destroy(nema_state *st)
{
    free(st);
}

void nema_state_reset(nema_state *st)
{
    if (st == NULL) return;
    memset(st->values, 0, sizeof(st->values));
    st->presence = 0u;
    st->list_id = 0u;
    st->snapshot_count = 0u;
}

void nema_state_begin_list(nema_state *st, uint32_t list_id)
{
    if (st == NULL) return;
    st->list_id = list_id;
}

semu_status nema_state_record(nema_state *st, const nema_record *rec,
                               nema_draw_fn on_draw, void *ctx,
                               semu_error *error)
{
    size_t idx;

    if (st == NULL || rec == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "nema_state: null argument");
        return SEMU_ERR_ARGUMENT;
    }
    if (!map_register(rec->reg_offset, &idx)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "nema_state: unknown register 0x%06x",
                       rec->reg_offset);
        return SEMU_ERR_UNSUPPORTED;
    }

    st->values[idx] = rec->value;
    st->presence |= BIT(idx);

    if (idx == P_DRAW_CMD) {
        if (!validate_draw(st, error)) {
            return SEMU_ERR_UNSUPPORTED;
        }
        ++st->snapshot_count;
        if (on_draw != NULL) {
            nema_draw_snapshot snap;
            build_snapshot(st, &snap);
            on_draw(ctx, &snap);
        }
    }
    return SEMU_OK;
}

size_t nema_state_snapshot_count(const nema_state *st)
{
    return (st != NULL) ? st->snapshot_count : 0u;
}
