#include "uart_internal.h"

#include "../../core/scheduler_internal.h"

#include <stdlib.h>

static int allowed_value(uint32_t value, const uint32_t *values, size_t count)
{
    size_t index;
    for (index = 0u; index < count; ++index) {
        if (values[index] == value) return 1;
    }
    return 0;
}

static size_t rx_threshold(uint32_t fifo_level)
{
    static const size_t thresholds[] = { 2u, 4u, 8u, 12u, 14u };
    unsigned selection = (fifo_level >> 3u) & 7u;
    return selection < SEMU_ARRAY_LEN(thresholds) ? thresholds[selection] : 2u;
}

static void free_events(rx_event *event)
{
    while (event != NULL) {
        rx_event *next = event->next;
        free(event);
        event = next;
    }
}

static size_t event_count(const rx_event *event)
{
    size_t count = 0u;
    while (event != NULL) {
        ++count;
        event = event->next;
    }
    return count;
}

semu_status semu_apollo4_uart_snapshot_write(
    const semu_apollo4_uart *uart, semu_snapshot_writer *writer,
    semu_error *error)
{
    const rx_event *event;
    size_t count;
    if (uart == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "UART snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (uart->tx_completion != NULL) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "UART snapshot cannot retain TX callback");
        return SEMU_ERR_UNSUPPORTED;
    }
    count = event_count(uart->rx_events);
    if (count > SEMU_APOLLO4_UART_FIFO_CAPACITY) {
        semu_error_set(error, SEMU_ERR_RANGE, "too many UART RX events");
        return SEMU_ERR_RANGE;
    }
    if (semu_snapshot_writer_u8(writer, (uint8_t)(uart->endpoint_attached != 0), error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)(uart->irq_level != 0), error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, (uint32_t)uart->rx_head, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, (uint32_t)uart->rx_count, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, (uint32_t)uart->rx_reserved, error) != SEMU_OK ||
        semu_snapshot_writer_bytes(writer, uart->rx_fifo, sizeof(uart->rx_fifo), error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, (uint32_t)uart->tx_head, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, (uint32_t)uart->tx_count, error) != SEMU_OK ||
        semu_snapshot_writer_bytes(writer, uart->tx_fifo, sizeof(uart->tx_fifo), error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, uart->tx_event, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, uart->control, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, uart->integer_baud, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, uart->fractional_baud, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, uart->line_control, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, uart->fifo_level, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, uart->interrupt_mask, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, uart->next_rx_slot, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, (uint32_t)count, error) != SEMU_OK)
        return error->code;
    for (event = uart->rx_events; event != NULL; event = event->next) {
        if (event->count == 0u || event->count > SEMU_APOLLO4_UART_FIFO_CAPACITY ||
            semu_snapshot_writer_u64(writer, event->id, error) != SEMU_OK ||
            semu_snapshot_writer_u32(writer, event->slot, error) != SEMU_OK ||
            semu_snapshot_writer_u32(writer, (uint32_t)event->count, error) != SEMU_OK ||
            semu_snapshot_writer_bytes(writer, event->bytes, event->count, error) != SEMU_OK) {
            if (error->code == SEMU_OK)
                semu_error_set(error, SEMU_ERR_FORMAT, "invalid UART RX event");
            return error->code;
        }
    }
    return SEMU_OK;
}

