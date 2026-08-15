/*
 * Nema completion events (ticket 506).
 * Schedules evidenced command-list completion with 100 µs delay.
 */

#include "nema_completion.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t list_id;
    semu_event_id event_id;
    nema_reg_write_fn on_reg_write;
    void *reg_context;
    nema_irq_fn on_irq;
    void *irq_context;
    int active;
} completion_entry;

struct nema_completion {
    completion_entry entries[NEMA_COMPLETION_MAX_EVENTS];
    size_t count;
};

static void completion_callback(void *context, uint64_t now_ns)
{
    completion_entry *e = (completion_entry *)context;
    (void)now_ns;
    if (e->on_reg_write != NULL) {
        e->on_reg_write(e->reg_context, NEMA_REG_CLID, e->list_id);
        e->on_reg_write(e->reg_context, NEMA_REG_INTERRUPT, 1u);
    }
    if (e->on_irq != NULL) {
        e->on_irq(e->irq_context, NEMA_COMPLETION_IRQ_LINE, 1);
    }
    e->active = 0;
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
    /* Active entries have dangling scheduler references; callers
     * must cancel or reset before destroy. */
    free(comp);
}

void nema_completion_reset(nema_completion *comp)
{
    if (comp == NULL) return;
    memset(comp->entries, 0, sizeof(comp->entries));
    comp->count = 0u;
}

void nema_completion_cancel(nema_completion *comp)
{
    size_t i;
    if (comp == NULL) return;
    for (i = 0u; i < NEMA_COMPLETION_MAX_EVENTS; ++i) {
        if (comp->entries[i].active) {
            comp->entries[i].active = 0;
        }
    }
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

semu_status nema_completion_schedule(nema_completion *comp,
                                     semu_scheduler *scheduler,
                                     uint32_t list_id,
                                     nema_reg_write_fn on_reg_write,
                                     void *reg_context,
                                     nema_irq_fn on_irq,
                                     void *irq_context,
                                     semu_error *error)
{
    size_t i, free_slot;
    semu_status st;

    if (comp == NULL || scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "completion: null");
        return SEMU_ERR_ARGUMENT;
    }

    /* No-op if already pending for this list ID */
    if (nema_completion_pending(comp, list_id)) {
        return SEMU_OK;
    }

    free_slot = NEMA_COMPLETION_MAX_EVENTS;
    for (i = 0u; i < NEMA_COMPLETION_MAX_EVENTS; ++i) {
        if (!comp->entries[i].active) {
            free_slot = i;
            break;
        }
    }
    if (free_slot >= NEMA_COMPLETION_MAX_EVENTS) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "completion: event budget exhausted");
        return SEMU_ERR_UNSUPPORTED;
    }

    comp->entries[free_slot].list_id = list_id;
    comp->entries[free_slot].on_reg_write = on_reg_write;
    comp->entries[free_slot].reg_context = reg_context;
    comp->entries[free_slot].on_irq = on_irq;
    comp->entries[free_slot].irq_context = irq_context;
    comp->entries[free_slot].active = 1;

    st = semu_scheduler_schedule(scheduler,
                                NEMA_COMPLETION_DELAY_NS,
                                completion_callback,
                                &comp->entries[free_slot],
                                &comp->entries[free_slot].event_id,
                                error);
    if (st != SEMU_OK) {
        comp->entries[free_slot].active = 0;
        return st;
    }

    ++comp->count;
    return SEMU_OK;
}

size_t nema_completion_count(const nema_completion *comp)
{
    return (comp != NULL) ? comp->count : 0u;
}
