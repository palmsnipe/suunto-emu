#ifndef SEMU_NEMA_COMPLETION_H
#define SEMU_NEMA_COMPLETION_H

#include "semu/scheduler.h"
#include "semu/types.h"
#include "../core/scheduler_internal.h"
#include "../core/snapshot_io.h"

/*
 * Nema completion events (ticket 506).
 * Schedules evidenced command-list completion: 100 µs delay, then
 * writes CLID + INTERRUPT=1 and asserts IRQ28.  Exactly one
 * completion per accepted list; refused/bootstrap emit none.
 *
 * Evidence: E-NEMA-RING-001 (no bootstrap IRQ),
 *           E-NEMA-LISTS-001 (completion ID/status/IRQ ordering).
 */

#include "../../src/display/nema_framing.h"

#define NEMA_COMPLETION_IRQ_LINE  28u
#define NEMA_COMPLETION_DELAY_NS  100000u  /* 100 µs */
#define NEMA_COMPLETION_MAX_EVENTS 64u

typedef void (*nema_reg_write_fn)(void *context, uint32_t offset,
                                   uint32_t value);
typedef void (*nema_irq_fn)(void *context, unsigned irq_line,
                             int asserted);

typedef struct nema_completion nema_completion;

semu_status nema_completion_create(nema_completion **out,
                                    semu_error *error);
void nema_completion_destroy(nema_completion *comp);
void nema_completion_reset(nema_completion *comp);

/* The bound scheduler must outlive active completions. Cancel/reset/destroy
 * removes their owned queued callbacks. Rebind a restored image to the target
 * scheduler before restoring its events or scheduling further completions.
 * An unbound snapshot-only object owns no queued events and may be destroyed. */

/*
 * Schedule a completion for the given list ID.  Only one completion
 * per list ID; calling twice for the same ID is a no-op.  When the
 * delay elapses, writes CLID=list_id, INTERRUPT=1, then asserts IRQ.
 * Dispatch retires the entry and captures this notification tuple before
 * calling out. Scheduling/rebinding cannot redirect an in-flight sequence;
 * cancel/reset/destroy removes queued work but does not revoke that sequence.
 * Callback contexts must remain alive through all three notifications, even
 * if the completion owner itself is reset or destroyed from a notification.
 * The scheduler must remain alive; its recursive-dispatch restrictions apply.
 * These rules do not permit freeing a callback context's machine/device owner.
 */
semu_status nema_completion_schedule(nema_completion *comp,
                                     semu_scheduler *scheduler,
                                     uint32_t list_id,
                                     nema_reg_write_fn on_reg_write,
                                     void *reg_context,
                                     nema_irq_fn on_irq,
                                     void *irq_context,
                                     semu_error *error);

/* Atomically admit a bounded group. Pending/repeated IDs remain no-ops;
 * failure preserves all entries, counters, scheduler IDs and queued events.
 * An active completion group belongs to one scheduler until reset/drained. */
semu_status nema_completion_schedule_batch(nema_completion *comp,
    semu_scheduler *scheduler, const uint32_t *list_ids, size_t count,
    nema_reg_write_fn on_reg_write, void *reg_context, nema_irq_fn on_irq,
    void *irq_context, semu_error *error);

/* Cancel all pending completions and their scheduler callbacks. */
void nema_completion_cancel(nema_completion *comp);

/* Total completions emitted since creation or last reset. */
size_t nema_completion_count(const nema_completion *comp);

/* Whether a completion is pending for the given list ID. */
int nema_completion_pending(const nema_completion *comp,
                            uint32_t list_id);
void nema_completion_rebind_active(
    nema_completion *comp, semu_scheduler *scheduler, nema_reg_write_fn on_reg_write,
    void *reg_context, nema_irq_fn on_irq, void *irq_context);
semu_status nema_completion_snapshot_write(
    const nema_completion *comp, semu_snapshot_writer *writer,
    semu_error *error);
semu_status nema_completion_snapshot_read(
    nema_completion *comp, semu_snapshot_reader *reader, semu_error *error);
semu_status nema_completion_snapshot_resolve_event(
    nema_completion *comp, uint32_t subject, semu_event_callback *callback,
    void **context, semu_error *error);
semu_status nema_completion_snapshot_event_id_matches(
    const nema_completion *comp, uint32_t subject, semu_event_id event_id,
    semu_error *error);
semu_status nema_completion_snapshot_event_links_match(
    const nema_completion *comp, const semu_scheduled_event_state *events,
    size_t count, semu_error *error);

#endif
