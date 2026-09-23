#ifndef SEMU_DISPLAY_NEMA_BACKEND_H
#define SEMU_DISPLAY_NEMA_BACKEND_H

#include "semu/bus.h"
#include "semu/display.h"
#include "semu/frame.h"
#include "semu/types.h"
#include "nema_framing.h"
#include "nema_state.h"
#include "nema_diagnostics.h"

/*
 * Nema backend and panel frame publication (ticket 513).
 * Wires framing -> state -> texture/draw -> completion into a frozen
 * backend callback.  Renders to an internal canonical RGB565LE
 * surface; generation increments once per renderer publication, not
 * per draw.  Physical panel publication requires E-NEMA-PANEL-001
 * (missing); the backend publishes renderer output to the frontend
 * but does not claim physical-panel frames.
 */

#define NEMA_BACKEND_PANEL_WIDTH  240u
#define NEMA_BACKEND_PANEL_HEIGHT 240u
#define NEMA_BACKEND_PANEL_BYTES \
    (NEMA_BACKEND_PANEL_WIDTH * NEMA_BACKEND_PANEL_HEIGHT * 2u)

typedef struct semu_nema_backend semu_nema_backend;

semu_nema_backend *semu_nema_backend_create(semu_error *error);
void semu_nema_backend_destroy(semu_nema_backend *backend);
/* Reset refuses with CONFLICT while preparing/prepared/publishing. Destroy
 * aborts a prepared transaction; destruction from callbacks is unsupported. */
semu_status semu_nema_backend_reset(semu_nema_backend *backend);

/* Public transactional contract; GPU/machine and the single-list convenience
 * all use these same operations. */
extern const semu_display_backend_ops semu_nema_backend_ops;
extern const semu_display_snapshot_ops semu_nema_backend_snapshot_ops;

/*
 * The submit function matching the frozen callback signature
 * (semu_display_backend_submit_fn).  Parses the command list at
 * command_ring_address (command_word_count 32-bit words), feeds
 * records through nema_state, and executes evidenced draws on the
 * internal surface.  Returns REFUSE on unsupported commands with
 * bounded diagnostics; pixels, inherited state and shadows are unchanged on
 * refusal. Odd word counts refuse before staging (E-EMU-NEMA-TAIL-001);
 * empty lists publish nothing. The original error survives diagnostic
 * saturation and a NULL error sink is supported.
 */
semu_transaction_result semu_nema_backend_submit(
    void *context, semu_bus *bus, uint32_t command_ring_address,
    uint32_t command_word_count, uint64_t virtual_time_ns,
    semu_frame_callback frame_callback, void *frame_context,
    semu_error *error);

/* Borrow the internal diagnostic surface frame (read-only). */
const semu_frame *semu_nema_backend_frame(semu_nema_backend *backend);

/* Borrow the diagnostics (read-only).  Returns NULL if none. */
const nema_diagnostics *semu_nema_backend_diagnostics(
    semu_nema_backend *backend);

/* Internal: increment surface generation on physical publication.
 * Defined in surface.c; declared here for backend use. */
void semu_surface_publish(semu_surface *surface);
/* Internal codec helper; caller validates the complete image before commit. */
void semu_surface_restore_generation(semu_surface *surface, uint64_t generation);

/* Internal: borrow writable pixel pointer and stride.
 * Defined in surface.c; declared here for backend use. */
uint8_t *semu_surface_pixels(semu_surface *surface, uint32_t *out_stride);

#endif
