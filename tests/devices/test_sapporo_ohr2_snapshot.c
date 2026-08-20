#include "sapporo_ohr2.h"
#include "test.h"

#include <string.h>

enum {
    SNAPSHOT_RESPONSE_OFFSET = 1u,
    SNAPSHOT_QUEUED_SEQUENCE_OFFSET = 59u,
    SNAPSHOT_RESPONSE_QUEUED_OFFSET = 61u,
    SNAPSHOT_SELECTOR_ARMED_OFFSET = 62u,
    SNAPSHOT_READY_OFFSET = 63u,
    SNAPSHOT_RESPONSE_CRC_OFFSET =
        SNAPSHOT_RESPONSE_OFFSET + SEMU_SAPPORO_OHR2_PAYLOAD_SIZE
};

static semu_transaction_result body_provider(
    void *context, semu_sapporo_ohr2_command command, uint16_t sequence,
    semu_sapporo_ohr2_state state, const uint8_t request[54],
    uint8_t response[54], semu_error *error)
{
    (void)context;
    (void)sequence;
    (void)state;
    (void)request;
    (void)error;
    (void)memset(response, 0xa5, 54u);
    if (command == SEMU_SAPPORO_OHR2_COMMAND_IDENTITY) {
        response[9u] = 'B';
        response[10u] = 'S';
        response[11u] = 'L';
        response[12u] = 0u;
    }
    return SEMU_TRANSACTION_OK;
}

static void make_request(uint8_t request[59], uint16_t command,
                         uint16_t sequence)
{
    uint32_t crc;
    (void)memset(request, 0, 59u);
    request[1u] = (uint8_t)command;
    request[2u] = (uint8_t)(command >> 8u);
    request[3u] = (uint8_t)sequence;
    request[4u] = (uint8_t)(sequence >> 8u);
    crc = semu_sapporo_ohr2_crc32(request + 1u, 54u);
    request[55u] = (uint8_t)crc;
    request[56u] = (uint8_t)(crc >> 8u);
    request[57u] = (uint8_t)(crc >> 16u);
    request[58u] = (uint8_t)(crc >> 24u);
}

static int queue_response(semu_sapporo_ohr2 *device, semu_error *error)
{
    uint8_t request[SEMU_SAPPORO_OHR2_REQUEST_SIZE];
    semu_serial_endpoint endpoint = semu_sapporo_ohr2_endpoint(device);
    semu_serial_transaction transaction;

    make_request(request, SEMU_SAPPORO_OHR2_COMMAND_IDENTITY, 1u);
    transaction = (semu_serial_transaction){
        SEMU_SAPPORO_OHR2_ADDRESS, 0u, request, sizeof(request), NULL, 0u
    };
    return endpoint.transfer(endpoint.context, &transaction, error) ==
           SEMU_TRANSACTION_OK;
}

static void put_u16le(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
}

static void put_u32le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static void test_round_trip(semu_test_context *context)
{
    semu_error error;
    semu_snapshot_writer writer;
    semu_snapshot_writer after;
    semu_snapshot_reader reader;
    semu_sapporo_ohr2 *source;
    semu_sapporo_ohr2 *target;

    semu_error_clear(&error);
    semu_snapshot_writer_init(&writer);
    semu_snapshot_writer_init(&after);
    source = semu_sapporo_ohr2_create(NULL, NULL, body_provider, NULL,
                                      &error);
    target = semu_sapporo_ohr2_create(NULL, NULL, body_provider, NULL,
                                      &error);
    SEMU_TEST_ASSERT(context, source != NULL && target != NULL);
    SEMU_TEST_ASSERT(context, queue_response(source, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_ohr2_snapshot_write(source, &writer,
                                                      &error));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_ohr2_snapshot_read(target, &reader,
                                                     &error));
    SEMU_TEST_ASSERT(context, semu_snapshot_reader_done(&reader));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_ohr2_snapshot_write(target, &after,
                                                      &error));
    SEMU_TEST_EQ_U64(context, writer.size, after.size);
    SEMU_TEST_ASSERT(context, memcmp(writer.data, after.data, writer.size) == 0);
    semu_snapshot_writer_destroy(&after);
    semu_snapshot_writer_destroy(&writer);
    semu_sapporo_ohr2_destroy(target);
    semu_sapporo_ohr2_destroy(source);
}

static void test_linkage_refusals(semu_test_context *context)
{
    semu_error error;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_sapporo_ohr2 *source;
    semu_sapporo_ohr2 *target;

    semu_error_clear(&error);
    semu_snapshot_writer_init(&writer);
    source = semu_sapporo_ohr2_create(NULL, NULL, body_provider, NULL,
                                      &error);
    target = semu_sapporo_ohr2_create(NULL, NULL, body_provider, NULL,
                                      &error);
    SEMU_TEST_ASSERT(context, source != NULL && target != NULL);
    SEMU_TEST_ASSERT(context, queue_response(source, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_ohr2_snapshot_write(source, &writer,
                                                      &error));

    writer.data[SNAPSHOT_READY_OFFSET] = 0u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_sapporo_ohr2_snapshot_read(target, &reader,
                                                     &error));
    writer.data[SNAPSHOT_READY_OFFSET] = 1u;
    writer.data[SNAPSHOT_RESPONSE_QUEUED_OFFSET] = 0u;
    writer.data[SNAPSHOT_SELECTOR_ARMED_OFFSET] = 1u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_sapporo_ohr2_snapshot_read(target, &reader,
                                                     &error));

    semu_snapshot_writer_destroy(&writer);
    semu_sapporo_ohr2_destroy(target);
    semu_sapporo_ohr2_destroy(source);
}

static void test_queued_response_refusals(semu_test_context *context)
{
    semu_error error;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_sapporo_ohr2 *source;
    semu_sapporo_ohr2 *target;
    uint32_t crc;

    semu_error_clear(&error);
    semu_snapshot_writer_init(&writer);
    source = semu_sapporo_ohr2_create(NULL, NULL, body_provider, NULL,
                                      &error);
    target = semu_sapporo_ohr2_create(NULL, NULL, body_provider, NULL,
                                      &error);
    SEMU_TEST_ASSERT(context, source != NULL && target != NULL);
    SEMU_TEST_ASSERT(context, queue_response(source, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_ohr2_snapshot_write(source, &writer,
                                                      &error));

    put_u16le(writer.data + SNAPSHOT_QUEUED_SEQUENCE_OFFSET, 0u);
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_sapporo_ohr2_snapshot_read(target, &reader,
                                                     &error));
    put_u16le(writer.data + SNAPSHOT_QUEUED_SEQUENCE_OFFSET, 1u);
    writer.data[SNAPSHOT_RESPONSE_OFFSET] = 0xffu;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_sapporo_ohr2_snapshot_read(target, &reader,
                                                     &error));
    writer.data[SNAPSHOT_RESPONSE_OFFSET] = 0u;
    crc = semu_sapporo_ohr2_crc32(writer.data + SNAPSHOT_RESPONSE_OFFSET,
                                  SEMU_SAPPORO_OHR2_PAYLOAD_SIZE);
    put_u32le(writer.data + SNAPSHOT_RESPONSE_CRC_OFFSET, crc ^ 1u);
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_sapporo_ohr2_snapshot_read(target, &reader,
                                                     &error));

    semu_snapshot_writer_destroy(&writer);
    semu_sapporo_ohr2_destroy(target);
    semu_sapporo_ohr2_destroy(source);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_round_trip),
        SEMU_TEST_CASE(test_linkage_refusals),
        SEMU_TEST_CASE(test_queued_response_refusals)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
