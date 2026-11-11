#include "uart_internal.h"
#include "../../core/scheduler_internal.h"
#include <stdlib.h>
#include <string.h>

#define UART3_CTS_ENABLE 0x8000u
#define UART3_RX_TIMEOUT_NS UINT64_C(32000)
#define UART3_BYTE_NS UINT64_C(10000)

static int uart3(const semu_apollo4_uart *uart)
{
    return uart->irq == SEMU_APOLLO4_UART3_IRQ;
}
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
static int uart3_line_ready(const semu_apollo4_uart *uart)
{
    return uart->integer_baud == 1u && uart->fractional_baud == 0x20u &&
           uart->line_control == 0x70u;
}

static size_t rx_threshold(const semu_apollo4_uart *uart)
{
    unsigned selection = (uart->fifo_level >> 3u) & 7u;
    static const size_t thresholds[] = { 2u, 4u, 8u, 12u, 14u };
    size_t threshold = selection < SEMU_ARRAY_LEN(thresholds) ? thresholds[selection] : 2u;
    return uart3(uart) ? threshold * 2u : threshold;
}
static uint32_t raw_interrupts(const semu_apollo4_uart *uart)
{
    uint32_t pending = uart->latched_interrupts;
    if (rx_enabled(uart) && uart->rx_count >= rx_threshold(uart))
        pending |= SEMU_APOLLO4_UART_INTERRUPT_RX;
    return pending;
}
uint32_t semu_apollo4_uart_masked_interrupts(const semu_apollo4_uart *uart)
{
    return raw_interrupts(uart) & uart->interrupt_mask;
}
static void update_irq(semu_apollo4_uart *uart)
{
    int level = semu_apollo4_uart_masked_interrupts(uart) != 0u;
    if (uart->irq_level == level) return;
    uart->irq_level = level;
    if (uart->irq_sink != NULL) {
        uart->irq_sink(uart->irq_context, uart->irq, level);
    }
}
static uint32_t flags(const semu_apollo4_uart *uart)
{
    uint32_t value = uart->tx_count == 0u
                         ? SEMU_APOLLO4_UART_FLAG_TX_EMPTY : 0u;
    if (uart->tx_count == uart->capacity)
        value |= SEMU_APOLLO4_UART_FLAG_TX_FULL;
    if (uart->rx_count == 0u) value |= SEMU_APOLLO4_UART_FLAG_RX_EMPTY;
    else if (!uart3(uart) || uart->rx_count == uart->capacity)
        value |= SEMU_APOLLO4_UART_FLAG_RX_FULL;
    if (uart3(uart) && uart->cts) value |= 1u;
    if (uart3(uart) && uart->tx_event != 0u) value |= 8u;
    return value;
}
static void clear_timeout(semu_apollo4_uart *uart)
{
    if (uart->timeout_event != 0u) {
        (void)semu_scheduler_cancel_owned(uart->scheduler, uart->timeout_event,
                                          semu_apollo4_uart_timeout_event, uart);
        uart->timeout_event = 0u;
    }
    uart->latched_interrupts &= ~SEMU_APOLLO4_UART_INTERRUPT_TIMEOUT;
}

void semu_apollo4_uart_timeout_event(void *context, uint64_t now_ns)
{
    semu_apollo4_uart *uart = context;
    (void)now_ns;
    uart->timeout_event = 0u;
    if (rx_enabled(uart) && uart->rx_count != 0u &&
        (uart->interrupt_mask & SEMU_APOLLO4_UART_INTERRUPT_TIMEOUT) != 0u)
        uart->latched_interrupts |= SEMU_APOLLO4_UART_INTERRUPT_TIMEOUT;
    update_irq(uart);
}

static semu_status schedule_timeout(semu_apollo4_uart *uart, semu_error *error)
{
    semu_event_id event;
    semu_status status = semu_scheduler_schedule_tagged(uart->scheduler,
        UART3_RX_TIMEOUT_NS, SEMU_SCHED_EVENT_UART3_TIMEOUT, 0u,
        semu_apollo4_uart_timeout_event, uart, &event, error);
    if (status != SEMU_OK) return status;
    if (uart->timeout_event != 0u)
        (void)semu_scheduler_cancel_owned(uart->scheduler, uart->timeout_event,
                                          semu_apollo4_uart_timeout_event, uart);
    uart->timeout_event = event;
    return SEMU_OK;
}

