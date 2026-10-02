#include "nema_backend_internal.h"
#include <stdlib.h>
#include <string.h>

semu_nema_backend *semu_nema_backend_create(semu_error *error)
{
    semu_nema_backend *b = calloc(1u, sizeof(*b));
    if (b == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate backend");
        return NULL;
    }
    b->surface = semu_surface_create(NEMA_BACKEND_PANEL_WIDTH,
        NEMA_BACKEND_PANEL_HEIGHT, error);
    if (b->surface == NULL ||
        nema_state_create(&b->state, error) != SEMU_OK ||
        nema_state_create(&b->pending_state, error) != SEMU_OK ||
        nema_diagnostics_create(&b->diag, error) != SEMU_OK ||
        nema_tsc6a_create(&b->tsc6a, error) != SEMU_OK ||
        nema_tsc6a_create(&b->pending_tsc6a, error) != SEMU_OK) {
        semu_nema_backend_destroy(b);
        return NULL;
    }
    b->published_pixels = calloc(NEMA_BACKEND_PANEL_BYTES, 1u);
    b->working_pixels = calloc(NEMA_BACKEND_PANEL_BYTES, 1u);
    b->guest_span = calloc(NEMA_TSC6A_SPAN_BYTES, 1u);
    b->span_scratch = malloc(NEMA_TSC6A_SPAN_BYTES);
    b->saved_guest_span = malloc(NEMA_TSC6A_SPAN_BYTES);
    if (nema_tsc6a_create(&b->baseline, error) != SEMU_OK ||
        nema_tsc6a_create(&b->saved_baseline, error) != SEMU_OK) {
        semu_nema_backend_destroy(b);
        return NULL;
    }
    if (b->working_pixels == NULL || b->published_pixels == NULL ||
        b->guest_span == NULL || b->span_scratch == NULL ||
        b->saved_guest_span == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate staging pixels");
        semu_nema_backend_destroy(b);
        return NULL;
    }
    return b;
}

void semu_nema_backend_destroy(semu_nema_backend *b)
{
    if (b == NULL || b->phase == NEMA_BACKEND_PREPARING ||
        b->phase == NEMA_BACKEND_COMMITTING) return;
    free(b->published_pixels);
    free(b->frames);
    free(b->working_pixels);
    free(b->guest_span);
    free(b->span_scratch);
    free(b->saved_guest_span);
    nema_tsc6a_destroy(b->saved_baseline);
    nema_tsc6a_destroy(b->baseline);
    nema_tsc6a_destroy(b->pending_tsc6a);
    nema_tsc6a_destroy(b->tsc6a);
    nema_diagnostics_destroy(b->diag);
    nema_state_destroy(b->pending_state);
    nema_state_destroy(b->state);
    semu_surface_destroy(b->surface);
    free(b);
}

semu_status semu_nema_backend_reset(semu_nema_backend *b)
{
    if (b == NULL) return SEMU_ERR_ARGUMENT;
    if (b->phase != NEMA_BACKEND_IDLE) return SEMU_ERR_CONFLICT;
    nema_state_reset(b->state);
    nema_state_reset(b->pending_state);
    nema_diagnostics_reset(b->diag);
    nema_tsc6a_reset(b->tsc6a);
    nema_tsc6a_reset(b->pending_tsc6a);
    b->tsc6a_dirty = 0;
    b->baseline_valid = 0;
    b->baseline_saved = 0;
    b->shadow_fresh = 0;
    b->pending_shadow_fresh = 0;
    semu_error_clear(&b->draw_error);
    b->draw_refusal_valid = 0;
    semu_surface_clear(b->surface, 0u);
    b->published_valid = 0;
    return SEMU_OK;
}

/* Diagnostic saturation never replaces the first renderer/bus error. */
static semu_status refuse(semu_nema_backend *b, nema_diag_category category,
    uint32_t address, uint32_t reg, uint32_t value, semu_error *error)
{
    if (error->code == SEMU_OK)
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
            "backend: %s at 0x%08x reg=0x%08x value=0x%08x",
            nema_diag_category_name(category), address, reg, value);
    (void)nema_diagnostics_record(b->diag, category, 0u, address,
        reg, value, NULL, 0u, NULL);
    return error->code;
}

