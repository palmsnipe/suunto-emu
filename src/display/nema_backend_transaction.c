#include "nema_backend_internal.h"
#include <stdlib.h>
#include <string.h>

static void discard(semu_nema_backend *b)
{
    /* Cache contents can carry history for undecoded blocks. Invalidation
     * alone is not rollback: preserve the exact pre-submission cache. */
    if (b->baseline_saved) {
        if (b->saved_baseline_valid) {
            nema_tsc6a *baseline = b->baseline;
            uint8_t *span = b->guest_span;
            b->baseline = b->saved_baseline; b->saved_baseline = baseline;
            b->guest_span = b->saved_guest_span; b->saved_guest_span = span;
        }
        b->baseline_valid = b->saved_baseline_valid;
        b->guest_span_base = b->saved_guest_span_base;
        b->baseline_saved = 0;
    }
    free(b->frames); b->frames = NULL;
    b->frame_count = 0u;
    b->callback = NULL; b->frame_context = NULL;
    b->tsc6a_dirty = 0;
    b->phase = NEMA_BACKEND_IDLE;
}

static semu_transaction_result prepare(void *context, semu_bus *bus,
    const semu_display_list *lists, size_t count, uint64_t time,
    semu_frame_callback callback, void *frame_context, semu_error *error)
{
    semu_nema_backend *b = context;
    semu_error local;
    size_t i, nonempty = 0u;
    (void)time;
    if (error == NULL) error = &local;
    semu_error_clear(error);
    if (b == NULL || bus == NULL || (count != 0u && lists == NULL)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "backend prepare: null");
        return SEMU_TRANSACTION_REFUSE;
    }
    /* Submission-level diagnostic: the previous prepare's draw-refusal
     * event never survives into this prepare, whatever this one returns
     * (a CONFLICT refuse carries no new encounter). */
    b->draw_refusal_valid = 0;
    if (b->phase != NEMA_BACKEND_IDLE) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "backend: transaction already active");
        return SEMU_TRANSACTION_REFUSE;
    }
    if (count > SEMU_DISPLAY_MAX_LISTS) {
        semu_error_set(error, SEMU_ERR_RANGE, "backend: too many lists");
        return SEMU_TRANSACTION_REFUSE;
    }
    for (i = 0u; i < count; ++i) {
        const semu_display_list *list = &lists[i];
        if (list->flags > SEMU_DISPLAY_LIST_INLINE) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED, "backend: invalid list flags");
            return SEMU_TRANSACTION_REFUSE;
        }
        if (list->word_count == 0u) continue;
        if ((list->word_count & 1u) != 0u || list->word_count > NEMA_MAX_LIST_WORDS ||
            list->word_count > (UINT32_MAX - list->address) / 4u ||
            (list->address & 3u) != 0u) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                "backend: invalid list range address=0x%08x words=%u",
                list->address, list->word_count);
            (void)nema_diagnostics_record(b->diag, (list->address & 3u) != 0u ?
                NEMA_DIAG_BAD_ALIGNMENT : NEMA_DIAG_BAD_SIZE, 0u,
                list->address, 0u, list->word_count, NULL, 0u, NULL);
            return SEMU_TRANSACTION_REFUSE;
        }
        if (list->flags == 0u) ++nonempty;
    }
    if (callback != NULL && nonempty >
        UINT64_MAX - semu_surface_frame(b->surface)->generation) {
        semu_error_set(error, SEMU_ERR_RANGE, "backend: frame generation overflow");
        return SEMU_TRANSACTION_REFUSE;
    }
    if (callback != NULL && nonempty != 0u) {
        b->frames = malloc(nonempty * (size_t)NEMA_BACKEND_PANEL_BYTES);
        if (b->frames == NULL) {
            semu_error_set(error, SEMU_ERR_NOMEM, "backend: cannot stage frames");
            return SEMU_TRANSACTION_REFUSE;
        }
    }
    b->phase = NEMA_BACKEND_PREPARING;
    b->tsc6a_dirty = 0;
    b->pending_shadow_fresh = b->shadow_fresh;
    semu_error_clear(&b->draw_error);
    nema_state_copy(b->pending_state, b->state);
    memcpy(b->working_pixels, semu_surface_frame(b->surface)->pixels,
        NEMA_BACKEND_PANEL_BYTES);
    for (i = 0u; i < count; ++i) {
        if (lists[i].word_count == 0u) continue;
        if (nema_backend_render_list(b, bus, &lists[i], error) != SEMU_OK) {
            discard(b);
            return SEMU_TRANSACTION_REFUSE;
        }
        if (callback != NULL && lists[i].flags == 0u) {
            memcpy(b->frames + b->frame_count * NEMA_BACKEND_PANEL_BYTES,
                b->working_pixels, NEMA_BACKEND_PANEL_BYTES);
            ++b->frame_count;
        }
    }
    b->callback = callback; b->frame_context = frame_context;
    b->phase = NEMA_BACKEND_PREPARED;
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

static void abort_transaction(void *context)
{
    semu_nema_backend *b = context;
    if (b != NULL && b->phase == NEMA_BACKEND_PREPARED) discard(b);
}

static void commit(void *context)
{
    semu_nema_backend *b = context;
    nema_state *state;
    size_t i;
    uint8_t *pixels;
    if (b == NULL || b->phase != NEMA_BACKEND_PREPARED) return;
    b->phase = NEMA_BACKEND_COMMITTING;
    state = b->state; b->state = b->pending_state; b->pending_state = state;
    if (b->tsc6a_dirty) {
        nema_tsc6a *shadow = b->tsc6a;
        b->tsc6a = b->pending_tsc6a; b->pending_tsc6a = shadow;
    }
    b->shadow_fresh = b->pending_shadow_fresh;
    b->baseline_saved = 0; /* The updated cache is now committed. */
    pixels = semu_surface_pixels(b->surface, NULL);
    if (b->callback != NULL) {
        for (i = 0u; i < b->frame_count; ++i) {
            memcpy(pixels, b->frames + i * NEMA_BACKEND_PANEL_BYTES,
                NEMA_BACKEND_PANEL_BYTES);
            semu_surface_publish(b->surface);
            memcpy(b->published_pixels, pixels, NEMA_BACKEND_PANEL_BYTES);
            b->published = *semu_surface_frame(b->surface);
            b->published.pixels = b->published_pixels;
            b->published_valid = 1;
            b->callback(b->frame_context, semu_surface_frame(b->surface));
        }
    }
    /* Inline draws after the last published child still commit their pixels. */
    memcpy(pixels, b->working_pixels, NEMA_BACKEND_PANEL_BYTES);
    discard(b);
}

const semu_display_backend_ops semu_nema_backend_ops = {
    prepare, commit, abort_transaction
};