static semu_status start_uart3_tx(semu_apollo4_uart *uart, uint32_t control,
                                 int cts, semu_error *error)
{
    if (uart->tx_event != 0u || (control & 0x101u) != 0x101u ||
        ((control & UART3_CTS_ENABLE) != 0u && !cts)) return SEMU_OK;
    return semu_scheduler_schedule_tagged(uart->scheduler, UART3_BYTE_NS,
        SEMU_SCHED_EVENT_UART3_TX, 0u, semu_apollo4_uart_tx_event, uart,
        &uart->tx_event, error);
}

semu_status semu_apollo4_uart_set_cts(semu_apollo4_uart *uart, int asserted,
                                     semu_error *error)
{
    semu_status status;
    if (uart == NULL || !uart3(uart) || (asserted != 0 && asserted != 1))
        return invalid(error, SEMU_ERR_ARGUMENT, "invalid Apollo4 UART3 CTS input");
    if (asserted && !uart->cts && uart->tx_count != 0u) {
        status = start_uart3_tx(uart, uart->control, asserted, error);
        if (status != SEMU_OK) return status;
    }
    uart->cts = asserted;
    semu_error_clear(error);
    return SEMU_OK;
}

static void remove_rx_event(semu_apollo4_uart *uart, rx_event *event)
{
    rx_event **link = &uart->rx_events;
    while (*link != NULL && *link != event) link = &(*link)->next;
    if (*link == event) *link = event->next;
}
void semu_apollo4_uart_rx_event(void *context, uint64_t now_ns)
{
    rx_event *event = (rx_event *)context;
    semu_apollo4_uart *uart = event->uart;
    size_t index;
    (void)now_ns;
    remove_rx_event(uart, event);
    uart->rx_reserved -= event->count;
    if (uart3(uart)) {
        semu_error error;
        if (!rx_enabled(uart) || !uart3_line_ready(uart)) {
            free(event);
            return;
        }
        if ((uart->interrupt_mask & SEMU_APOLLO4_UART_INTERRUPT_TIMEOUT) != 0u &&
            schedule_timeout(uart, &error) != SEMU_OK) {
            semu_scheduler_callback_fail(uart->scheduler, &error);
            free(event);
            return;
        }
    }
    for (index = 0u; index < event->count; ++index) {
        size_t position = (uart->rx_head + uart->rx_count) %
                          uart->capacity;
        uart->rx_fifo[position] = event->bytes[index];
        ++uart->rx_count;
    }
    update_irq(uart);
    free(event);
}
void semu_apollo4_uart_tx_event(void *context, uint64_t now_ns)
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
                            uart->capacity;
            --uart->tx_count;
        }
    }
    if (uart3(uart)) {
        semu_error error;
        if (result != SEMU_TRANSACTION_OK) {
            semu_error_set(&error, SEMU_ERR_UNSUPPORTED, "Apollo4 UART3 endpoint refused timed TX");
            semu_scheduler_callback_fail(uart->scheduler, &error);
        } else if (uart->tx_count == 0u) {
            uart->latched_interrupts |= SEMU_APOLLO4_UART_INTERRUPT_TX_COMPLETE;
        } else if (start_uart3_tx(uart, uart->control, uart->cts, &error) != SEMU_OK) {
            semu_scheduler_callback_fail(uart->scheduler, &error);
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
    uart->rx_head = (uart->rx_head + 1u) % uart->capacity;
    --uart->rx_count;
    if (uart3(uart) && uart->rx_count == 0u) clear_timeout(uart);
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
    if (uart->tx_count == uart->capacity) {
        return invalid(error, SEMU_ERR_RANGE, "Apollo4 UART TX FIFO is full");
    }
    if (uart3(uart)) {
        semu_status status;
        if (!uart3_line_ready(uart))
            return invalid(error, SEMU_ERR_STATE, "Apollo4 UART3 line is not configured");
        status = start_uart3_tx(uart, uart->control, uart->cts, error);
        if (status != SEMU_OK) return status;
        position = (uart->tx_head + uart->tx_count) % uart->capacity;
        uart->tx_fifo[position] = (uint8_t)value;
        ++uart->tx_count;
        semu_error_clear(error);
        return SEMU_OK;
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
                   uart->capacity;
        uart->tx_fifo[position] = (uint8_t)value;
        ++uart->tx_count;
        update_irq(uart);
    }
    semu_error_clear(error);
    return SEMU_OK;
}
static semu_apollo4_uart *create_uart(semu_scheduler *scheduler, unsigned irq,
                                      size_t capacity, semu_error *error)
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
    uart->irq = irq;
    uart->capacity = capacity;
    semu_apollo4_uart_reset(uart);
    semu_error_clear(error);
    return uart;
}
semu_apollo4_uart *semu_apollo4_uart_create(semu_scheduler *scheduler,
                                          semu_error *error)
{
    return create_uart(scheduler, SEMU_APOLLO4_UART1_IRQ,
                       SEMU_APOLLO4_UART_FIFO_CAPACITY, error);
}