semu_status semu_apollo4_uart_snapshot_read(
    semu_apollo4_uart *uart, semu_snapshot_reader *reader,
    semu_error *error)
{
    semu_apollo4_uart candidate;
    rx_event *head = NULL;
    rx_event **tail = &head;
    uint8_t attached;
    uint8_t irq_level;
    uint32_t rx_head, rx_count, rx_reserved, tx_head, tx_count, count;
    size_t reserved = 0u;
    size_t index;
    static const uint32_t controls[] = { 0u, 0x8u, 0x18u, 0x19u,
                                         0x219u, 0x319u };
    static const uint32_t line_controls[] = { 0u, 0x10u, 0x70u };
    static const uint32_t fifo_levels[] = { 0u, 0x10u, 0x12u };
    static const uint32_t masks[] = { 0u, 0x51u };
    if (uart == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "UART snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *uart;
    if (semu_snapshot_reader_u8(reader, &attached, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &irq_level, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &rx_head, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &rx_count, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &rx_reserved, error) != SEMU_OK ||
        semu_snapshot_reader_bytes(reader, candidate.rx_fifo, sizeof(candidate.rx_fifo), error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &tx_head, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &tx_count, error) != SEMU_OK ||
        semu_snapshot_reader_bytes(reader, candidate.tx_fifo, sizeof(candidate.tx_fifo), error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &candidate.tx_event, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.control, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.integer_baud, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.fractional_baud, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.line_control, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.fifo_level, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.interrupt_mask, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.next_rx_slot, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &count, error) != SEMU_OK)
        return error->code;
    if (attached > 1u || irq_level > 1u || candidate.next_rx_slot == 0u ||
        rx_head >= SEMU_APOLLO4_UART_FIFO_CAPACITY ||
        tx_head >= SEMU_APOLLO4_UART_FIFO_CAPACITY ||
        rx_count > SEMU_APOLLO4_UART_FIFO_CAPACITY ||
        tx_count > SEMU_APOLLO4_UART_FIFO_CAPACITY ||
        rx_reserved > SEMU_APOLLO4_UART_FIFO_CAPACITY ||
        rx_reserved > SEMU_APOLLO4_UART_FIFO_CAPACITY - rx_count ||
        count > SEMU_APOLLO4_UART_FIFO_CAPACITY ||
        !allowed_value(candidate.control, controls, SEMU_ARRAY_LEN(controls)) ||
        !allowed_value(candidate.integer_baud, (uint32_t[]){ 0u, 1u }, 2u) ||
        !allowed_value(candidate.fractional_baud, (uint32_t[]){ 0u, 0x28u }, 2u) ||
        !allowed_value(candidate.line_control, line_controls,
                       SEMU_ARRAY_LEN(line_controls)) ||
        !allowed_value(candidate.fifo_level, fifo_levels,
                       SEMU_ARRAY_LEN(fifo_levels)) ||
        !allowed_value(candidate.interrupt_mask, masks, SEMU_ARRAY_LEN(masks)) ||
        (irq_level != 0u) !=
            ((candidate.control & 0x201u) == 0x201u &&
             rx_count >= rx_threshold(candidate.fifo_level) &&
             (candidate.interrupt_mask & SEMU_APOLLO4_UART_INTERRUPT_RX) != 0u) ||
        (candidate.tx_event != 0u &&
         (attached == 0u || tx_count == 0u))) {
        semu_error_set(error, SEMU_ERR_FORMAT, "invalid UART snapshot FIFO state");
        return SEMU_ERR_FORMAT;
    }
    candidate.endpoint_attached = attached;
    candidate.irq_level = irq_level;
    candidate.rx_head = rx_head;
    candidate.rx_count = rx_count;
    candidate.rx_reserved = rx_reserved;
    candidate.tx_head = tx_head;
    candidate.tx_count = tx_count;
    candidate.tx_completion = NULL;
    candidate.tx_completion_context = NULL;
    for (index = 0u; index < count; ++index) {
        uint64_t id;
        uint32_t slot, size;
        rx_event *event;
        rx_event *existing;
        if (semu_snapshot_reader_u64(reader, &id, error) != SEMU_OK ||
            semu_snapshot_reader_u32(reader, &slot, error) != SEMU_OK ||
            semu_snapshot_reader_u32(reader, &size, error) != SEMU_OK ||
            id == 0u || slot == 0u || slot >= candidate.next_rx_slot ||
            size == 0u || size > SEMU_APOLLO4_UART_FIFO_CAPACITY ||
            reserved > SEMU_APOLLO4_UART_FIFO_CAPACITY - size) {
            free_events(head);
            if (error->code == SEMU_OK)
                semu_error_set(error, SEMU_ERR_FORMAT, "invalid UART RX event");
            return error->code;
        }
        for (existing = head; existing != NULL; existing = existing->next) {
            if (existing->slot == slot || existing->id == id) {
                free_events(head);
                semu_error_set(error, SEMU_ERR_FORMAT,
                               "duplicate UART RX event identity");
                return SEMU_ERR_FORMAT;
            }
        }
        event = (rx_event *)calloc(1u, offsetof(rx_event, bytes) + size);
        if (event == NULL) {
            free_events(head);
            semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate UART RX event");
            return SEMU_ERR_NOMEM;
        }
        event->uart = uart;
        event->id = id;
        event->slot = slot;
        event->count = size;
        if (semu_snapshot_reader_bytes(reader, event->bytes, size, error) != SEMU_OK) {
            free(event);
            free_events(head);
            return error->code;
        }
        *tail = event;
        tail = &event->next;
        reserved += size;
    }
    if (reserved != rx_reserved) {
        free_events(head);
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "UART RX reservation does not match events");
        return SEMU_ERR_FORMAT;
    }
    free_events(uart->rx_events);
    candidate.rx_events = head;
    *uart = candidate;
    return SEMU_OK;
}

semu_status semu_apollo4_uart_snapshot_resolve_event(
    semu_apollo4_uart *uart, uint32_t kind, uint32_t subject,
    semu_event_callback *callback, void **context, semu_error *error)
{
    rx_event *event;
    if (uart == NULL || callback == NULL || context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "UART snapshot event arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (kind == SEMU_SCHED_EVENT_UART_TX && subject == 0u && uart->tx_event != 0u) {
        *callback = semu_apollo4_uart_tx_event;
        *context = uart;
        return SEMU_OK;
    }
    if (kind != SEMU_SCHED_EVENT_UART_RX) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "UART snapshot event is not present");
        return SEMU_ERR_CONFLICT;
    }
    for (event = uart->rx_events; event != NULL; event = event->next) {
        if (event->slot == subject) {
            *callback = semu_apollo4_uart_rx_event;
            *context = event;
            return SEMU_OK;
        }
    }
    semu_error_set(error, SEMU_ERR_CONFLICT, "UART RX snapshot event is not present");
    return SEMU_ERR_CONFLICT;
}
