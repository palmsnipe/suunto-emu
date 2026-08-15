#ifndef SEMU_NEMA_STATE_H
#define SEMU_NEMA_STATE_H

#include "../../src/display/nema_framing.h"
#include "semu/types.h"

/*
 * Nema register and draw state (ticket 502).
 * Converts framed register/value records into immutable draw-state
 * snapshots at evidenced DRAW_CMD boundaries.  No rasterization,
 * texture dereference, or pixel mutation.
 *
 * Evidence: E-NEMA-LISTS-001 (verified) — register set, inheritance,
 * preload/no-draw, and first-frame draw dispatch.
 */

/* Observed graphics register offsets (from SapporoNemaP.cs
 * ObservedGraphicsRegisters). */
#define NEMA_REG_TEX0_BASE     0x000u
#define NEMA_REG_TEX0_FSTRIDE  0x004u
#define NEMA_REG_TEX0_RESXY    0x008u
#define NEMA_REG_TEX1_BASE     0x010u
#define NEMA_REG_TEX1_FSTRIDE  0x014u
#define NEMA_REG_TEX1_RESXY    0x018u
#define NEMA_REG_TEX_COLOR     0x01Cu
#define NEMA_REG_IMEM_ADDR     0x0C4u
#define NEMA_REG_IMEM_DATAH    0x0C8u
#define NEMA_REG_IMEM_DATAL    0x0CCu
#define NEMA_REG_DRAW_CMD      0x100u
#define NEMA_REG_DRAW_START_XY 0x104u
#define NEMA_REG_DRAW_END_XY   0x108u
#define NEMA_REG_CLIPMIN       0x110u
#define NEMA_REG_CLIPMAX       0x114u
#define NEMA_REG_MATMULT       0x118u
#define NEMA_REG_CODEPTR       0x11Cu
#define NEMA_REG_POINT0_X      0x120u
#define NEMA_REG_POINT0_Y      0x124u
#define NEMA_REG_DRAW_COLOR    0x12Cu
#define NEMA_REG_POINT1_X      0x130u
#define NEMA_REG_POINT1_Y      0x134u
#define NEMA_REG_POINT2_X      0x140u
#define NEMA_REG_POINT2_Y      0x144u
#define NEMA_REG_POINT3_X      0x150u
#define NEMA_REG_POINT3_Y      0x154u
#define NEMA_REG_MM00          0x160u
#define NEMA_REG_MM01          0x164u
#define NEMA_REG_MM02          0x168u
#define NEMA_REG_MM10          0x16Cu
#define NEMA_REG_MM11          0x170u
#define NEMA_REG_MM12          0x174u
#define NEMA_REG_CONST0        0x200u
#define NEMA_REG_CONST1        0x204u

/* Evidenced draw command values. */
#define NEMA_DRAW_QUAD         0x00000005u
#define NEMA_DRAW_TRI_SOLID    0x00000004u
#define NEMA_DRAW_TRI_AA       0x40000004u

/* Texture format constants (from FSTRIDE high byte). */
#define NEMA_FMT_RGB565        0x04u
#define NEMA_FMT_A2LE           0x28u
#define NEMA_FMT_TSC6A         0x17u

typedef struct {
    /* Target surface (TEX0) */
    uint32_t target_base;
    uint32_t target_format;
    uint32_t target_sampling;
    uint32_t target_stride;
    uint32_t target_width;
    uint32_t target_height;

    /* Source texture (TEX1), if present */
    uint32_t src_base;
    uint32_t src_format;
    uint32_t src_sampling;
    uint32_t src_stride;
    uint32_t src_width;
    uint32_t src_height;
    uint32_t src_present;

    /* Clip rectangle */
    uint32_t clip_min_x, clip_min_y;
    uint32_t clip_max_x, clip_max_y;

    /* Quad geometry (16.16 fixed-point) */
    uint32_t point0_x, point0_y;
    uint32_t point1_x, point1_y;
    uint32_t point2_x, point2_y;
    uint32_t point3_x, point3_y;

    /* Colors and program */
    uint32_t draw_cmd;
    uint32_t draw_color;
    uint32_t tex_color;
    uint32_t matmult;
    uint32_t codeptr;

    /* Source list identity */
    uint32_t list_id;
} nema_draw_snapshot;

typedef struct nema_state nema_state;

typedef void (*nema_draw_fn)(void *context,
                              const nema_draw_snapshot *snapshot);

/*
 * Create/destroy state machine.  State persists across lists to model
 * inheritance.  Use nema_state_reset to clear all state.
 */
semu_status nema_state_create(nema_state **out, semu_error *error);
void nema_state_destroy(nema_state *state);
void nema_state_reset(nema_state *state);

/* Begin a new command list.  Does NOT clear inherited register state. */
void nema_state_begin_list(nema_state *state, uint32_t list_id);

/*
 * Process one framed register/value record.  On DRAW_CMD, validates
 * required state and invokes on_draw if complete.  Returns
 * SEMU_ERR_UNSUPPORTED for unknown registers or unsupported draw
 * commands.  Returns SEMU_OK for normal records and accepted draws.
 */
semu_status nema_state_record(nema_state *state, const nema_record *record,
                               nema_draw_fn on_draw, void *draw_context,
                               semu_error *error);

/* Number of snapshots emitted since creation or last reset. */
size_t nema_state_snapshot_count(const nema_state *state);

#endif
