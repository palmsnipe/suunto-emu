#include "sapporo_cxd5610.h"
#include <stdlib.h>
#include <string.h>
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
static semu_status require_transport(const semu_sapporo_cxd5610 *transport,
                                     semu_error *error)
{
    if (transport == NULL || transport->scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "CXD5610 transport and scheduler required");
        return SEMU_ERR_ARGUMENT;
    }
    return SEMU_OK;
}
static void trace_byte(semu_sapporo_cxd5610 *transport,
                       semu_sapporo_cxd5610_trace_direction direction,
                       uint8_t value)
{
    if (transport->trace != NULL) {
        transport->trace(transport->trace_context, direction, value,
                         semu_scheduler_now(transport->scheduler));
    }
}
static void set_awake(semu_sapporo_cxd5610 *transport, int level)
{
    if (transport->awake_signal != NULL) transport->awake_signal(
        transport->awake_context, 0u, level);
}
static void inject_now(semu_sapporo_cxd5610 *transport, const uint8_t *bytes,
                       size_t count)
{
    uint64_t generation = transport->generation;
    size_t index;
    for (index = 0u; index < count; ++index) {
        if (transport->generation != generation) break;
        trace_byte(transport, SEMU_SAPPORO_CXD5610_RX, bytes[index]);
        if (transport->generation != generation) break;
        if (transport->rx_sink != NULL) {
            transport->rx_sink(transport->rx_context, bytes[index],
                               semu_scheduler_now(transport->scheduler));
        }
    }
}
static void scheduled_event(void *context, uint64_t now_ns)
{
    semu_cxd_event *event = (semu_cxd_event *)context;
    semu_sapporo_cxd5610 *transport = event->transport;
    uint64_t generation = event->generation;
    (void)now_ns;
    if (transport->generation != generation) return;
    if (event->kind == 1u) {
        transport->rx_event = 0u;
        inject_now(transport, transport->rx, transport->rx_count);
        if (transport->generation != generation) return;
        transport->rx_count = 0u;
    } else if (event->kind == 2u) {
        transport->awake_event = 0u;
        transport->awake_stage = 2u;
        set_awake(transport, 1);
        if (transport->generation != generation) return;
        event->kind = 3u;
        if (semu_scheduler_schedule(transport->scheduler,
                                     SEMU_SAPPORO_CXD5610_AWAKE_PULSE_NS,
                                     scheduled_event, event,
                                     &transport->awake_event, NULL) != SEMU_OK) {
            transport->awake_stage = 0u;
            set_awake(transport, 0);
        }
        return;
    } else {
        transport->awake_event = 0u;
        transport->awake_stage = 0u;
        set_awake(transport, 0);
    }
}
static semu_transaction_result transfer(void *context,
                                        semu_serial_transaction *transaction,
                                        semu_error *error)
{
    semu_sapporo_cxd5610 *transport =
        (semu_sapporo_cxd5610 *)context;
    uint8_t candidate[SEMU_SAPPORO_CXD5610_MAX_REQUEST];
    size_t candidate_count;
    size_t index;
    semu_transaction_result result;
    if (require_transport(transport, error) != SEMU_OK || transaction == NULL ||
        transaction->address != 0u || transaction->chip_select != 0u ||
        transaction->tx == NULL || transaction->tx_size == 0u ||
        transaction->rx_size != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "CXD5610 requires a nonempty UART TX stream");
        return SEMU_TRANSACTION_REFUSE;
    }
    candidate_count = transport->pending_count;
    memcpy(candidate, transport->pending, candidate_count);
    for (index = 0u; index < transaction->tx_size; ++index) {
        if (candidate_count >= sizeof(candidate)) {
            semu_error_set(error, SEMU_ERR_RANGE,
                           "CXD5610 request exceeds the evidenced bound");
            return SEMU_TRANSACTION_REFUSE;
        }
        candidate[candidate_count++] = transaction->tx[index];
        if (candidate_count >= 2u &&
            candidate[candidate_count - 2u] == '\r' &&
            candidate[candidate_count - 1u] == '\n') {
            if (index + 1u != transaction->tx_size || candidate[0] != '@') {
                semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                               "CXD5610 request framing is not evidenced");
                return SEMU_TRANSACTION_REFUSE;
            }
            if (transport->exchange == NULL) {
                semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                               "CXD5610 request has no response fixture");
                return SEMU_TRANSACTION_REFUSE;
            }
            memcpy(transport->pending, candidate, candidate_count);
            transport->pending_count = 0u;
            for (size_t byte = 0u; byte < transaction->tx_size; ++byte) {
                trace_byte(transport, SEMU_SAPPORO_CXD5610_TX,
                           transaction->tx[byte]);
            }
            result = transport->exchange(transport->exchange_context,
                                          candidate, candidate_count,
                                          transport, error);
            if (result == SEMU_TRANSACTION_OK) {
                semu_error_clear(error);
            }
            return result;
        }
    }
    memcpy(transport->pending, candidate, candidate_count);
    transport->pending_count = candidate_count;
    for (index = 0u; index < transaction->tx_size; ++index) {
        trace_byte(transport, SEMU_SAPPORO_CXD5610_TX, transaction->tx[index]);
    }
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}
semu_sapporo_cxd5610 *semu_sapporo_cxd5610_create(
    semu_scheduler *scheduler, semu_peripheral_signal_fn awake_signal,
    void *awake_context, semu_sapporo_cxd5610_byte_fn rx_sink,
    void *rx_context, semu_sapporo_cxd5610_trace_fn trace,
    void *trace_context, semu_error *error)
{
    semu_sapporo_cxd5610 *transport;
    if (scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "CXD5610 requires a scheduler");
        return NULL;
    }
    transport = (semu_sapporo_cxd5610 *)calloc(1u, sizeof(*transport));
    if (transport == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate CXD5610 transport");
        return NULL;
    }
    transport->scheduler = scheduler;
    transport->awake_signal = awake_signal;
    transport->awake_context = awake_context;
    transport->rx_sink = rx_sink;
    transport->rx_context = rx_context;
    transport->trace = trace;
    transport->trace_context = trace_context;
    transport->generation = 1u;
    transport->rx_event_context.transport = transport;
    transport->awake_event_context.transport = transport;
    set_awake(transport, 0);
    semu_error_clear(error);
    return transport;
}
void semu_sapporo_cxd5610_reset(void *context)
{
    semu_sapporo_cxd5610 *transport =
        (semu_sapporo_cxd5610 *)context;
    if (transport == NULL) return;
    ++transport->generation;
    if (transport->generation == 0u) transport->generation = 1u;
    if (transport->rx_event != 0u) {
        (void)semu_scheduler_cancel(transport->scheduler, transport->rx_event);
        transport->rx_event = 0u;
    }
    if (transport->awake_event != 0u) {
        (void)semu_scheduler_cancel(transport->scheduler,
                                     transport->awake_event);
        transport->awake_event = 0u;
    }
    transport->pending_count = 0u;
    transport->rx_count = 0u;
    transport->awake_stage = 0u;
    set_awake(transport, 0);
}
void semu_sapporo_cxd5610_destroy(semu_sapporo_cxd5610 *transport)
{
    if (transport == NULL) return;
    semu_sapporo_cxd5610_reset(transport);
    free(transport);
}
semu_serial_endpoint semu_sapporo_cxd5610_endpoint(
    semu_sapporo_cxd5610 *transport)
{
    return (semu_serial_endpoint){"sapporo-cxd5610", transfer, transport};
}
void semu_sapporo_cxd5610_set_exchange(
    semu_sapporo_cxd5610 *transport, semu_sapporo_cxd5610_exchange_fn exchange,
    void *exchange_context)
{
    if (transport != NULL) { transport->exchange = exchange;
        transport->exchange_context = exchange_context; }
}
void semu_sapporo_cxd5610_set_rx_sink(
    semu_sapporo_cxd5610 *transport, semu_sapporo_cxd5610_byte_fn rx_sink,
    void *rx_context)
{
    if (transport != NULL) {
        transport->rx_sink = rx_sink;
        transport->rx_context = rx_context;
    }
}
void semu_sapporo_cxd5610_set_awake_signal(
    semu_sapporo_cxd5610 *transport, semu_peripheral_signal_fn signal,
    void *context)
{
    if (transport != NULL) {
        transport->awake_signal = signal;
        transport->awake_context = context;
        set_awake(transport, 0);
    }
}
semu_status semu_sapporo_cxd5610_inject_rx(
    semu_sapporo_cxd5610 *transport, const uint8_t *bytes, size_t count,
    semu_error *error)
{
    if (require_transport(transport, error) != SEMU_OK ||
        (bytes == NULL || count == 0u) || count > SEMU_SAPPORO_CXD5610_MAX_RX ||
        transport->rx_sink == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "CXD5610 RX injection is invalid or detached");
        return SEMU_ERR_ARGUMENT;
    }
    inject_now(transport, bytes, count);
    semu_error_clear(error);
    return SEMU_OK;
}
semu_status semu_sapporo_cxd5610_inject_rx_after(
    semu_sapporo_cxd5610 *transport, const uint8_t *bytes, size_t count,
    uint64_t delay_ns, semu_error *error)
{
    if (require_transport(transport, error) != SEMU_OK ||
        (bytes == NULL || count == 0u) || count > sizeof(transport->rx) ||
        transport->rx_sink == NULL || transport->rx_event != 0u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "CXD5610 delayed RX injection is invalid");
        return SEMU_ERR_ARGUMENT;
    }
    semu_cxd_event *event = &transport->rx_event_context;
    event->kind = 1u;
    event->generation = transport->generation;
    memcpy(transport->rx, bytes, count);
    transport->rx_count = count;
    if (semu_scheduler_schedule(transport->scheduler, delay_ns,
                                 scheduled_event, event,
                                 &transport->rx_event, error) != SEMU_OK) {
        transport->rx_count = 0u;
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    semu_error_clear(error);
    return SEMU_OK;
}
semu_status semu_sapporo_cxd5610_pulse_awake_after(
    semu_sapporo_cxd5610 *transport, uint64_t delay_ns, semu_error *error)
{
    if (require_transport(transport, error) != SEMU_OK ||
        transport->awake_event != 0u || transport->awake_stage != 0u) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "CXD5610 awake pulse is already scheduled");
        return SEMU_ERR_STATE;
    }
    semu_cxd_event *event = &transport->awake_event_context;
    event->kind = 2u;
    event->generation = transport->generation;
    set_awake(transport, 0);
    if (transport->generation != event->generation) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "CXD5610 reset during awake scheduling");
        return SEMU_ERR_STATE;
    }
    if (semu_scheduler_schedule(transport->scheduler, delay_ns,
                                 scheduled_event, event,
                                 &transport->awake_event, error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    transport->awake_stage = 1u;
    semu_error_clear(error);
    return SEMU_OK;
}
