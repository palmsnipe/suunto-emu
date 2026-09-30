#include "nema_backend_internal.h"
#include "nema_tsc6a_internal.h"

#include <stdlib.h>
#include <string.h>

/*
 * Ticket 710 instance (E-EMU-SAP235-TICKTRAIL-001): the 2.35 main-screen
 * seconds sweep left an accumulating trail because the semantic shadow
 * persists across frames and no GPU draw ever clears it.  The pinned
 * firmware research record (native-tsc6a-transition-surface.md) documents
 * the identical failure class on the 2.22 resolve destination — "a single
 * rotating arc became a dense fan" — fixed there by giving the resolve
 * per-frame-fresh semantics; this is the source-side application of the
 * same ruling.
 *
 * Model: the compressed surface's resting state is the decode of its real
 * SRAM span (the guest's composition; census /tmp/sap235-tsc6a-b, span
 * sha 373b538e… byte-stable across the whole tick continuation, twice).
 * Each resolve consumes the frame: the frame's draws blend onto the
 * resting state, the resolve publishes them, and the shadow then returns
 * to the resting state for the next frame.  The resting state is kept in
 * a baseline surface re-decoded through the ticket-788 block law whenever
 * the guest rewrites the span; blocks the law cannot decode (the
 * aux-bit population, 5,441 of 14,400 — every one outside the pinned
 * resolve clips, twice-derived) stay at the transparent-black reset
 * value, the same content they hold in the pre-fix shadow.
 */

#define SYNC_SPAN_BLOCKS 14400u
#define SYNC_SPAN_BYTES (SYNC_SPAN_BLOCKS * 12u)

static int read_span(semu_bus *bus, uint32_t base, uint8_t *destination)
{
    semu_error error;
    semu_error_clear(&error);
    return semu_bus_copy_out(bus, base, destination, SYNC_SPAN_BYTES,
                             &error) == SEMU_OK;
}

/* Decode one 4x4 block into a 480x480 shadow surface. */
static void decode_block(nema_tsc6a *surface, size_t block,
                         const uint8_t *bytes)
{
    uint8_t texels[16][4];
    unsigned row = (unsigned)(block / 120u);
    unsigned col = (unsigned)(block % 120u);
    unsigned p;
    if (!tsc6a_expand_block(bytes, texels)) {
        return; /* undecodable: keep the resting content */
    }
    for (p = 0u; p < 16u; ++p) {
        size_t x = (size_t)col * 4u + p % 4u;
        size_t y = (size_t)row * 4u + p / 4u;
        surface->pixels[y * NEMA_TSC6A_WIDTH + x] =
            ((uint32_t)texels[p][3] << 24u) |
            ((uint32_t)texels[p][0] << 16u) |
            ((uint32_t)texels[p][1] << 8u) |
            (uint32_t)texels[p][2];
    }
}

/* Re-derive the baseline from the guest's current span bytes.  The first
 * build decodes every block; later calls decode only the blocks the
 * guest rewrote since the cached copy. */
void nema_tsc6a_frame_baseline(semu_nema_backend *backend, semu_bus *bus,
                               uint32_t base)
{
    uint8_t *span;
    size_t block;
    int full;

    if (backend == NULL || bus == NULL || backend->baseline == NULL ||
        backend->guest_span == NULL) {
        return;
    }
    if (backend->baseline_valid && backend->guest_span_base != base) {
        backend->baseline_valid = 0;
    }
    if (!backend->baseline_valid) {
        if (!read_span(bus, base, backend->guest_span)) {
            return;
        }
        nema_tsc6a_reset(backend->baseline);
        for (block = 0u; block < SYNC_SPAN_BLOCKS; ++block) {
            decode_block(backend->baseline, block,
                         backend->guest_span + block * 12u);
        }
        backend->guest_span_base = base;
        backend->baseline_valid = 1;
        return;
    }
    span = (uint8_t *)malloc(SYNC_SPAN_BYTES);
    if (span == NULL) {
        return;
    }
    if (!read_span(bus, base, span)) {
        free(span);
        return;
    }
    full = memcmp(span, backend->guest_span, SYNC_SPAN_BYTES) != 0;
    if (full) {
        for (block = 0u; block < SYNC_SPAN_BLOCKS; ++block) {
            if (memcmp(span + block * 12u,
                       backend->guest_span + block * 12u, 12u) != 0) {
                decode_block(backend->baseline, block, span + block * 12u);
            }
        }
        memcpy(backend->guest_span, span, SYNC_SPAN_BYTES);
    }
    free(span);
}

/* A frame begins: the shadow takes the resting state. */
void nema_tsc6a_frame_begin(semu_nema_backend *backend, semu_bus *bus,
                            uint32_t base, nema_tsc6a *shadow)
{
    if (backend == NULL || shadow == NULL || backend->baseline == NULL) {
        return;
    }
    if (!backend->baseline_valid || backend->shadow_fresh == 0) {
        nema_tsc6a_frame_baseline(backend, bus, base);
    }
    if (backend->baseline_valid) {
        nema_tsc6a_copy(shadow, backend->baseline);
        backend->shadow_fresh = 1;
    }
}

/* A frame ends: the resolve has consumed the composition, so the shadow
 * returns to the resting state for the next frame. */
void nema_tsc6a_frame_end(semu_nema_backend *backend, nema_tsc6a *shadow)
{
    if (backend == NULL || shadow == NULL || backend->baseline == NULL ||
        !backend->baseline_valid) {
        return;
    }
    nema_tsc6a_copy(shadow, backend->baseline);
    backend->shadow_fresh = 0;
}
