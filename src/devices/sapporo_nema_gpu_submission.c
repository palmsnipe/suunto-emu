#include "sapporo_nema_gpu_internal.h"
#include "../display/nema_framing.h"
#include "../display/nema_backend.h"

/*
 * Ring-kick execution failures are GPU-side only (ticket 794,
 * E-EMU-SAP235-RINGKICK-CPU-INVISIBLE-001, citing lane census
 * c575c2dce1b1566405aadca096f8d04d43920394812eb72ba0b63b76ab2194cc and the
 * FINDINGS §5/§7 refusal ladder, both twice byte-identical): a guest store
 * of a refused ring-stop value leaves CFSR=HFSR=SHCSR=0, no IRQ pending,
 * and the core ADVANCES — the write commits at bus level while the GPU
 * renders nothing for the refused child and emits no completion.  The
 * backend signals exactly that encounter through the bounded draw-refusal
 * event; a REFUSE with no event keeps today's hard error contract (bad
 * framing, invalid list ranges, NULL, allocation, and parse-level refusals
 * are programming errors, not draw-state refusals).
 */

/* The event getter is backend-internal; the ops table identity check proves
 * backend_context is a semu_nema_backend whenever the machine registered
 * the known NEMA backend.  Foreign (test) backends keep the legacy
 * error-propagating contract. */
static const nema_draw_refusal *submission_draw_refusal(const semu_nema_gpu *g)
{
    if (g->backend.prepare == semu_nema_backend_ops.prepare &&
        g->backend.commit == semu_nema_backend_ops.commit &&
        g->backend.abort == semu_nema_backend_ops.abort)
        return semu_nema_backend_draw_refusal(
            (const semu_nema_backend *)g->backend_context);
    return NULL;
}

/* Deterministic named refusal line: no host pointers, only guest words.
 * Field names mirror the lane NEMA_DRAW_STATE row fields where the tree
 * snapshot carries them. */
static void log_draw_refused(const semu_nema_gpu *g,
    const nema_draw_refusal *r)
{
    const nema_draw_snapshot *s = &r->snapshot;
    if (g->logger == NULL) return;
    semu_log_write(g->logger, SEMU_LOG_WARNING, "gpu", "draw-refused",
        "ord=%u child=0x%08x offset=%u draw=0x%08x reason=\"%s\" "
        "target=0x%08x fmt=0x%02x wh=%ux%u "
        "src=%u/0x%08x fmt=0x%02x/%u/%u/%ux%u "
        "color=0x%08x drawcolor=0x%08x clipmin=%u,%u clipmax=%u,%u "
        "mat=%u code=0x%08x imem=%u,0x%08x,0x%08x "
        "p0=0x%08x,0x%08x p1=0x%08x,0x%08x p2=0x%08x,0x%08x p3=0x%08x,0x%08x",
        (unsigned)g->submission_ordinal, (unsigned)r->child_address,
        (unsigned)r->offset_bytes, (unsigned)s->draw_cmd, r->reason,
        (unsigned)s->target_base, (unsigned)(s->target_format & 0xffu),
        (unsigned)s->target_width, (unsigned)s->target_height,
        (unsigned)s->src_present, (unsigned)s->src_base,
        (unsigned)(s->src_format & 0xffu), (unsigned)s->src_sampling,
        (unsigned)s->src_stride, (unsigned)s->src_width,
        (unsigned)s->src_height, (unsigned)s->tex_color,
        (unsigned)s->draw_color, (unsigned)s->clip_min_x,
        (unsigned)s->clip_min_y, (unsigned)s->clip_max_x,
        (unsigned)s->clip_max_y, (unsigned)s->matrix_present,
        (unsigned)s->codeptr, (unsigned)s->imem_addr,
        (unsigned)s->imem_datah, (unsigned)s->imem_datal,
        (unsigned)s->point0_x, (unsigned)s->point0_y,
        (unsigned)s->point1_x, (unsigned)s->point1_y,
        (unsigned)s->point2_x, (unsigned)s->point2_y,
        (unsigned)s->point3_x, (unsigned)s->point3_y);
}

