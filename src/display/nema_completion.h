#ifndef SEMU_NEMA_COMPLETION_H
#define SEMU_NEMA_COMPLETION_H

#include "semu/scheduler.h"
#include "semu/types.h"
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

/*
 * Schedule a completion for the given list ID.  Only one completion
 * per list ID; calling twice for the same ID is a no-op.  When the
 * delay elapses, writes CLID=list_id, INTERRUPT=1, then asserts IRQ.
 */
semu_status nema_completion_schedule(nema_completion *comp,
                                     semu_scheduler *scheduler,
                                     uint32_t list_id,
                                     nema_reg_write_fn on_reg_write,
                                     void *reg_context,
                                     nema_irq_fn on_irq,
                                     void *irq_context,
                                     semu_error *error);

/* Cancel all pending completions. */
void nema_completion_cancel(nema_completion *comp);

/* Total completions emitted since creation or last reset. */
size_t nema_completion_count(const nema_completion *comp);

/* Whether a completion is pending for the given list ID. */
int nema_completion_pending(const nema_completion *comp,
                            uint32_t list_id);
void nema_completion_rebind_active(
    nema_completion *comp, nema_reg_write_fn on_reg_write,
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

#endif