semu_apollo4_uart *semu_apollo4_uart3_create(semu_scheduler *scheduler,
                                           semu_error *error)
{
    return create_uart(scheduler, SEMU_APOLLO4_UART3_IRQ, UART3_FIFO_CAPACITY, error);
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
        (void)semu_scheduler_cancel_owned(uart->scheduler, event->id,
                                          semu_apollo4_uart_rx_event, event);
        free(event);
    }
    if (uart->tx_event != 0u) {
        (void)semu_scheduler_cancel_owned(uart->scheduler, uart->tx_event,
                                          semu_apollo4_uart_tx_event, uart);
    }
    clear_timeout(uart);
    uart->latched_interrupts = 0u;
    uart->cts = 0;
    uart->tx_event = 0u;
    uart->tx_completion = NULL;
    uart->tx_completion_context = NULL;
    uart->rx_head = 0u;
    uart->rx_count = 0u;
    uart->rx_reserved = 0u;
    uart->next_rx_slot = 1u;
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
            uart->irq_sink(uart->irq_context, uart->irq, 0);
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
    case SEMU_APOLLO4_UART_MASKED_INTERRUPT_STATUS: *value = semu_apollo4_uart_masked_interrupts(uart); break;
    default:
        return invalid(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 UART refuses unknown read offset");
    }
    semu_error_clear(error);
    return SEMU_OK;
}
int semu_apollo4_uart3_register_valid(uint32_t offset, uint32_t value)
{
    switch (offset) {
    case SEMU_APOLLO4_UART_INTEGER_BAUD: return value <= 1u;
    case SEMU_APOLLO4_UART_FRACTIONAL_BAUD: return value == 0u || value == 0x20u;
    case SEMU_APOLLO4_UART_LINE_CONTROL: return value == 0u || value == 0x70u;
    case SEMU_APOLLO4_UART_CONTROL:
        return value == 0u || value == 0x18u || value == 0x8318u || value == 0x8319u;
    case SEMU_APOLLO4_UART_FIFO_LEVEL: return value == 0u || value == 0x10u || value == 0x12u;
    case SEMU_APOLLO4_UART_INTERRUPT_MASK: return (value & ~UART3_IRQ_MASK) == 0u;
    case SEMU_APOLLO4_UART_INTERRUPT_CLEAR:
        return (value & ~UART3_IRQ_MASK) == 0u || value == 0x7ffu || value == UINT32_MAX;
    default: return 0;
    }
}

