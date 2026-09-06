#include "sapporo_cxd5610_internal.h"

semu_status semu_sapporo_cxd5610_snapshot_write(
    const semu_sapporo_cxd5610 *transport, semu_snapshot_writer *writer,
    semu_error *error)
{
    if (transport == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "CXD5610 snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (transport->pending_count > sizeof(transport->pending) ||
        transport->rx_count > sizeof(transport->rx)) {
        semu_error_set(error, SEMU_ERR_RANGE, "CXD5610 snapshot buffer is invalid");
        return SEMU_ERR_RANGE;
    }
    if (semu_snapshot_writer_u32(writer, (uint32_t)transport->pending_count, error) != SEMU_OK ||
        semu_snapshot_writer_bytes(writer, transport->pending,
                                   transport->pending_count, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, (uint32_t)transport->rx_count, error) != SEMU_OK ||
        semu_snapshot_writer_bytes(writer, transport->rx,
                                   transport->rx_count, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, transport->rx_event, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, transport->rx_event_context.kind, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, transport->rx_event_context.generation, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, transport->awake_event, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, transport->awake_event_context.kind, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, transport->awake_event_context.generation, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, transport->awake_stage, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, transport->generation, error) != SEMU_OK)
        return error->code;
    return SEMU_OK;
}

semu_status semu_sapporo_cxd5610_snapshot_read(
    semu_sapporo_cxd5610 *transport, semu_snapshot_reader *reader,
    semu_error *error)
{
    semu_sapporo_cxd5610 candidate;
    uint32_t pending_count, rx_count;
    if (transport == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "CXD5610 snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *transport;
    if (semu_snapshot_reader_u32(reader, &pending_count, error) != SEMU_OK ||
        pending_count > sizeof(candidate.pending) ||
        semu_snapshot_reader_bytes(reader, candidate.pending, pending_count, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &rx_count, error) != SEMU_OK ||
        rx_count > sizeof(candidate.rx) ||
        semu_snapshot_reader_bytes(reader, candidate.rx, rx_count, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &candidate.rx_event, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &candidate.rx_event_context.kind, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &candidate.rx_event_context.generation, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &candidate.awake_event, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &candidate.awake_event_context.kind, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &candidate.awake_event_context.generation, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &candidate.awake_stage, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &candidate.generation, error) != SEMU_OK)
        return error->code;
    if ((candidate.rx_event_context.kind != 0u &&
         candidate.rx_event_context.kind != 1u) ||
        candidate.awake_event_context.kind > 3u || candidate.awake_stage > 2u ||
        candidate.generation == 0u ||
        (candidate.rx_event == 0u && rx_count != 0u) ||
        (candidate.rx_event != 0u &&
         (rx_count == 0u ||
          candidate.rx_event_context.kind != 1u ||
          candidate.rx_event_context.generation != candidate.generation)) ||
        (candidate.awake_event == 0u && candidate.awake_stage != 0u) ||
        (candidate.awake_event != 0u &&
         (candidate.awake_event_context.kind < 2u ||
          candidate.awake_event_context.kind > 3u ||
          candidate.awake_event_context.generation != candidate.generation ||
          (candidate.awake_stage == 1u &&
           candidate.awake_event_context.kind != 2u) ||
          (candidate.awake_stage == 2u &&
           candidate.awake_event_context.kind != 3u) ||
          candidate.awake_stage == 0u))) {
        semu_error_set(error, SEMU_ERR_FORMAT, "invalid CXD5610 snapshot event state");
        return SEMU_ERR_FORMAT;
    }
    candidate.pending_count = pending_count;
    candidate.rx_count = rx_count;
    candidate.rx_event_context.transport = transport;
    candidate.awake_event_context.transport = transport;
    *transport = candidate;
    return SEMU_OK;
}

semu_status semu_sapporo_cxd5610_snapshot_resolve_event(
    semu_sapporo_cxd5610 *transport, uint32_t subject,
    semu_event_callback *callback, void **context, semu_error *error)
{
    if (transport == NULL || callback == NULL || context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "CXD5610 snapshot event arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (subject == 0u && transport->rx_event != 0u) {
        *callback = semu_sapporo_cxd5610_scheduled_event;
        *context = &transport->rx_event_context;
        return SEMU_OK;
    }
    if ((subject == 2u || subject == 3u) && transport->awake_event != 0u &&
        transport->awake_event_context.kind == subject) {
        *callback = semu_sapporo_cxd5610_scheduled_event;
        *context = &transport->awake_event_context;
        return SEMU_OK;
    }
    semu_error_set(error, SEMU_ERR_CONFLICT, "CXD5610 snapshot event is not present");
    return SEMU_ERR_CONFLICT;
}

semu_status semu_sapporo_cxd5610_snapshot_event_id_matches(
    const semu_sapporo_cxd5610 *transport, uint32_t subject,
    semu_event_id event_id, semu_error *error)
{
    if (transport != NULL &&
        ((subject == 0u && transport->rx_event == event_id &&
          transport->rx_event != 0u) ||
         ((subject == 2u || subject == 3u) &&
          transport->awake_event == event_id &&
          transport->awake_event_context.kind == subject &&
          transport->awake_event != 0u)))
        return SEMU_OK;
    semu_error_set(error, SEMU_ERR_FORMAT,
                   "CXD5610 snapshot event identity does not match device");
    return SEMU_ERR_FORMAT;
}

static int has_scheduler_event(const semu_scheduled_event_state *events,
                               size_t count, uint32_t kind, uint32_t subject,
                               semu_event_id event_id)
{
    size_t index;
    for (index = 0u; index < count; ++index) {
        if (events[index].kind == kind && events[index].subject == subject &&
            events[index].id == event_id)
            return 1;
    }
    return 0;
}

semu_status semu_sapporo_cxd5610_snapshot_event_links_match(
    const semu_sapporo_cxd5610 *transport,
    const semu_scheduled_event_state *events, size_t count,
    semu_error *error)
{
    if (transport == NULL || (events == NULL && count != 0u)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "CXD5610 snapshot event linkage arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (transport->rx_event != 0u &&
        !has_scheduler_event(events, count, SEMU_SCHED_EVENT_CXD_RX, 0u,
                             transport->rx_event)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "CXD5610 RX state has no scheduler event");
        return SEMU_ERR_FORMAT;
    }
    if (transport->awake_event != 0u &&
        !has_scheduler_event(events, count, SEMU_SCHED_EVENT_CXD_AWAKE,
                             transport->awake_event_context.kind,
                             transport->awake_event)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "CXD5610 awake state has no scheduler event");
        return SEMU_ERR_FORMAT;
    }
    return SEMU_OK;
}
