#include "nema_backend_internal.h"
#include "nema_tsc6a_internal.h"

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

static void save_cache(semu_nema_backend *b)
{
    if (b->baseline_saved) return;
    b->saved_baseline_valid = b->baseline_valid;
    b->saved_guest_span_base = b->guest_span_base;
    if (b->baseline_valid) {
        nema_tsc6a_copy(b->saved_baseline, b->baseline);
        memcpy(b->saved_guest_span, b->guest_span, NEMA_TSC6A_SPAN_BYTES);
    }
    b->baseline_saved = 1;
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
        /* Preserve the established cache policy. Clearing an old decoded
         * block here changes pinned snapshots; that correction requires
         * separate golden re-derivation (E-EMU-NEMA-CACHE-LIFECYCLE-001). */
        return;
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
semu_status nema_tsc6a_frame_baseline(semu_nema_backend *backend, semu_bus *bus,
                                      uint32_t base, semu_error *error)
{
    uint8_t *span;
    size_t block;
    int full;
    semu_status st;

    if (backend == NULL || bus == NULL || backend->baseline == NULL ||
        backend->guest_span == NULL || backend->span_scratch == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "nema_tsc6a: invalid baseline cache");
        return SEMU_ERR_ARGUMENT;
    }
    span = backend->span_scratch;
    /* Validate and copy the whole span before changing any cached state.
     * copy_out is memory-only; an unmapped span is a submission refusal. */
    st = semu_bus_copy_out(bus, base, span, NEMA_TSC6A_SPAN_BYTES, error);
    if (st != SEMU_OK) return st;
    full = !backend->baseline_valid || backend->guest_span_base != base;
    if (full || memcmp(span, backend->guest_span, NEMA_TSC6A_SPAN_BYTES) != 0) {
        save_cache(backend);
        if (full) nema_tsc6a_reset(backend->baseline);
        for (block = 0u; block < SYNC_SPAN_BLOCKS; ++block) {
            if (full || memcmp(span + block * 12u,
                               backend->guest_span + block * 12u, 12u) != 0) {
                decode_block(backend->baseline, block, span + block * 12u);
            }
        }
        memcpy(backend->guest_span, span, NEMA_TSC6A_SPAN_BYTES);
    }
    backend->guest_span_base = base;
    backend->baseline_valid = 1;
    return SEMU_OK;
}

/* Called only after a successful baseline refresh. Frame lifecycle state
 * belongs to the pending transaction, just like the shadow pixels. */
void nema_tsc6a_frame_begin(semu_nema_backend *backend, nema_tsc6a *shadow)
{
    nema_tsc6a_copy(shadow, backend->baseline);
    backend->pending_shadow_fresh = 1;
}

/* A successful resolve consumes the pending composition. */
void nema_tsc6a_frame_end(semu_nema_backend *backend, nema_tsc6a *shadow)
{
    nema_tsc6a_copy(shadow, backend->baseline);
    backend->pending_shadow_fresh = 0;
}
