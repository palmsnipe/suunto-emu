#ifndef SEMU_APOLLO4_UART_H
#define SEMU_APOLLO4_UART_H

#include "semu/bus.h"
#include "semu/scheduler.h"
#include "semu/types.h"
#include "../../core/snapshot_io.h"

#define SEMU_APOLLO4_UART1_BASE 0x4001d000u
#define SEMU_APOLLO4_UART1_IRQ 16u
#define SEMU_APOLLO4_UART_FIFO_CAPACITY 16u

#define SEMU_APOLLO4_UART_DATA 0x00u
#define SEMU_APOLLO4_UART_FLAG 0x18u
#define SEMU_APOLLO4_UART_INTEGER_BAUD 0x24u
#define SEMU_APOLLO4_UART_FRACTIONAL_BAUD 0x28u
#define SEMU_APOLLO4_UART_LINE_CONTROL 0x2cu
#define SEMU_APOLLO4_UART_CONTROL 0x30u
#define SEMU_APOLLO4_UART_FIFO_LEVEL 0x34u
#define SEMU_APOLLO4_UART_INTERRUPT_MASK 0x38u
#define SEMU_APOLLO4_UART_RAW_INTERRUPT_STATUS 0x3cu
#define SEMU_APOLLO4_UART_MASKED_INTERRUPT_STATUS 0x40u
#define SEMU_APOLLO4_UART_INTERRUPT_CLEAR 0x44u

#define SEMU_APOLLO4_UART_FLAG_RX_EMPTY 0x10u
#define SEMU_APOLLO4_UART_FLAG_TX_FULL 0x20u
#define SEMU_APOLLO4_UART_FLAG_RX_FULL 0x40u
#define SEMU_APOLLO4_UART_FLAG_TX_EMPTY 0x80u
#define SEMU_APOLLO4_UART_INTERRUPT_RX 0x10u

typedef struct semu_apollo4_uart semu_apollo4_uart;

typedef semu_transaction_result (*semu_apollo4_uart_transmit_fn)(
    void *context, uint8_t value, semu_error *error);

typedef struct semu_apollo4_uart_endpoint {
    const char *name;
    semu_apollo4_uart_transmit_fn transmit;
    void *context;
} semu_apollo4_uart_endpoint;

typedef void (*semu_apollo4_uart_irq_fn)(void *context, unsigned irq,
                                          int level);
typedef void (*semu_apollo4_uart_tx_completion_fn)(
    void *context, semu_transaction_result result);

semu_apollo4_uart *semu_apollo4_uart_create(semu_scheduler *scheduler,
                                             semu_error *error);
void semu_apollo4_uart_destroy(semu_apollo4_uart *uart);
void semu_apollo4_uart_reset(semu_apollo4_uart *uart);

semu_status semu_apollo4_uart_read(semu_apollo4_uart *uart, uint32_t offset,
                                   unsigned width, uint32_t *value,
                                   semu_error *error);
semu_status semu_apollo4_uart_write(semu_apollo4_uart *uart, uint32_t offset,
                                    unsigned width, uint32_t value,
                                    semu_error *error);
const semu_bus_device_ops *semu_apollo4_uart_bus_ops(void);

semu_status semu_apollo4_uart_attach_endpoint(
    semu_apollo4_uart *uart, const semu_apollo4_uart_endpoint *endpoint,
    semu_error *error);
semu_status semu_apollo4_uart_detach_endpoint(semu_apollo4_uart *uart,
                                               semu_error *error);
semu_status semu_apollo4_uart_set_irq_sink(semu_apollo4_uart *uart,
                                            semu_apollo4_uart_irq_fn sink,
                                            void *context, semu_error *error);

semu_status semu_apollo4_uart_schedule_rx(semu_apollo4_uart *uart,
                                           uint64_t delay_ns,
                                           const uint8_t *bytes, size_t count,
                                           semu_error *error);
semu_status semu_apollo4_uart_schedule_tx_completion(
    semu_apollo4_uart *uart, uint64_t delay_ns,
    semu_apollo4_uart_tx_completion_fn completion, void *context,
    semu_error *error);
semu_status semu_apollo4_uart_schedule_tx(semu_apollo4_uart *uart,
                                          uint64_t delay_ns,
                                          semu_error *error);
semu_status semu_apollo4_uart_snapshot_write(
    const semu_apollo4_uart *uart, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_apollo4_uart_snapshot_read(
    semu_apollo4_uart *uart, semu_snapshot_reader *reader,
    semu_error *error);
semu_status semu_apollo4_uart_snapshot_resolve_event(
    semu_apollo4_uart *uart, uint32_t kind, uint32_t subject,
    semu_event_callback *callback, void **context, semu_error *error);

#endif