static semu_status write_uart3_register(semu_apollo4_uart *uart, uint32_t offset,
                                       uint32_t value, semu_error *error)
{
    if (!semu_apollo4_uart3_register_valid(offset, value))
        return invalid(error, SEMU_ERR_UNSUPPORTED, "unsupported Apollo4 UART3 register write");
    switch (offset) {
    case SEMU_APOLLO4_UART_INTEGER_BAUD: uart->integer_baud = value; break;
    case SEMU_APOLLO4_UART_FRACTIONAL_BAUD: uart->fractional_baud = value; break;
    case SEMU_APOLLO4_UART_LINE_CONTROL: uart->line_control = value; break;
    case SEMU_APOLLO4_UART_CONTROL:
        if ((value & 0x301u) != 0x301u) {
            if (uart->tx_event != 0u)
                (void)semu_scheduler_cancel_owned(uart->scheduler, uart->tx_event,
                                                  semu_apollo4_uart_tx_event, uart);
            uart->tx_event = 0u;
            uart->tx_count = 0u;
            uart->latched_interrupts = 0u;
            clear_timeout(uart);
        } else if (uart->tx_count != 0u) {
            semu_status status = start_uart3_tx(uart, value, uart->cts, error);
            if (status != SEMU_OK) return status;
        }
        uart->control = value;
        break;
    case SEMU_APOLLO4_UART_FIFO_LEVEL: uart->fifo_level = value; break;
    case SEMU_APOLLO4_UART_INTERRUPT_MASK:
        if ((value & SEMU_APOLLO4_UART_INTERRUPT_TIMEOUT) == 0u) {
            clear_timeout(uart);
        } else if ((uart->interrupt_mask & SEMU_APOLLO4_UART_INTERRUPT_TIMEOUT) == 0u &&
                   rx_enabled(uart) && uart->rx_count != 0u) {
            semu_status status = schedule_timeout(uart, error);
            if (status != SEMU_OK) return status;
        }
        uart->interrupt_mask = value;
        break;
    case SEMU_APOLLO4_UART_INTERRUPT_CLEAR:
        uart->latched_interrupts &= ~value;
        break;
    default:
        return invalid(error, SEMU_ERR_UNSUPPORTED, "unknown Apollo4 UART3 register");
    }
    update_irq(uart);
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
    if (uart3(uart)) return write_uart3_register(uart, offset, value, error);
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
    if (sink != NULL) sink(context, uart->irq, uart->irq_level);
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
    if (count > uart->capacity ||
        uart->rx_count > uart->capacity - count ||
        uart->rx_reserved > uart->capacity - count - uart->rx_count) {
        return invalid(error, SEMU_ERR_RANGE,
                       "Apollo4 UART RX injection exceeds FIFO capacity");
    }
    size = offsetof(rx_event, bytes) + count;
    event = (rx_event *)malloc(size);
    if (event == NULL) return invalid(error, SEMU_ERR_NOMEM,
                                      "cannot allocate Apollo4 UART RX event");
    event->uart = uart;
    event->next = NULL;
    if (uart->next_rx_slot == UINT32_MAX) {
        free(event);
        return invalid(error, SEMU_ERR_RANGE,
                       "Apollo4 UART RX event slot overflow");
    }
    event->slot = uart->next_rx_slot;
    event->count = count;
    memcpy(event->bytes, bytes, count);
    status = semu_scheduler_schedule_tagged(uart->scheduler, delay_ns,
                                     uart3(uart) ? SEMU_SCHED_EVENT_UART3_RX : SEMU_SCHED_EVENT_UART_RX,
                                     event->slot,
                                     semu_apollo4_uart_rx_event, event, &event->id, error);
    if (status != SEMU_OK) {
        free(event);
        return status;
    }
    ++uart->next_rx_slot;
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
    if (uart == NULL || uart3(uart) || uart->tx_count == 0u || !uart->endpoint_attached) {
        return invalid(error, SEMU_ERR_STATE,
                       "Apollo4 UART has no pending TX completion");
    }
    if (uart->tx_event != 0u) {
        return invalid(error, SEMU_ERR_CONFLICT,
                       "Apollo4 UART TX completion is already scheduled");
    }
    status = semu_scheduler_schedule_tagged(uart->scheduler, delay_ns,
                                     SEMU_SCHED_EVENT_UART_TX, 0u,
                                     semu_apollo4_uart_tx_event, uart, &event, error);
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
