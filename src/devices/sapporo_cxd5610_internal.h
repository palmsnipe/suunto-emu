#ifndef SEMU_SAPPORO_CXD5610_INTERNAL_H
#define SEMU_SAPPORO_CXD5610_INTERNAL_H

#include "sapporo_cxd5610.h"

typedef struct semu_cxd_event semu_cxd_event;
struct semu_cxd_event {
    semu_sapporo_cxd5610 *transport;
    uint8_t kind;
    uint64_t generation;
};
struct semu_sapporo_cxd5610 {
    semu_scheduler *scheduler;
    semu_peripheral_signal_fn awake_signal;
    void *awake_context;
    semu_sapporo_cxd5610_byte_fn rx_sink;
    void *rx_context;
    semu_sapporo_cxd5610_trace_fn trace;
    void *trace_context;
    semu_sapporo_cxd5610_exchange_fn exchange;
    void *exchange_context;
    uint8_t pending[SEMU_SAPPORO_CXD5610_MAX_REQUEST];
    size_t pending_count;
    uint8_t rx[SEMU_SAPPORO_CXD5610_MAX_RX];
    size_t rx_count;
    semu_event_id rx_event; semu_cxd_event rx_event_context;
    semu_event_id awake_event; semu_cxd_event awake_event_context;
    uint8_t awake_stage;
    uint64_t generation;
};

void semu_sapporo_cxd5610_scheduled_event(void *context, uint64_t now_ns);

#endif
