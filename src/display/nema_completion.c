/*
 * Nema completion events (ticket 506).
 * Schedules evidenced command-list completion with 100 µs delay.
 */

#include "nema_completion_internal.h"
#include "../core/scheduler_internal.h"

#include <stdlib.h>
#include <string.h>

void nema_completion_callback(void *context, uint64_t now_ns)
{
    completion_entry *e = (completion_entry *)context;
    completion_entry notification;
    nema_completion *comp;
    (void)now_ns;
    if (!e->active) {
        return;
    }
    /* Callbacks may reuse/reset/destroy this slot's owner. Never reread the
     * entry after the first notification; callback contexts must remain live. */
    notification = *e;
    comp = e->owner;
    e->active = 0;
    if (comp != NULL && comp->active_count > 0u) {
        --comp->active_count;
    }
    if (notification.on_reg_write != NULL) {
        notification.on_reg_write(notification.reg_context, NEMA_REG_CLID, notification.list_id);
        notification.on_reg_write(notification.reg_context, NEMA_REG_INTERRUPT, 1u);
    }
    if (notification.on_irq != NULL) {
        notification.on_irq(notification.irq_context, NEMA_COMPLETION_IRQ_LINE, 1);
    }
}

semu_status nema_completion_create(nema_completion **out,
                                    semu_error *error)
{
    nema_completion *comp;

    comp = (nema_completion *)calloc(1u, sizeof(*comp));
    if (comp == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "completion: alloc failed");
        return SEMU_ERR_NOMEM;
    }
    *out = comp;
    return SEMU_OK;
}

void nema_completion_destroy(nema_completion *comp)
{
    if (comp == NULL) return;
    nema_completion_cancel(comp);
    free(comp);
}

void nema_completion_reset(nema_completion *comp)
{
    if (comp == NULL) return;
    nema_completion_cancel(comp);
    memset(comp->entries, 0, sizeof(comp->entries));
    comp->count = 0u;
    comp->active_count = 0u;
}

void nema_completion_cancel(nema_completion *comp)
{
    size_t i;
    if (comp == NULL) return;
    for (i = 0u; i < NEMA_COMPLETION_MAX_EVENTS; ++i) {
        if (comp->entries[i].active) {
            (void)semu_scheduler_cancel_owned(comp->scheduler, comp->entries[i].event_id,
                nema_completion_callback, &comp->entries[i]);
            comp->entries[i].active = 0;
        }
    }
    comp->active_count = 0u;
}

int nema_completion_pending(const nema_completion *comp, uint32_t list_id)
{
    size_t i;
    if (comp == NULL) return 0;
    for (i = 0u; i < NEMA_COMPLETION_MAX_EVENTS; ++i) {
        if (comp->entries[i].active && comp->entries[i].list_id == list_id) {
            return 1;
        }
    }
    return 0;
}

void nema_completion_rebind_active(
    nema_completion *comp, semu_scheduler *scheduler, nema_reg_write_fn on_reg_write,
    void *reg_context, nema_irq_fn on_irq, void *irq_context)
{
    size_t index;
    if (comp == NULL) return;
    comp->scheduler = scheduler;
    for (index = 0u; index < NEMA_COMPLETION_MAX_EVENTS; ++index) {
        if (comp->entries[index].active != 0) {
            comp->entries[index].on_reg_write = on_reg_write;
            comp->entries[index].reg_context = reg_context;
            comp->entries[index].on_irq = on_irq;
            comp->entries[index].irq_context = irq_context;
        }
    }
}

semu_status nema_completion_schedule(nema_completion *comp,
    semu_scheduler *scheduler, uint32_t list_id, nema_reg_write_fn on_reg_write,
    void *reg_context, nema_irq_fn on_irq, void *irq_context, semu_error *error)
{
    return nema_completion_schedule_batch(comp, scheduler, &list_id, 1u,
        on_reg_write, reg_context, on_irq, irq_context, error);
}

semu_status nema_completion_schedule_batch(nema_completion *comp,
    semu_scheduler *scheduler, const uint32_t *list_ids, size_t count,
    nema_reg_write_fn on_reg_write, void *reg_context, nema_irq_fn on_irq,
    void *irq_context, semu_error *error)
{
    completion_entry staged[NEMA_COMPLETION_MAX_EVENTS];
    semu_event_request requests[NEMA_COMPLETION_MAX_EVENTS];
    semu_event_id ids[NEMA_COMPLETION_MAX_EVENTS];
    size_t slots[NEMA_COMPLETION_MAX_EVENTS], n = 0u, i, j, free_slot = 0u;
    semu_status status;
    if (comp == NULL || scheduler == NULL || (count != 0u && list_ids == NULL)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "completion: null");
        return SEMU_ERR_ARGUMENT;
    }
    if (count > NEMA_COMPLETION_MAX_EVENTS) {
        semu_error_set(error, SEMU_ERR_RANGE, "completion: batch exceeds bound");
        return SEMU_ERR_RANGE;
    }
    if (comp->active_count != 0u && comp->scheduler != scheduler) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "completion: scheduler ownership mismatch");
        return SEMU_ERR_CONFLICT;
    }
    for (i = 0u; i < count; ++i) {
        if (nema_completion_pending(comp, list_ids[i])) continue;
        for (j = 0u; j < n && staged[j].list_id != list_ids[i]; ++j) {}
        if (j < n) continue;
        while (free_slot < NEMA_COMPLETION_MAX_EVENTS && comp->entries[free_slot].active)
            ++free_slot;
        if (free_slot == NEMA_COMPLETION_MAX_EVENTS) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED, "completion: event budget exhausted");
            return SEMU_ERR_UNSUPPORTED;
        }
        slots[n] = free_slot++;
        staged[n] = (completion_entry){comp, list_ids[i], 0u,
            on_reg_write, reg_context, on_irq, irq_context, 1};
        requests[n] = (semu_event_request){NEMA_COMPLETION_DELAY_NS,
            SEMU_SCHED_EVENT_NEMA_COMPLETION, (uint32_t)slots[n],
            nema_completion_callback, &comp->entries[slots[n]]};
        ++n;
    }
    if (n > SIZE_MAX - comp->count) {
        semu_error_set(error, SEMU_ERR_RANGE, "completion: total count overflow");
        return SEMU_ERR_RANGE;
    }
    status = semu_scheduler_schedule_batch(scheduler, requests, n, ids, error);
    if (status != SEMU_OK) return status;
    for (i = 0u; i < n; ++i) {
        staged[i].event_id = ids[i];
        comp->entries[slots[i]] = staged[i];
    }
    if (n != 0u) comp->scheduler = scheduler;
    comp->count += n;
    comp->active_count += n;
    return SEMU_OK;
}

size_t nema_completion_count(const nema_completion *comp)
{
    return (comp != NULL) ? comp->count : 0u;
}
