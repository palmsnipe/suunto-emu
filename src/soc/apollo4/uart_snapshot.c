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

static int valid_uart3_state(const semu_apollo4_uart *uart)
{
    const uint32_t latched = SEMU_APOLLO4_UART_INTERRUPT_TIMEOUT |
                             SEMU_APOLLO4_UART_INTERRUPT_TX_COMPLETE;
    int timeout_active = uart->timeout_event != 0u ||
        (uart->latched_interrupts & SEMU_APOLLO4_UART_INTERRUPT_TIMEOUT) != 0u;
    return (uart->cts == 0 || uart->cts == 1) &&
        (uart->latched_interrupts & ~latched) == 0u &&
        (!timeout_active || (uart->rx_count != 0u &&
          (uart->control & 0x201u) == 0x201u &&
          (uart->interrupt_mask & SEMU_APOLLO4_UART_INTERRUPT_TIMEOUT) != 0u)) &&
        semu_apollo4_uart3_register_valid(SEMU_APOLLO4_UART_CONTROL, uart->control) &&
        semu_apollo4_uart3_register_valid(SEMU_APOLLO4_UART_INTEGER_BAUD, uart->integer_baud) &&
        semu_apollo4_uart3_register_valid(SEMU_APOLLO4_UART_FRACTIONAL_BAUD, uart->fractional_baud) &&
        semu_apollo4_uart3_register_valid(SEMU_APOLLO4_UART_LINE_CONTROL, uart->line_control) &&
        semu_apollo4_uart3_register_valid(SEMU_APOLLO4_UART_FIFO_LEVEL, uart->fifo_level) &&
        semu_apollo4_uart3_register_valid(SEMU_APOLLO4_UART_INTERRUPT_MASK, uart->interrupt_mask) &&
        (uart->tx_count == 0u || (uart->control & 0x101u) == 0x101u);
}

static int valid_uart1_registers(const semu_apollo4_uart *uart)
{
    static const uint32_t controls[] = { 0u, 0x8u, 0x18u, 0x19u, 0x219u, 0x319u };
    static const uint32_t line_controls[] = { 0u, 0x10u, 0x70u };
    static const uint32_t fifo_levels[] = { 0u, 0x10u, 0x12u };
    return allowed_value(uart->control, controls, SEMU_ARRAY_LEN(controls)) &&
        uart->integer_baud <= 1u &&
        (uart->fractional_baud == 0u || uart->fractional_baud == 0x28u) &&
        allowed_value(uart->line_control, line_controls, SEMU_ARRAY_LEN(line_controls)) &&
        allowed_value(uart->fifo_level, fifo_levels, SEMU_ARRAY_LEN(fifo_levels)) &&
        (uart->interrupt_mask == 0u || uart->interrupt_mask == 0x51u);
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
    if (count > uart->capacity) {
        semu_error_set(error, SEMU_ERR_RANGE, "too many UART RX events");
        return SEMU_ERR_RANGE;
    }
    if (semu_snapshot_writer_u8(writer, (uint8_t)(uart->endpoint_attached != 0), error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)(uart->irq_level != 0), error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, (uint32_t)uart->rx_head, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, (uint32_t)uart->rx_count, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, (uint32_t)uart->rx_reserved, error) != SEMU_OK ||
        semu_snapshot_writer_bytes(writer, uart->rx_fifo, uart->capacity, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, (uint32_t)uart->tx_head, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, (uint32_t)uart->tx_count, error) != SEMU_OK ||
        semu_snapshot_writer_bytes(writer, uart->tx_fifo, uart->capacity, error) != SEMU_OK ||
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
    if (uart->irq == SEMU_APOLLO4_UART3_IRQ &&
        (semu_snapshot_writer_u8(writer, (uint8_t)uart->cts, error) != SEMU_OK ||
         semu_snapshot_writer_u32(writer, uart->latched_interrupts, error) != SEMU_OK ||
         semu_snapshot_writer_u64(writer, uart->timeout_event, error) != SEMU_OK))
        return error->code;
    for (event = uart->rx_events; event != NULL; event = event->next) {
        if (event->count == 0u || event->count > uart->capacity ||
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
    uint8_t cts = 0u;
    uint32_t rx_head, rx_count, rx_reserved, tx_head, tx_count, count;
    size_t reserved = 0u;
    size_t index;
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
        semu_snapshot_reader_bytes(reader, candidate.rx_fifo, uart->capacity, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &tx_head, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &tx_count, error) != SEMU_OK ||
        semu_snapshot_reader_bytes(reader, candidate.tx_fifo, uart->capacity, error) != SEMU_OK ||
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
    if (uart->irq == SEMU_APOLLO4_UART3_IRQ &&
        (semu_snapshot_reader_u8(reader, &cts, error) != SEMU_OK ||
         semu_snapshot_reader_u32(reader, &candidate.latched_interrupts, error) != SEMU_OK ||
         semu_snapshot_reader_u64(reader, &candidate.timeout_event, error) != SEMU_OK))
        return error->code;
    candidate.cts = cts;
    candidate.rx_count = rx_count;
    candidate.tx_count = tx_count;
    if (attached > 1u || irq_level > 1u || candidate.next_rx_slot == 0u ||
        (attached != 0u && candidate.endpoint.transmit == NULL) ||
        rx_head >= uart->capacity ||
        tx_head >= uart->capacity ||
        rx_count > uart->capacity ||
        tx_count > uart->capacity ||
        (attached == 0u && tx_count != 0u) ||
        rx_reserved > uart->capacity ||
        rx_reserved > uart->capacity - rx_count ||
        count > uart->capacity ||
        (uart->irq == SEMU_APOLLO4_UART3_IRQ ?
            !valid_uart3_state(&candidate) : !valid_uart1_registers(&candidate)) ||
        (irq_level != 0u) != (semu_apollo4_uart_masked_interrupts(&candidate) != 0u) ||
        (candidate.tx_event != 0u &&
         (attached == 0u || tx_count == 0u))) {
        semu_error_set(error, SEMU_ERR_FORMAT, "invalid UART snapshot FIFO state");
        return SEMU_ERR_FORMAT;
    }
    candidate.endpoint_attached = attached;
    candidate.irq_level = irq_level;
    candidate.rx_head = rx_head;
    candidate.rx_reserved = rx_reserved;
    candidate.tx_head = tx_head;
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
            size == 0u || size > uart->capacity ||
            reserved > uart->capacity - size) {
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
    if (kind == SEMU_SCHED_EVENT_UART3_TIMEOUT && subject == 0u && uart->timeout_event != 0u) {
        *callback = semu_apollo4_uart_timeout_event;
        *context = uart;
        return SEMU_OK;
    }
    if ((kind == SEMU_SCHED_EVENT_UART_TX || kind == SEMU_SCHED_EVENT_UART3_TX) &&
        subject == 0u && uart->tx_event != 0u) {
        *callback = semu_apollo4_uart_tx_event;
        *context = uart;
        return SEMU_OK;
    }
    if (kind != SEMU_SCHED_EVENT_UART_RX && kind != SEMU_SCHED_EVENT_UART3_RX) {
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
