#ifndef SEMU_NEMA_TSC6A_H
#define SEMU_NEMA_TSC6A_H

#include "nema_state.h"
#include "semu/bus.h"
#include "semu/types.h"

/*
 * Bounded semantic model for the observed Sapporo TSC6A transition path
 * (E-NEMA-TSC6A-001).  It is a shadow/resolve model, not a compressed codec.
 *
 * The guest's compressed TSC6A bytes are deliberately not decoded.  Native
 * onboarding draws are accumulated in a fixed 480x480 ARGB shadow, then the
 * evidenced resolve program composites that shadow into the canonical RGB565
 * panel surface.  Unknown TSC6A state remains refused.
 *
 * Evidence: suunto-firmware/docs/research/native-tsc6a-transition-surface.md
 * (observed semantic shadow and resolve forms; no firmware bytes copied).
 */

#define NEMA_TSC6A_WIDTH  480u
#define NEMA_TSC6A_HEIGHT 480u
#define NEMA_TSC6A_PIXELS (NEMA_TSC6A_WIDTH * NEMA_TSC6A_HEIGHT)

typedef struct nema_tsc6a nema_tsc6a;

semu_status nema_tsc6a_create(nema_tsc6a **out, semu_error *error);
void nema_tsc6a_destroy(nema_tsc6a *surface);
void nema_tsc6a_reset(nema_tsc6a *surface);
void nema_tsc6a_copy(nema_tsc6a *destination, const nema_tsc6a *source);

/* Execute an evidenced TSC6A target triangle or A2LE mask. */
semu_status nema_tsc6a_draw_target(nema_tsc6a *surface, semu_bus *bus,
                                   const nema_draw_snapshot *snapshot,
                                   semu_error *error);

/* Resolve the semantic shadow through the observed TSC6A source program. */
semu_status nema_tsc6a_resolve(const nema_tsc6a *surface,
                               const nema_draw_snapshot *snapshot,
                               uint8_t *rgb565_le, uint32_t stride,
                               semu_error *error);

/* Resolve the observed DRAW_CMD=5 TSC6A mask/quad form. */
semu_status nema_tsc6a_resolve_mask(const nema_tsc6a *surface,
                                    const nema_draw_snapshot *snapshot,
                                    uint8_t *rgb565_le, uint32_t stride,
                                    semu_error *error);

/* Execute the observed RGB565 solid/edge-AA triangle form. */
semu_status nema_tsc6a_draw_rgb565_triangle(
    const nema_draw_snapshot *snapshot, uint8_t *rgb565_le,
    uint32_t width, uint32_t height, uint32_t stride, int edge_antialias,
    semu_error *error);

#endif
