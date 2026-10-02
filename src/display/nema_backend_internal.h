#ifndef SEMU_NEMA_BACKEND_INTERNAL_H
#define SEMU_NEMA_BACKEND_INTERNAL_H
#include "nema_backend.h"
#include "nema_tsc6a.h"

enum { NEMA_BACKEND_IDLE, NEMA_BACKEND_PREPARING, NEMA_BACKEND_PREPARED,
       NEMA_BACKEND_COMMITTING };
#define NEMA_TSC6A_SPAN_BYTES (14400u * 12u)
struct semu_nema_backend {
    semu_surface *surface;
    semu_frame published;
    uint8_t *published_pixels;
    int published_valid;
    nema_state *state, *pending_state;
    nema_diagnostics *diag;
    nema_tsc6a *tsc6a, *pending_tsc6a;
    uint8_t *working_pixels, *frames;
    size_t frame_count;
    semu_frame_callback callback;
    void *frame_context;
    int phase, tsc6a_dirty;
    /* Ticket 710 instance (E-EMU-SAP235-TICKTRAIL-001): the compressed
     * surface's resting state is the decode of its real SRAM span, kept
     * in a baseline surface that is re-derived whenever the guest
     * rewrites the span (the cached bytes drive a per-block diff; the
     * 788 block law decodes, undecodable blocks keep the reset value).
     * Each frame begins and ends at the resting state — the resolve
     * consumes the frame, so no stroke survives into the next one. */
    uint8_t *guest_span, *span_scratch, *saved_guest_span;
    uint32_t guest_span_base;
    nema_tsc6a *baseline, *saved_baseline;
    int baseline_valid;
    /* Copied lazily before the first cache mutation in a transaction. */
    int baseline_saved, saved_baseline_valid;
    uint32_t saved_guest_span_base;
    int shadow_fresh, pending_shadow_fresh;
    semu_error draw_error;
    /* Draw-state refusal event (ticket 794): the snapshot captured at the
     * refused draw plus the bounded event record. */
    nema_draw_snapshot refusal_snapshot;
    nema_draw_refusal draw_refusal;
    int draw_refusal_valid;
};
typedef struct { semu_nema_backend *backend; semu_bus *bus; } nema_draw_context;
void nema_backend_draw(void *context, const nema_draw_snapshot *snapshot);
semu_status nema_tsc6a_frame_baseline(semu_nema_backend *backend, semu_bus *bus,
    uint32_t base, semu_error *error);
semu_status nema_tsc6a_validate_resolve(const semu_nema_backend *backend,
    const nema_draw_snapshot *snapshot, semu_error *error);
void nema_tsc6a_frame_begin(semu_nema_backend *backend, nema_tsc6a *shadow);
void nema_tsc6a_frame_end(semu_nema_backend *backend, nema_tsc6a *shadow);
semu_status nema_backend_render_list(semu_nema_backend *backend, semu_bus *bus,
    const semu_display_list *list, semu_error *error);
#endif
