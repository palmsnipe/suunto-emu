#ifndef SEMU_SAPPORO_CXD5610_H
#define SEMU_SAPPORO_CXD5610_H

#include <stddef.h>
#include <stdint.h>

#include "semu/peripheral.h"
#include "semu/scheduler.h"
#include "../core/snapshot_io.h"

#define SEMU_SAPPORO_CXD5610_MAX_REQUEST 9u
#define SEMU_SAPPORO_CXD5610_MAX_RX 64u
#define SEMU_SAPPORO_CXD5610_AWAKE_PULSE_NS UINT64_C(1000000)

typedef struct semu_sapporo_cxd5610 semu_sapporo_cxd5610;

typedef enum semu_sapporo_cxd5610_trace_direction {
    SEMU_SAPPORO_CXD5610_TX = 0,
    SEMU_SAPPORO_CXD5610_RX
} semu_sapporo_cxd5610_trace_direction;

typedef void (*semu_sapporo_cxd5610_byte_fn)(
    void *context, uint8_t value, uint64_t virtual_time_ns);
typedef void (*semu_sapporo_cxd5610_trace_fn)(
    void *context, semu_sapporo_cxd5610_trace_direction direction,
    uint8_t value, uint64_t virtual_time_ns);
typedef semu_transaction_result (*semu_sapporo_cxd5610_exchange_fn)(
    void *context, const uint8_t *request, size_t count,
    semu_sapporo_cxd5610 *transport, semu_error *error);

semu_sapporo_cxd5610 *semu_sapporo_cxd5610_create(
    semu_scheduler *scheduler, semu_peripheral_signal_fn awake_signal,
    void *awake_context, semu_sapporo_cxd5610_byte_fn rx_sink,
    void *rx_context, semu_sapporo_cxd5610_trace_fn trace,
    void *trace_context, semu_error *error);
void semu_sapporo_cxd5610_destroy(semu_sapporo_cxd5610 *transport);
void semu_sapporo_cxd5610_reset(void *context);

semu_serial_endpoint semu_sapporo_cxd5610_endpoint(
    semu_sapporo_cxd5610 *transport);
void semu_sapporo_cxd5610_set_exchange(
    semu_sapporo_cxd5610 *transport, semu_sapporo_cxd5610_exchange_fn exchange,
    void *exchange_context);
void semu_sapporo_cxd5610_set_rx_sink(
    semu_sapporo_cxd5610 *transport, semu_sapporo_cxd5610_byte_fn rx_sink,
    void *rx_context);
void semu_sapporo_cxd5610_set_awake_signal(
    semu_sapporo_cxd5610 *transport, semu_peripheral_signal_fn signal,
    void *context);
semu_status semu_sapporo_cxd5610_inject_rx(
    semu_sapporo_cxd5610 *transport, const uint8_t *bytes, size_t count,
    semu_error *error);
semu_status semu_sapporo_cxd5610_inject_rx_after(
    semu_sapporo_cxd5610 *transport, const uint8_t *bytes, size_t count,
    uint64_t delay_ns, semu_error *error);
semu_status semu_sapporo_cxd5610_pulse_awake_after(
    semu_sapporo_cxd5610 *transport, uint64_t delay_ns, semu_error *error);
semu_status semu_sapporo_cxd5610_snapshot_write(
    const semu_sapporo_cxd5610 *transport, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_sapporo_cxd5610_snapshot_read(
    semu_sapporo_cxd5610 *transport, semu_snapshot_reader *reader,
    semu_error *error);
semu_status semu_sapporo_cxd5610_snapshot_resolve_event(
    semu_sapporo_cxd5610 *transport, uint32_t subject,
    semu_event_callback *callback, void **context, semu_error *error);

#endif
