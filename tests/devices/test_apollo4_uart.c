#include "../../src/soc/apollo4/uart.h"

#include "semu/scheduler.h"
#include "test.h"

#include <string.h>

typedef struct endpoint_log {
    uint8_t bytes[SEMU_APOLLO4_UART_FIFO_CAPACITY + 8u];
    size_t count;
    unsigned waits;
    int refuse;
} endpoint_log;

typedef struct irq_log {
    unsigned irq;
    int levels[8];
    size_t count;
} irq_log;

static semu_apollo4_uart *make_uart(semu_scheduler **scheduler,
                                    semu_error *error)
{
    *scheduler = semu_scheduler_create(error);
    if (*scheduler == NULL) return NULL;
    return semu_apollo4_uart_create(*scheduler, error);
}

static semu_transaction_result transmit(void *context, uint8_t value,
                                        semu_error *error)
{
    endpoint_log *log = (endpoint_log *)context;
    (void)error;
    if (log->refuse) return SEMU_TRANSACTION_REFUSE;
    if (log->waits != 0u) {
        --log->waits;
        return SEMU_TRANSACTION_WAIT;
    }
    log->bytes[log->count++] = value;
    return SEMU_TRANSACTION_OK;
}

static void irq(void *context, unsigned number, int level)
{
    irq_log *log = (irq_log *)context;
    log->irq = number;
    if (log->count < SEMU_ARRAY_LEN(log->levels)) log->levels[log->count++] = level;
}

static void complete_tx(void *context, semu_transaction_result result)
{
    semu_transaction_result *out = (semu_transaction_result *)context;
    *out = result;
}

static int configure(semu_apollo4_uart *uart, semu_error *error)
{
    return semu_apollo4_uart_write(uart, SEMU_APOLLO4_UART_INTEGER_BAUD, 4u,
                                   1u, error) == SEMU_OK &&
           semu_apollo4_uart_write(uart, SEMU_APOLLO4_UART_FRACTIONAL_BAUD,
                                   4u, 0x28u, error) == SEMU_OK &&
           semu_apollo4_uart_write(uart, SEMU_APOLLO4_UART_LINE_CONTROL, 4u,
                                   0x70u, error) == SEMU_OK &&
           semu_apollo4_uart_write(uart, SEMU_APOLLO4_UART_FIFO_LEVEL, 4u,
                                   0u, error) == SEMU_OK &&
           semu_apollo4_uart_write(uart, SEMU_APOLLO4_UART_CONTROL, 4u,
                                   0x319u, error) == SEMU_OK &&
           semu_apollo4_uart_write(uart, SEMU_APOLLO4_UART_INTERRUPT_MASK,
                                   4u, 0x51u, error) == SEMU_OK;
}