/* Record the bounded draw-state refusal event (ticket 794).  Exactly one
 * event survives per refused transaction: the render loop stops at the
 * first encounter, so this is called at most once per prepare. */
static void record_draw_refusal(semu_nema_backend *b,
    const semu_display_list *list, uint32_t offset_bytes)
{
    size_t n = strlen(b->draw_error.text);
    if (n >= sizeof(b->draw_refusal.reason))
        n = sizeof(b->draw_refusal.reason) - 1u;
    memset(&b->draw_refusal, 0, sizeof(b->draw_refusal));
    b->draw_refusal.child_address = list->address;
    b->draw_refusal.offset_bytes = offset_bytes;
    b->draw_refusal.snapshot = b->refusal_snapshot;
    memcpy(b->draw_refusal.reason, b->draw_error.text, n);
    b->draw_refusal.reason[n] = '\0';
    b->draw_refusal_valid = 1;
}

semu_status nema_backend_render_list(semu_nema_backend *b, semu_bus *bus,
    const semu_display_list *list, semu_error *error)
{
    uint32_t i;
    nema_draw_context draw = {b, bus};
    nema_state_begin_list(b->pending_state, 0u);
    for (i = 0u; i + 1u < list->word_count; i += 2u) {
        uint32_t reg, value, address = list->address + i * 4u;
        nema_record record;
        if (nema_command_read_word(bus, address, &reg, error) != SEMU_OK)
            return refuse(b, NEMA_DIAG_FRAMING_ERROR, address, 0u, 0u, error);
        if (nema_command_read_word(bus, address + 4u, &value, error) != SEMU_OK)
            return refuse(b, NEMA_DIAG_FRAMING_ERROR, address + 4u, 0u, 0u, error);
        if ((reg >> 24u) != 0u && (reg >> 24u) != 0xffu)
            return refuse(b, NEMA_DIAG_BAD_PREFIX, address, reg, value, error);
        if ((reg & 3u) != 0u)
            return refuse(b, NEMA_DIAG_BAD_ALIGNMENT, address, reg, value, error);
        record = (nema_record){(uint8_t)(reg >> 24u), reg & 0xffffffu, value, address + 4u};
        if (nema_state_record(b->pending_state, &record, nema_backend_draw,
            &draw, error) != SEMU_OK)
            return refuse(b, NEMA_DIAG_UNKNOWN_REGISTER, address + 4u, reg, value, error);
        if (b->draw_error.code != SEMU_OK) {
            *error = b->draw_error;
            record_draw_refusal(b, list, i * 4u);
            return refuse(b, NEMA_DIAG_UNSUPPORTED_DRAW, address + 4u, reg, value, error);
        }
    }
    return SEMU_OK;
}

semu_transaction_result semu_nema_backend_submit(void *context, semu_bus *bus,
    uint32_t address, uint32_t words, uint64_t time,
    semu_frame_callback callback, void *frame_context, semu_error *error)
{
    const semu_display_list list = {address, words, 0u};
    semu_transaction_result result = semu_nema_backend_ops.prepare(context, bus,
        &list, 1u, time, callback, frame_context, error);
    if (result == SEMU_TRANSACTION_OK) semu_nema_backend_ops.commit(context);
    return result;
}

const semu_frame *semu_nema_backend_frame(semu_nema_backend *backend)
{
    return backend != NULL ? semu_surface_frame(backend->surface) : NULL;
}

const nema_diagnostics *semu_nema_backend_diagnostics(semu_nema_backend *backend)
{
    return backend != NULL ? backend->diag : NULL;
}

const nema_draw_refusal *semu_nema_backend_draw_refusal(
    const semu_nema_backend *backend)
{
    return backend != NULL && backend->draw_refusal_valid != 0
        ? &backend->draw_refusal : NULL;
}
