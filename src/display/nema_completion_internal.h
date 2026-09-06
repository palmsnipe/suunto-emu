#ifndef SEMU_NEMA_COMPLETION_INTERNAL_H
#define SEMU_NEMA_COMPLETION_INTERNAL_H
#include "nema_completion.h"
typedef struct {
    struct nema_completion *owner;
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
    size_t active_count;
    semu_scheduler *scheduler;
};

void nema_completion_callback(void *context, uint64_t now_ns);
#endif