static void test_reset_and_registers(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_apollo4_uart *uart = make_uart(&scheduler, &error);
    uint32_t value = 0u;
    SEMU_TEST_ASSERT(context, uart != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_read(
        uart, SEMU_APOLLO4_UART_FIFO_LEVEL, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x12u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT, semu_apollo4_uart_read(
        uart, SEMU_APOLLO4_UART_FLAG, 2u, &value, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, semu_apollo4_uart_read(
        uart, 0x04u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, semu_apollo4_uart_write(
        uart, SEMU_APOLLO4_UART_INTEGER_BAUD, 4u, 2u, &error));
    semu_apollo4_uart_destroy(uart);
    semu_scheduler_destroy(scheduler);
}

static void test_rx_fifo_irq_and_transcript(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_apollo4_uart *uart = make_uart(&scheduler, &error);
    irq_log irq_log = { 0u, { 0 }, 0u };
    endpoint_log endpoint = { { 0 }, 0u, 0u, 0 };
    semu_apollo4_uart_endpoint target = { "synthetic-cxd5610", transmit, &endpoint };
    uint8_t input[] = { '$', 'P', 'S', 'S', '0', '0', '\r', '\n' };
    uint32_t value = 0u;
    size_t index;
    SEMU_TEST_ASSERT(context, uart != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_attach_endpoint(
        uart, &target, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_set_irq_sink(
        uart, irq, &irq_log, &error));
    SEMU_TEST_ASSERT(context, configure(uart, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_schedule_rx(
        uart, 10u, input, sizeof(input), &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(scheduler, 10u,
                                                               &error));
    SEMU_TEST_EQ_U64(context, 16u, irq_log.irq);
    SEMU_TEST_EQ_U64(context, 1u, irq_log.count > 0u
                                      ? irq_log.levels[irq_log.count - 1u] : 0u);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_read(
        uart, SEMU_APOLLO4_UART_MASKED_INTERRUPT_STATUS, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x10u, value);
    for (index = 0u; index < sizeof(input); ++index) {
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_read(
            uart, SEMU_APOLLO4_UART_DATA, 4u, &value, &error));
        SEMU_TEST_EQ_U64(context, input[index], value);
    }
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, semu_apollo4_uart_read(
        uart, SEMU_APOLLO4_UART_DATA, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_write(
        uart, SEMU_APOLLO4_UART_INTERRUPT_CLEAR, 4u, 0x10u, &error));
    SEMU_TEST_EQ_U64(context, 0u, irq_log.levels[irq_log.count - 1u]);
    semu_apollo4_uart_destroy(uart);
    semu_scheduler_destroy(scheduler);
}

static void test_fifo_bounds_and_atomic_refusals(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_apollo4_uart *uart = make_uart(&scheduler, &error);
    endpoint_log endpoint = { { 0 }, 0u, 0u, 0 };
    semu_apollo4_uart_endpoint target = { "target", transmit, &endpoint };
    uint8_t byte = 0x41u;
    uint8_t full[SEMU_APOLLO4_UART_FIFO_CAPACITY];
    uint32_t value = 0u;
    size_t index;
    SEMU_TEST_ASSERT(context, uart != NULL);
    memset(full, 0x33, sizeof(full));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_attach_endpoint(
        uart, &target, &error));
    SEMU_TEST_ASSERT(context, configure(uart, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_schedule_rx(
        uart, 0u, full, sizeof(full), &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, semu_apollo4_uart_schedule_rx(
        uart, 0u, &byte, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_run_next(scheduler, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, semu_apollo4_uart_schedule_rx(
        uart, 0u, &byte, 1u, &error));
    for (index = 0u; index < sizeof(full); ++index) {
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_read(
            uart, SEMU_APOLLO4_UART_DATA, 4u, &value, &error));
    }
    endpoint.refuse = 1;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, semu_apollo4_uart_write(
        uart, SEMU_APOLLO4_UART_DATA, 4u, 0x41u, &error));
    SEMU_TEST_EQ_U64(context, 0u, endpoint.count);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT, semu_apollo4_uart_write(
        uart, SEMU_APOLLO4_UART_DATA, 1u, 0x41u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_detach_endpoint(
        uart, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, semu_apollo4_uart_write(
        uart, SEMU_APOLLO4_UART_DATA, 4u, 0x41u, &error));
    semu_apollo4_uart_destroy(uart);
    semu_scheduler_destroy(scheduler);
}

static void test_tx_wait_completion_and_detach(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_apollo4_uart *uart = make_uart(&scheduler, &error);
    endpoint_log endpoint = { { 0 }, 0u, 1u, 0 };
    semu_apollo4_uart_endpoint target = { "target", transmit, &endpoint };
    semu_transaction_result completion = SEMU_TRANSACTION_REFUSE;
    uint32_t value = 0u;
    SEMU_TEST_ASSERT(context, uart != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_attach_endpoint(
        uart, &target, &error));
    SEMU_TEST_ASSERT(context, configure(uart, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_write(
        uart, SEMU_APOLLO4_UART_DATA, 4u, 0x40u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, semu_apollo4_uart_detach_endpoint(
        uart, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_schedule_tx_completion(
        uart, 7u, complete_tx, &completion, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(scheduler, 7u,
                                                               &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, completion);
    SEMU_TEST_EQ_U64(context, 1u, endpoint.count);
    SEMU_TEST_EQ_U64(context, 0x40u, endpoint.bytes[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_read(
        uart, SEMU_APOLLO4_UART_FLAG, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, SEMU_APOLLO4_UART_FLAG_TX_EMPTY |
                            SEMU_APOLLO4_UART_FLAG_RX_EMPTY, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_detach_endpoint(
        uart, &error));
    semu_apollo4_uart_destroy(uart);
    semu_scheduler_destroy(scheduler);
}

static void test_repeatable_transcript(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler_a;
    semu_scheduler *scheduler_b;
    semu_apollo4_uart *a = make_uart(&scheduler_a, &error);
    semu_apollo4_uart *b = make_uart(&scheduler_b, &error);
    endpoint_log log_a = { { 0 }, 0u, 0u, 0 };
    endpoint_log log_b = { { 0 }, 0u, 0u, 0 };
    semu_apollo4_uart_endpoint endpoint_a = { "a", transmit, &log_a };
    semu_apollo4_uart_endpoint endpoint_b = { "b", transmit, &log_b };
    const uint8_t bytes[] = { '@', 'V', 'E', 'R', '\r', '\n' };
    size_t index;
    SEMU_TEST_ASSERT(context, a != NULL && b != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_attach_endpoint(
        a, &endpoint_a, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_attach_endpoint(
        b, &endpoint_b, &error));
    SEMU_TEST_ASSERT(context, configure(a, &error));
    SEMU_TEST_ASSERT(context, configure(b, &error));
    for (index = 0u; index < sizeof(bytes); ++index) {
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_write(
            a, SEMU_APOLLO4_UART_DATA, 4u, bytes[index], &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_uart_write(
            b, SEMU_APOLLO4_UART_DATA, 4u, bytes[index], &error));
    }
    SEMU_TEST_EQ_U64(context, sizeof(bytes), log_a.count);
    SEMU_TEST_ASSERT(context, memcmp(log_a.bytes, log_b.bytes, log_a.count) == 0);
    semu_apollo4_uart_destroy(a);
    semu_apollo4_uart_destroy(b);
    semu_scheduler_destroy(scheduler_a);
    semu_scheduler_destroy(scheduler_b);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_reset_and_registers),
        SEMU_TEST_CASE(test_rx_fifo_irq_and_transcript),
        SEMU_TEST_CASE(test_fifo_bounds_and_atomic_refusals),
        SEMU_TEST_CASE(test_tx_wait_completion_and_detach),
        SEMU_TEST_CASE(test_repeatable_transcript)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
