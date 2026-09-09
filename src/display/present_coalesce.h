#ifndef SEMU_PRESENT_COALESCE_H
#define SEMU_PRESENT_COALESCE_H

#include "semu/frame.h"
#include "semu/types.h"

/* Presentation-side burst coalescer.
 *
 * The display backend publishes one frame per guest publish-flagged
 * transaction. Animation redraws arrive as bursts of such publishes minutes
 * apart by 100 us to a few ms of virtual time (observed on the 2.22.60
 * w-ltim "Searching for GPS" ring: 84 intermediate composites within one
 * burst, gap median 0.36 ms, versus ~29 ms between redraw ticks). A physical
 * LCD latches only at frame boundaries, so only the final composite of a
 * burst is ever visible; presenting every intermediate produces strobing
 * partial-composite flicker.
 *
 * This coalescer holds the newest frame of a burst and releases the
 * previous burst's final frame when a gap of at least threshold_ns of
 * virtual time separates two publishes. It is pure host-side presentation
 * policy: it never mutates guest state and releases frames in order. The
 * released frame borrows internal storage until the next call. Invalid
 * frames are refused without disturbing the held frame. */
typedef struct semu_present_coalescer semu_present_coalescer;

semu_present_coalescer *semu_present_coalescer_create(uint64_t threshold_ns,
                                                      semu_error *error);
void semu_present_coalescer_destroy(semu_present_coalescer *co);

/* Returns a frame to present now, or NULL when nothing matured. The frame
 * argument is borrowed only for the call and may be retained internally. */
const semu_frame *semu_present_coalescer_observe(
    semu_present_coalescer *co, const semu_frame *frame,
    uint64_t virtual_time_ns, semu_error *error);

/* Releases any held frame once the source is known finished (e.g. stop). */
const semu_frame *semu_present_coalescer_flush(
    semu_present_coalescer *co);

#endif
