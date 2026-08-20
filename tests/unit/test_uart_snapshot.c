#include "../../src/soc/apollo4/uart.h"
#include "../../src/core/snapshot_io.h"
#include "../../src/core/scheduler_internal.h"
#include "semu/scheduler.h"
#include "test.h"

#include <stddef.h>
#include <string.h>

enum {
    UART_SNAPSHOT_RX_RESERVED_OFFSET = 10u,
    UART_SNAPSHOT_NEXT_RX_SLOT_OFFSET = 86u,
    UART_SNAPSHOT_EVENT_ID_OFFSET = 94u,
    UART_SNAPSHOT_EVENT_SLOT_OFFSET = 102u
};

static void put_u32le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static semu_apollo4_uart *make_uart(semu_scheduler **scheduler,
                                    semu_error *error)
{
    *scheduler = semu_scheduler_create(error);
    if (*scheduler == NULL) return NULL;
    return semu_apollo4_uart_create(*scheduler, error);
}

static int write_pending_snapshot(semu_apollo4_uart *uart,
                                  semu_snapshot_writer *writer,
                                  semu_error *error)
{
    static const uint8_t byte = 0xa5u;
    semu_snapshot_writer_init(writer);
    return semu_apollo4_uart_schedule_rx(uart, 100u, &byte, 1u, error) ==
               SEMU_OK &&
           semu_apollo4_uart_snapshot_write(uart, writer, error) == SEMU_OK;
}

static void test_pending_rx_round_trip(semu_test_context *context)
{
    semu_error error;
    semu_error target_error;
    semu_scheduler *scheduler;
    semu_scheduler *target_scheduler;
    semu_apollo4_uart *uart = make_uart(&scheduler, &error);
    semu_apollo4_uart *target = make_uart(&target_scheduler, &target_error);
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_event_callback callback = NULL;
    void *event_context = NULL;

    SEMU_TEST_ASSERT(context, uart != NULL && target != NULL);
    SEMU_TEST_ASSERT(context, write_pending_snapshot(uart, &writer, &error));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_uart_snapshot_read(target, &reader,
                                                     &target_error));
    SEMU_TEST_ASSERT(context, semu_snapshot_reader_done(&reader));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_uart_snapshot_resolve_event(
                         target, SEMU_SCHED_EVENT_UART_RX, 1u,
                         &callback, &event_context, &target_error));
    SEMU_TEST_ASSERT(context, callback != NULL && event_context != NULL);
    semu_snapshot_writer_destroy(&writer);
    semu_apollo4_uart_destroy(target);
    semu_scheduler_destroy(target_scheduler);
    semu_apollo4_uart_destroy(uart);
    semu_scheduler_destroy(scheduler);
}

static void test_rx_reservation_mismatch_refuses(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_apollo4_uart *uart = make_uart(&scheduler, &error);
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;

    SEMU_TEST_ASSERT(context, uart != NULL);
    SEMU_TEST_ASSERT(context, write_pending_snapshot(uart, &writer, &error));
    put_u32le(writer.data + UART_SNAPSHOT_RX_RESERVED_OFFSET, 0u);
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_apollo4_uart_snapshot_read(uart, &reader, &error));
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_count(scheduler));
    semu_snapshot_writer_destroy(&writer);
    semu_apollo4_uart_destroy(uart);
    semu_scheduler_destroy(scheduler);
}

static void test_rx_event_identity_refuses(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_apollo4_uart *uart = make_uart(&scheduler, &error);
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;

    SEMU_TEST_ASSERT(context, uart != NULL);
    SEMU_TEST_ASSERT(context, write_pending_snapshot(uart, &writer, &error));
    put_u32le(writer.data + UART_SNAPSHOT_NEXT_RX_SLOT_OFFSET, 1u);
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_apollo4_uart_snapshot_read(uart, &reader, &error));

    put_u32le(writer.data + UART_SNAPSHOT_NEXT_RX_SLOT_OFFSET, 2u);
    put_u32le(writer.data + UART_SNAPSHOT_EVENT_SLOT_OFFSET, 0u);
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_apollo4_uart_snapshot_read(uart, &reader, &error));

    put_u32le(writer.data + UART_SNAPSHOT_EVENT_SLOT_OFFSET, 1u);
    memset(writer.data + UART_SNAPSHOT_EVENT_ID_OFFSET, 0, sizeof(uint64_t));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_apollo4_uart_snapshot_read(uart, &reader, &error));
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_count(scheduler));
    semu_snapshot_writer_destroy(&writer);
    semu_apollo4_uart_destroy(uart);
    semu_scheduler_destroy(scheduler);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_pending_rx_round_trip),
        SEMU_TEST_CASE(test_rx_reservation_mismatch_refuses),
        SEMU_TEST_CASE(test_rx_event_identity_refuses)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
