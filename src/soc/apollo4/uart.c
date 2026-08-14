#include "uart.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
typedef struct rx_event rx_event;
struct rx_event {
    semu_apollo4_uart *uart;
    semu_event_id id;
    rx_event *next;
    size_t count;
    uint8_t bytes[1];
};
struct semu_apollo4_uart {
    semu_scheduler *scheduler;
    semu_apollo4_uart_endpoint endpoint;
    int endpoint_attached;
    semu_apollo4_uart_irq_fn irq_sink;
    void *irq_context;
    int irq_level;
    uint8_t rx_fifo[SEMU_APOLLO4_UART_FIFO_CAPACITY];
    size_t rx_head;
    size_t rx_count;
    size_t rx_reserved;
    rx_event *rx_events;
    uint8_t tx_fifo[SEMU_APOLLO4_UART_FIFO_CAPACITY];
    size_t tx_head;
    size_t tx_count;
    semu_event_id tx_event;
    semu_apollo4_uart_tx_completion_fn tx_completion;
    void *tx_completion_context;
    uint32_t control;
    uint32_t integer_baud;
    uint32_t fractional_baud;
    uint32_t line_control;
    uint32_t fifo_level;
    uint32_t interrupt_mask;
};
static int allowed_value(uint32_t value, const uint32_t *values, size_t count)
{
    size_t index;
    for (index = 0u; index < count; ++index) {
        if (values[index] == value) return 1;
    }
    return 0;
}
static semu_status invalid(semu_error *error, semu_status status,
                           const char *message)
{
    semu_error_set(error, status, "%s", message);
    return status;
}
static int rx_enabled(const semu_apollo4_uart *uart)
{
    return (uart->control & 0x201u) == 0x201u;
}
static int tx_enabled(const semu_apollo4_uart *uart)
{
    return (uart->control & 0x101u) == 0x101u;
}
static size_t rx_threshold(const semu_apollo4_uart *uart)
{
    unsigned selection = (uart->fifo_level >> 3u) & 7u;
    static const size_t thresholds[] = { 2u, 4u, 8u, 12u, 14u };
    return selection < SEMU_ARRAY_LEN(thresholds) ? thresholds[selection] : 2u;
}
static uint32_t raw_interrupts(const semu_apollo4_uart *uart)
{
    if (rx_enabled(uart) && uart->rx_count >= rx_threshold(uart)) {
        return SEMU_APOLLO4_UART_INTERRUPT_RX;
    }
    return 0u;
}
static uint32_t masked_interrupts(const semu_apollo4_uart *uart)
{
    return raw_interrupts(uart) & uart->interrupt_mask;
}
static void update_irq(semu_apollo4_uart *uart)
{
    int level = masked_interrupts(uart) != 0u;
    if (uart->irq_level == level) return;
    uart->irq_level = level;
    if (uart->irq_sink != NULL) {
        uart->irq_sink(uart->irq_context, SEMU_APOLLO4_UART1_IRQ, level);
    }
}
static uint32_t flags(const semu_apollo4_uart *uart)
{
    uint32_t value = uart->tx_count == 0u
                         ? SEMU_APOLLO4_UART_FLAG_TX_EMPTY : 0u;
    if (uart->tx_count == SEMU_APOLLO4_UART_FIFO_CAPACITY)
        value |= SEMU_APOLLO4_UART_FLAG_TX_FULL;
    if (uart->rx_count == 0u) value |= SEMU_APOLLO4_UART_FLAG_RX_EMPTY;
    else value |= SEMU_APOLLO4_UART_FLAG_RX_FULL;
    return value;
}
static void remove_rx_event(semu_apollo4_uart *uart, rx_event *event)
{
    rx_event **link = &uart->rx_events;
    while (*link != NULL && *link != event) link = &(*link)->next;
    if (*link == event) *link = event->next;
}
static void rx_callback(void *context, uint64_t now_ns)
{
    rx_event *event = (rx_event *)context;
    semu_apollo4_uart *uart = event->uart;
    size_t index;
    (void)now_ns;
    remove_rx_event(uart, event);
    uart->rx_reserved -= event->count;
    for (index = 0u; index < event->count; ++index) {
        size_t position = (uart->rx_head + uart->rx_count) %
                          SEMU_APOLLO4_UART_FIFO_CAPACITY;
        uart->rx_fifo[position] = event->bytes[index];
        ++uart->rx_count;
    }
    update_irq(uart);
    free(event);
}
static void tx_callback(void *context, uint64_t now_ns)
{
    semu_apollo4_uart *uart = (semu_apollo4_uart *)context;
    semu_transaction_result result = SEMU_TRANSACTION_REFUSE;
    uint8_t value;
    (void)now_ns;
    uart->tx_event = 0u;
    if (uart->tx_count != 0u && uart->endpoint_attached) {
        value = uart->tx_fifo[uart->tx_head];
        result = uart->endpoint.transmit(uart->endpoint.context, value, NULL);
        if ((unsigned)result > (unsigned)SEMU_TRANSACTION_REFUSE)
            result = SEMU_TRANSACTION_REFUSE;
        if (result == SEMU_TRANSACTION_OK) {
            uart->tx_head = (uart->tx_head + 1u) %
                            SEMU_APOLLO4_UART_FIFO_CAPACITY;
            --uart->tx_count;
        }
    }
    if (uart->tx_completion != NULL) {
        uart->tx_completion(uart->tx_completion_context, result);
    }
    uart->tx_completion = NULL;
    uart->tx_completion_context = NULL;
    update_irq(uart);
}
static semu_status validate_access(const semu_apollo4_uart *uart,
                                   uint32_t offset, unsigned width,
                                   semu_error *error)
{
    if (uart == NULL || (offset & 3u) != 0u || width != 4u) {
        return invalid(error, SEMU_ERR_ARGUMENT, "invalid Apollo4 UART access");
    }
    return SEMU_OK;
}
static semu_status read_data(semu_apollo4_uart *uart, uint32_t *value,
                             semu_error *error)
{
    if (!rx_enabled(uart)) return invalid(error, SEMU_ERR_STATE,
                                          "Apollo4 UART receiver is disabled");
    if (uart->rx_count == 0u) return invalid(error, SEMU_ERR_STATE,
                                             "Apollo4 UART RX FIFO is empty");
    *value = uart->rx_fifo[uart->rx_head];
    uart->rx_head = (uart->rx_head + 1u) % SEMU_APOLLO4_UART_FIFO_CAPACITY;
    --uart->rx_count;
    update_irq(uart);
    return SEMU_OK;
}
static semu_status write_data(semu_apollo4_uart *uart, uint32_t value,
                              semu_error *error)
{
    semu_transaction_result result;
    size_t position;
    if (!tx_enabled(uart)) return invalid(error, SEMU_ERR_STATE,
                                          "Apollo4 UART transmitter is disabled");
    if ((value & ~0xffu) != 0u) return invalid(error, SEMU_ERR_ARGUMENT,
                                               "Apollo4 UART data is not a byte");
    if (!uart->endpoint_attached) return invalid(error, SEMU_ERR_STATE,
                                                 "Apollo4 UART endpoint is detached");
    if (uart->tx_count == SEMU_APOLLO4_UART_FIFO_CAPACITY) {
        return invalid(error, SEMU_ERR_RANGE, "Apollo4 UART TX FIFO is full");
    }
    result = uart->endpoint.transmit(uart->endpoint.context, (uint8_t)value,
                                     error);
    if ((unsigned)result > (unsigned)SEMU_TRANSACTION_REFUSE) {
        return invalid(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 UART endpoint returned an invalid result");
    }
    if (result == SEMU_TRANSACTION_REFUSE) {
        return invalid(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 UART endpoint refused TX byte");
    }
    if (result == SEMU_TRANSACTION_WAIT) {
        position = (uart->tx_head + uart->tx_count) %
                   SEMU_APOLLO4_UART_FIFO_CAPACITY;
        uart->tx_fifo[position] = (uint8_t)value;
        ++uart->tx_count;
        update_irq(uart);
    }
    semu_error_clear(error);
    return SEMU_OK;
}
semu_apollo4_uart *semu_apollo4_uart_create(semu_scheduler *scheduler,
                                             semu_error *error)
{
    semu_apollo4_uart *uart;
    if (scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 UART requires a scheduler");
        return NULL;
    }
    uart = (semu_apollo4_uart *)calloc(1u, sizeof(*uart));
    if (uart == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate Apollo4 UART");
        return NULL;
    }
    uart->scheduler = scheduler;
    semu_apollo4_uart_reset(uart);
    semu_error_clear(error);
    return uart;
}
void semu_apollo4_uart_destroy(semu_apollo4_uart *uart)
{
    if (uart != NULL) {
        semu_apollo4_uart_reset(uart);
        free(uart);
    }
}
void semu_apollo4_uart_reset(semu_apollo4_uart *uart)
{
    rx_event *event;
    if (uart == NULL) return;
    while (uart->rx_events != NULL) {
        event = uart->rx_events;
        uart->rx_events = event->next;
        (void)semu_scheduler_cancel(uart->scheduler, event->id);
        free(event);
    }
    if (uart->tx_event != 0u) {
        (void)semu_scheduler_cancel(uart->scheduler, uart->tx_event);
    }
    uart->tx_event = 0u;
    uart->tx_completion = NULL;
    uart->tx_completion_context = NULL;
    uart->rx_head = 0u;
    uart->rx_count = 0u;
    uart->rx_reserved = 0u;
    uart->tx_head = 0u;
    uart->tx_count = 0u;
    uart->control = 0u;
    uart->integer_baud = 0u;
    uart->fractional_baud = 0u;
    uart->line_control = 0u;
    uart->fifo_level = 0x12u;
    uart->interrupt_mask = 0u;
    if (uart->irq_level != 0) {
        uart->irq_level = 0;
        if (uart->irq_sink != NULL) {
            uart->irq_sink(uart->irq_context, SEMU_APOLLO4_UART1_IRQ, 0);
        }
    }
}
semu_status semu_apollo4_uart_read(semu_apollo4_uart *uart, uint32_t offset,
                                   unsigned width, uint32_t *value,
                                   semu_error *error)
{
    semu_status status = validate_access(uart, offset, width, error);
    if (status != SEMU_OK || value == NULL) {
        if (status == SEMU_OK) status = invalid(error, SEMU_ERR_ARGUMENT,
                                                "UART read value is required");
        return status;
    }
    switch (offset) {
    case SEMU_APOLLO4_UART_DATA: return read_data(uart, value, error);
    case SEMU_APOLLO4_UART_FLAG: *value = flags(uart); break;
    case SEMU_APOLLO4_UART_INTEGER_BAUD: *value = uart->integer_baud; break;
    case SEMU_APOLLO4_UART_FRACTIONAL_BAUD: *value = uart->fractional_baud; break;
    case SEMU_APOLLO4_UART_LINE_CONTROL: *value = uart->line_control; break;
    case SEMU_APOLLO4_UART_CONTROL: *value = uart->control; break;
    case SEMU_APOLLO4_UART_FIFO_LEVEL: *value = uart->fifo_level; break;
    case SEMU_APOLLO4_UART_INTERRUPT_MASK: *value = uart->interrupt_mask; break;
    case SEMU_APOLLO4_UART_RAW_INTERRUPT_STATUS: *value = raw_interrupts(uart); break;
    case SEMU_APOLLO4_UART_MASKED_INTERRUPT_STATUS: *value = masked_interrupts(uart); break;
    default:
        return invalid(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 UART refuses unknown read offset");
    }
    semu_error_clear(error);
    return SEMU_OK;
}
semu_status semu_apollo4_uart_write(semu_apollo4_uart *uart, uint32_t offset,
                                    unsigned width, uint32_t value,
                                    semu_error *error)
{
    static const uint32_t controls[] = { 0u, 0x8u, 0x18u, 0x19u, 0x219u, 0x319u };
    static const uint32_t line_controls[] = { 0u, 0x10u, 0x70u };
    static const uint32_t fifo_levels[] = { 0u, 0x10u };
    static const uint32_t masks[] = { 0u, 0x51u };
    semu_status status = validate_access(uart, offset, width, error);
    if (status != SEMU_OK) return status;
    if (offset == SEMU_APOLLO4_UART_DATA) return write_data(uart, value, error);
    switch (offset) {
    case SEMU_APOLLO4_UART_INTEGER_BAUD:
        if (!allowed_value(value, (uint32_t[]){ 0u, 1u }, 2u))
            return invalid(error, SEMU_ERR_UNSUPPORTED, "Apollo4 UART baud divisor refused");
        uart->integer_baud = value;
        break;
    case SEMU_APOLLO4_UART_FRACTIONAL_BAUD:
        if (!allowed_value(value, (uint32_t[]){ 0u, 0x28u }, 2u))
            return invalid(error, SEMU_ERR_UNSUPPORTED, "Apollo4 UART baud fraction refused");
        uart->fractional_baud = value;
        break;
    case SEMU_APOLLO4_UART_LINE_CONTROL:
        if (!allowed_value(value, line_controls, SEMU_ARRAY_LEN(line_controls)))
            return invalid(error, SEMU_ERR_UNSUPPORTED, "Apollo4 UART line mode refused");
        uart->line_control = value;
        break;
    case SEMU_APOLLO4_UART_CONTROL:
        if (!allowed_value(value, controls, SEMU_ARRAY_LEN(controls)))
            return invalid(error, SEMU_ERR_UNSUPPORTED, "Apollo4 UART control mode refused");
        uart->control = value;
        update_irq(uart);
        break;
    case SEMU_APOLLO4_UART_FIFO_LEVEL:
        if (!allowed_value(value, fifo_levels, SEMU_ARRAY_LEN(fifo_levels)))
            return invalid(error, SEMU_ERR_UNSUPPORTED, "Apollo4 UART FIFO level refused");
        uart->fifo_level = value;
        update_irq(uart);
        break;
    case SEMU_APOLLO4_UART_INTERRUPT_MASK:
        if (!allowed_value(value, masks, SEMU_ARRAY_LEN(masks)))
            return invalid(error, SEMU_ERR_UNSUPPORTED, "Apollo4 UART interrupt mask refused");
        uart->interrupt_mask = value;
        update_irq(uart);
        break;
    case SEMU_APOLLO4_UART_INTERRUPT_CLEAR:
        if (value != SEMU_APOLLO4_UART_INTERRUPT_RX && value != UINT32_MAX)
            return invalid(error, SEMU_ERR_UNSUPPORTED, "Apollo4 UART interrupt clear refused");
        update_irq(uart);
        break;
    default:
        return invalid(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 UART refuses unknown write offset");
    }
    semu_error_clear(error);
    return SEMU_OK;
}
static semu_status bus_read(void *context, uint32_t offset, unsigned width,
                            uint32_t *value, semu_error *error)
{
    return semu_apollo4_uart_read((semu_apollo4_uart *)context, offset, width,
                                  value, error);
}
static semu_status bus_write(void *context, uint32_t offset, unsigned width,
                             uint32_t value, semu_error *error)
{
    return semu_apollo4_uart_write((semu_apollo4_uart *)context, offset, width,
                                   value, error);
}
static void bus_reset(void *context)
{
    semu_apollo4_uart_reset((semu_apollo4_uart *)context);
}
const semu_bus_device_ops *semu_apollo4_uart_bus_ops(void)
{
    static const semu_bus_device_ops ops = { bus_read, bus_write, bus_reset };
    return &ops;
}
semu_status semu_apollo4_uart_attach_endpoint(
    semu_apollo4_uart *uart, const semu_apollo4_uart_endpoint *endpoint,
    semu_error *error)
{
    if (uart == NULL || endpoint == NULL || endpoint->transmit == NULL) {
        return invalid(error, SEMU_ERR_ARGUMENT, "invalid Apollo4 UART endpoint");
    }
    if (uart->endpoint_attached) {
        return invalid(error, SEMU_ERR_CONFLICT,
                       "Apollo4 UART endpoint is already attached");
    }
    uart->endpoint = *endpoint;
    uart->endpoint_attached = 1;
    semu_error_clear(error);
    return SEMU_OK;
}
semu_status semu_apollo4_uart_detach_endpoint(semu_apollo4_uart *uart,
                                               semu_error *error)
{
    if (uart == NULL) return invalid(error, SEMU_ERR_ARGUMENT, "UART is required");
    if (!uart->endpoint_attached || uart->tx_count != 0u || uart->tx_event != 0u) {
        return invalid(error, SEMU_ERR_STATE,
                       "Apollo4 UART cannot detach a busy endpoint");
    }
    memset(&uart->endpoint, 0, sizeof(uart->endpoint));
    uart->endpoint_attached = 0;
    semu_error_clear(error);
    return SEMU_OK;
}
semu_status semu_apollo4_uart_set_irq_sink(semu_apollo4_uart *uart,
                                            semu_apollo4_uart_irq_fn sink,
                                            void *context, semu_error *error)
{
    if (uart == NULL) return invalid(error, SEMU_ERR_ARGUMENT, "UART is required");
    uart->irq_sink = sink;
    uart->irq_context = context;
    if (sink != NULL) sink(context, SEMU_APOLLO4_UART1_IRQ, uart->irq_level);
    semu_error_clear(error);
    return SEMU_OK;
}
semu_status semu_apollo4_uart_schedule_rx(semu_apollo4_uart *uart,
                                           uint64_t delay_ns,
                                           const uint8_t *bytes, size_t count,
                                           semu_error *error)
{
    rx_event *event;
    size_t size;
    semu_status status;
    if (uart == NULL || bytes == NULL || count == 0u) {
        return invalid(error, SEMU_ERR_ARGUMENT, "invalid Apollo4 UART RX injection");
    }
    if (count > SEMU_APOLLO4_UART_FIFO_CAPACITY ||
        uart->rx_count > SEMU_APOLLO4_UART_FIFO_CAPACITY - count ||
        uart->rx_reserved > SEMU_APOLLO4_UART_FIFO_CAPACITY - count - uart->rx_count) {
        return invalid(error, SEMU_ERR_RANGE,
                       "Apollo4 UART RX injection exceeds FIFO capacity");
    }
    size = offsetof(rx_event, bytes) + count;
    event = (rx_event *)malloc(size);
    if (event == NULL) return invalid(error, SEMU_ERR_NOMEM,
                                      "cannot allocate Apollo4 UART RX event");
    event->uart = uart;
    event->next = NULL;
    event->count = count;
    memcpy(event->bytes, bytes, count);
    status = semu_scheduler_schedule(uart->scheduler, delay_ns, rx_callback,
                                     event, &event->id, error);
    if (status != SEMU_OK) {
        free(event);
        return status;
    }
    event->next = uart->rx_events;
    uart->rx_events = event;
    uart->rx_reserved += count;
    semu_error_clear(error);
    return SEMU_OK;
}
semu_status semu_apollo4_uart_schedule_tx_completion(
    semu_apollo4_uart *uart, uint64_t delay_ns,
    semu_apollo4_uart_tx_completion_fn completion, void *context,
    semu_error *error)
{
    semu_event_id event;
    semu_status status;
    if (uart == NULL || uart->tx_count == 0u || !uart->endpoint_attached) {
        return invalid(error, SEMU_ERR_STATE,
                       "Apollo4 UART has no pending TX completion");
    }
    if (uart->tx_event != 0u) {
        return invalid(error, SEMU_ERR_CONFLICT,
                       "Apollo4 UART TX completion is already scheduled");
    }
    status = semu_scheduler_schedule(uart->scheduler, delay_ns, tx_callback, uart,
                                     &event, error);
    if (status != SEMU_OK) return status;
    uart->tx_event = event;
    uart->tx_completion = completion;
    uart->tx_completion_context = context;
    semu_error_clear(error);
    return SEMU_OK;
}
semu_status semu_apollo4_uart_schedule_tx(semu_apollo4_uart *uart,
                                          uint64_t delay_ns,
                                          semu_error *error)
{
    return semu_apollo4_uart_schedule_tx_completion(uart, delay_ns, NULL, NULL,
                                                     error);
}
