#include "nema_backend_internal.h"
#include <stdlib.h>

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
    b->working_pixels = calloc(NEMA_BACKEND_PANEL_BYTES, 1u);
    if (b->working_pixels == NULL) {
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
    free(b->frames);
    free(b->working_pixels);
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
    semu_error_clear(&b->draw_error);
    semu_surface_clear(b->surface, 0u);
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
