#ifndef SEMU_NEMA_BACKEND_INTERNAL_H
#define SEMU_NEMA_BACKEND_INTERNAL_H
#include "nema_backend.h"
#include "nema_tsc6a.h"

enum { NEMA_BACKEND_IDLE, NEMA_BACKEND_PREPARING, NEMA_BACKEND_PREPARED,
       NEMA_BACKEND_COMMITTING };
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
    semu_error draw_error;
    /* Draw-state refusal event (ticket 794): the snapshot captured at the
     * refused draw plus the bounded event record. */
    nema_draw_snapshot refusal_snapshot;
    nema_draw_refusal draw_refusal;
    int draw_refusal_valid;
};
typedef struct { semu_nema_backend *backend; semu_bus *bus; } nema_draw_context;
void nema_backend_draw(void *context, const nema_draw_snapshot *snapshot);
semu_status nema_backend_render_list(semu_nema_backend *backend, semu_bus *bus,
    const semu_display_list *list, semu_error *error);
#endif
