#ifndef SEMU_APOLLO4_UART_INTERNAL_H
#define SEMU_APOLLO4_UART_INTERNAL_H

#include "uart.h"

#include <stddef.h>

typedef struct rx_event rx_event;
struct rx_event {
    semu_apollo4_uart *uart;
    semu_event_id id;
    rx_event *next;
    uint32_t slot;
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
    uint32_t next_rx_slot;
};

void semu_apollo4_uart_rx_event(void *context, uint64_t now_ns);
void semu_apollo4_uart_tx_event(void *context, uint64_t now_ns);

#endif
