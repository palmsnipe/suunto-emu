#include "sapporo_nema_gpu_internal.h"
#include "../display/nema_framing.h"

typedef struct {
    semu_display_list lists[SEMU_DISPLAY_MAX_LISTS];
    size_t count;
} submission;

static void collect_child(void *context, uint32_t address, uint32_t words)
{
    submission *s = context;
    /* The framing parser has already validated its 32-child bound. */
    if (s->count < SEMU_DISPLAY_MAX_LISTS)
        s->lists[s->count++] = (semu_display_list){address, words};
}

static semu_status markers(semu_nema_gpu *g, uint32_t base, uint32_t words,
    uint32_t old, uint32_t end, uint32_t *ids, size_t *count, semu_error *error)
{
    uint32_t i, submitted = (end + words - old) % words;
    *count = 0u;
    for (i = 0u; i < submitted; ++i) {
        uint32_t word;
        semu_status status = semu_bus_read(g->bus,
            base + ((old + i) % words) * 4u, 4u, &word, error);
        if (status != SEMU_OK) return status;
        if (word == NEMA_REG_CMDADDR || word == (NEMA_HOLDCMD | NEMA_REG_CMDADDR)) {
            i += 3u; /* Framing already validated these descriptors. */
        } else if (word == NEMA_REG_CLID) {
            uint32_t marker[3], j;
            if (submitted - i < 4u || *count == NEMA_COMPLETION_MAX_EVENTS) {
                semu_error_set(error, SEMU_ERR_UNSUPPORTED, "nema: invalid completion marker count");
                return SEMU_ERR_UNSUPPORTED;
            }
            for (j = 0u; j < 3u; ++j) {
                status = semu_bus_read(g->bus, base + ((old + i + j + 1u) % words) * 4u,
                    4u, &marker[j], error);
                if (status != SEMU_OK) return status;
            }
            if (marker[1] != NEMA_REG_INTERRUPT || marker[2] != 1u) {
                semu_error_set(error, SEMU_ERR_UNSUPPORTED, "nema: unsupported completion marker");
                return SEMU_ERR_UNSUPPORTED;
            }
            ids[(*count)++] = marker[0]; i += 3u;
        }
    }
    return SEMU_OK;
}

static semu_status submit_validated(semu_nema_gpu *g, uint32_t raw,
    uint32_t address, uint32_t words, uint32_t old, uint32_t end, semu_error *error)
{
    submission s = {{{0u, 0u}}, 0u};
    uint32_t ids[NEMA_COMPLETION_MAX_EVENTS];
    size_t count = 0u;
    semu_status status;
    semu_transaction_result result;
    int immediate = 0, prepared = 0;
    status = nema_framing_parse(g->bus, address, words, old, end,
        collect_child, &s, NULL, NULL, error);
    if (status != SEMU_OK) return status;
    status = markers(g, address, words, old, end, ids, &count, error);
    if (status != SEMU_OK) return status;
    if (s.count == 0u && count == 0u) {
        s.lists[s.count++] = end >= old
            ? (semu_display_list){g->previous_ring_stop, end - old}
            : (semu_display_list){address, words};
        immediate = 1;
    }
    if (count != 0u && g->scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_STATE, "nema: completion scheduler missing");
        return SEMU_ERR_STATE;
    }
    if (s.count != 0u) {
        semu_error_clear(error);
        result = g->backend.prepare(g->backend_context, g->bus, s.lists, s.count,
            0u, g->frame_callback, g->frame_context, error);
        if (result != SEMU_TRANSACTION_OK || error->code != SEMU_OK) {
            if (result != SEMU_TRANSACTION_REFUSE) g->backend.abort(g->backend_context);
            if (error->code < SEMU_ERR_ARGUMENT || error->code > SEMU_ERR_NOMEM)
                semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                    "nema: backend refused or returned unsupported result %u", (unsigned)result);
            return error->code;
        }
        prepared = 1;
    }
    if (count != 0u) {
        status = nema_completion_schedule_batch(g->completion, g->scheduler, ids, count,
            nema_gpu_completion_write, g, nema_gpu_completion_irq, g, error);
        if (status != SEMU_OK) {
            if (prepared) g->backend.abort(g->backend_context);
            return status;
        }
    }
    /* No fallible operation remains after atomic scheduler admission. */
    g->registers[NEMA_REG_CMDRINGSTOP / 4u] = raw;
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