static semu_status submit_validated(semu_nema_gpu *g, uint32_t raw,
    uint32_t address, uint32_t words, uint32_t old, uint32_t end, semu_error *error)
{
    nema_ring_plan plan;
    semu_status status;
    semu_transaction_result result;
    int immediate = 0, prepared = 0;
    status = nema_framing_prepare(g->bus, address, words, old, end, &plan, error);
    if (status != SEMU_OK) return status;
    if (plan.child_count == 0u && plan.marker_count == 0u &&
        plan.list_count != 0u && !plan.quiet) {
        plan.lists[plan.list_count - 1u].flags = 0u;
        immediate = 1;
    }
    if (plan.marker_count != 0u && g->scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_STATE, "nema: completion scheduler missing");
        return SEMU_ERR_STATE;
    }
    if (plan.list_count != 0u) {
        semu_error_clear(error);
        result = g->backend.prepare(g->backend_context, g->bus, plan.lists, plan.list_count,
            0u, g->frame_callback, g->frame_context, error);
        if (result != SEMU_TRANSACTION_OK || error->code != SEMU_OK) {
            const nema_draw_refusal *refusal =
                result == SEMU_TRANSACTION_REFUSE ? submission_draw_refusal(g) : NULL;
            if (refusal != NULL) {
                /* GPU-side refusal: zero pixel/frame writes (the backend
                 * discarded the transaction), no completion scheduling and
                 * no completion IRQ (lane irq=0), but the store commits,
                 * the ring pointer advances, and one named line is logged;
                 * normal operation continues. */
                ++g->submission_ordinal;
                log_draw_refused(g, refusal);
                g->registers[NEMA_REG_CMDRINGSTOP / 4u] = raw;
                g->previous_ring_stop = raw & ~NEMA_RING_CTRL_MASK;
                semu_error_clear(error);
                return SEMU_OK;
            }
            if (result != SEMU_TRANSACTION_REFUSE) g->backend.abort(g->backend_context);
            if (error->code < SEMU_ERR_ARGUMENT || error->code > SEMU_ERR_NOMEM)
                semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                    "nema: backend refused or returned unsupported result %u", (unsigned)result);
            return error->code;
        }
        prepared = 1;
    }
    if (plan.marker_count != 0u) {
        status = nema_completion_schedule_batch(g->completion, g->scheduler, plan.markers, plan.marker_count,
            nema_gpu_completion_write, g, nema_gpu_completion_irq, g, error);
        if (status != SEMU_OK) {
            if (prepared) g->backend.abort(g->backend_context);
            return status;
        }
    }
    /* No fallible operation remains after atomic scheduler admission. */
    g->registers[NEMA_REG_CMDRINGSTOP / 4u] = raw;
    ++g->submission_ordinal;
    if (prepared) g->backend.commit(g->backend_context);
    g->previous_ring_stop = raw & ~NEMA_RING_CTRL_MASK;
    ++g->frame_generation;
    g->registers[NEMA_REG_FRAME_GEN / 4u] = g->frame_generation;
    if (immediate) {
        g->registers[NEMA_REG_INTERRUPT / 4u] = 1u;
        nema_gpu_completion_irq(g, NEMA_GPU_IRQ, 1);
    }
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status nema_gpu_submit(semu_nema_gpu *g, uint32_t raw, semu_error *error)
{
    semu_error local;
    uint32_t address = g->registers[NEMA_REG_CMDADDR / 4u];
    uint32_t size = g->registers[NEMA_REG_CMDSIZE / 4u];
    uint32_t stop = raw & ~NEMA_RING_CTRL_MASK;
    semu_status status;
    if (error == NULL) error = &local;
    if (!nema_gpu_ring_configured(g) || size % 4u != 0u || (address & 3u) != 0u ||
        g->previous_ring_stop < address || g->previous_ring_stop >= address + size ||
        stop < address || stop >= address + size) {
        semu_error_set(error, SEMU_ERR_RANGE, "nema: invalid active ring range");
        return SEMU_ERR_RANGE;
    }
    if (stop == g->previous_ring_stop) {
        g->registers[NEMA_REG_CMDRINGSTOP / 4u] = raw;
        ++g->submission_ordinal;
        semu_error_clear(error); return SEMU_OK;
    }
    if (g->backend.prepare == NULL || g->frame_generation == UINT32_MAX) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "nema: backend missing or generation exhausted");
        return SEMU_ERR_UNSUPPORTED;
    }
    g->submitting = 1;
    status = submit_validated(g, raw, address, size / 4u,
        (g->previous_ring_stop - address) / 4u, (stop - address) / 4u, error);
    g->submitting = 0;
    return status;
}
