#include "test.h"

#include <string.h>

#include "../../src/devices/sapporo_lps22.h"

typedef struct transcript_op {
    uint8_t reg;
    uint8_t value;
    uint8_t expected;
    int write;
} transcript_op;

static semu_transaction_result read_reg(
    semu_serial_endpoint *endpoint, uint8_t address, uint8_t reg,
    uint8_t *value, semu_error *error)
{
    semu_serial_transaction transaction;
    memset(&transaction, 0, sizeof(transaction));
    transaction.address = address;
    transaction.tx = &reg;
    transaction.tx_size = 1u;
    transaction.rx = value;
    transaction.rx_size = 1u;
    return endpoint->transfer(endpoint->context, &transaction, error);
}

static semu_transaction_result write_reg(
    semu_serial_endpoint *endpoint, uint8_t address, uint8_t reg,
    uint8_t value, semu_error *error)
{
    uint8_t bytes[2];
    semu_serial_transaction transaction;
    bytes[0u] = reg;
    bytes[1u] = value;
    memset(&transaction, 0, sizeof(transaction));
    transaction.address = address;
    transaction.tx = bytes;
    transaction.tx_size = sizeof(bytes);
    return endpoint->transfer(endpoint->context, &transaction, error);
}

static void run_transcript(semu_test_context *context,
                           semu_serial_endpoint *endpoint,
                           semu_error *error)
{
    static const transcript_op operations[] = {
        { 0x0fu, 0u,    0xb1u, 0 },
        { 0x11u, 0u,    0x00u, 0 },
        { 0x11u, 0x80u, 0u,    1 },
        { 0x11u, 0u,    0x00u, 0 },
        { 0x11u, 0u,    0x00u, 0 },
        { 0x11u, 0x04u, 0u,    1 },
        { 0x11u, 0u,    0x00u, 0 },
        { 0x10u, 0u,    0x00u, 0 },
        { 0x10u, 0x02u, 0u,    1 },
        { 0x10u, 0u,    0x02u, 0 },
        { 0x10u, 0x0eu, 0u,    1 },
        { 0x33u, 0u,    0x00u, 0 },
        { 0x10u, 0u,    0x0eu, 0 },
        { 0x10u, 0x1eu, 0u,    1 }
    };
    uint8_t value = 0xffu;
    size_t index;

    for (index = 0u; index < SEMU_ARRAY_LEN(operations); ++index) {
        if (operations[index].write) {
            SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                write_reg(endpoint, 0x5cu, operations[index].reg,
                          operations[index].value, error));
        } else {
            SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                read_reg(endpoint, 0x5cu, operations[index].reg,
                         &value, error));
            SEMU_TEST_EQ_U64(context, operations[index].expected, value);
        }
    }
}

static void test_exact_transcript_and_reset(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_lps22 *sensor;
    semu_serial_endpoint endpoint;

    semu_error_clear(&error);
    sensor = semu_sapporo_lps22_create(0x5cu, &error);
    SEMU_TEST_ASSERT(context, sensor != NULL);
    endpoint = semu_sapporo_lps22_endpoint(sensor);
    run_transcript(context, &endpoint, &error);
    semu_sapporo_lps22_reset(sensor);
    run_transcript(context, &endpoint, &error);
    semu_sapporo_lps22_destroy(sensor);
}

static void test_refusals_are_atomic(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_lps22 *sensor;
    semu_serial_endpoint endpoint;
    semu_serial_transaction transaction;
    uint8_t selector = 0x10u;
    uint8_t value = 0xffu;
    uint8_t bytes[3] = { 0x10u, 0x22u, 0x33u };

    semu_error_clear(&error);
    sensor = semu_sapporo_lps22_create(0x5cu, &error);
    SEMU_TEST_ASSERT(context, sensor != NULL);
    endpoint = semu_sapporo_lps22_endpoint(sensor);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     write_reg(&endpoint, 0x5cu, 0x10u, 0x1eu, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     read_reg(&endpoint, 0x5du, 0x10u, &value, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     read_reg(&endpoint, 0x5cu, 0x12u, &value, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     write_reg(&endpoint, 0x5cu, 0x12u, 0x44u, &error));
    memset(&transaction, 0, sizeof(transaction));
    transaction.address = 0x5cu;
    transaction.tx = bytes;
    transaction.tx_size = sizeof(bytes);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        endpoint.transfer(endpoint.context, &transaction, &error));
    transaction.tx = &selector;
    transaction.tx_size = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        endpoint.transfer(endpoint.context, &transaction, &error));
    transaction.rx = &value;
    transaction.rx_size = 1u;
    transaction.tx_size = 2u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        endpoint.transfer(endpoint.context, &transaction, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     read_reg(&endpoint, 0x5cu, 0x10u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x1eu, value);
    semu_sapporo_lps22_destroy(sensor);
    SEMU_TEST_ASSERT(context,
                     semu_sapporo_lps22_create(0x5du, &error) == NULL);
}

static void test_snapshot_round_trip_and_refusal(
    semu_test_context *context)
{
    semu_error error;
    semu_sapporo_lps22 *source;
    semu_sapporo_lps22 *target;
    semu_serial_endpoint endpoint;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    uint8_t value = 0u;

    semu_error_clear(&error);
    source = semu_sapporo_lps22_create(0x5cu, &error);
    target = semu_sapporo_lps22_create(0x5cu, &error);
    SEMU_TEST_ASSERT(context, source != NULL && target != NULL);
    endpoint = semu_sapporo_lps22_endpoint(source);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     write_reg(&endpoint, 0x5cu, 0x10u, 0x1eu, &error));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_lps22_snapshot_write(source, &writer, &error));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_lps22_snapshot_read(target, &reader, &error));
    endpoint = semu_sapporo_lps22_endpoint(target);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     read_reg(&endpoint, 0x5cu, 0x10u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x1eu, value);
    writer.data[3u] = 0x84u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_sapporo_lps22_snapshot_read(target, &reader, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     read_reg(&endpoint, 0x5cu, 0x10u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x1eu, value);
    semu_snapshot_writer_destroy(&writer);
    semu_sapporo_lps22_destroy(target);
    semu_sapporo_lps22_destroy(source);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_exact_transcript_and_reset),
        SEMU_TEST_CASE(test_refusals_are_atomic),
        SEMU_TEST_CASE(test_snapshot_round_trip_and_refusal)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
