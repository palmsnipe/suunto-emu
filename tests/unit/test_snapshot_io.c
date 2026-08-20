#include "test.h"

#include "../../src/core/snapshot_io.h"

#include <stdlib.h>
#include <string.h>

static void test_writer_rejects_corrupt_size(semu_test_context *context)
{
    semu_snapshot_writer writer;
    semu_error error;
    uint8_t value = 0x5au;

    semu_error_clear(&error);
    semu_snapshot_writer_init(&writer);
    writer.size = SEMU_SNAPSHOT_MAX_SECTION_SIZE + 1u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        semu_snapshot_writer_bytes(&writer, &value, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_SNAPSHOT_MAX_SECTION_SIZE + 1u,
        writer.size);
    semu_snapshot_writer_destroy(&writer);
}

static void test_writer_round_trip(semu_test_context *context)
{
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_error error;
    uint8_t output[3u];
    static const uint8_t input[] = { 0x12u, 0x34u, 0x56u };

    semu_error_clear(&error);
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_snapshot_writer_bytes(&writer, input, sizeof(input), &error));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_snapshot_reader_bytes(&reader, output, sizeof(output), &error));
    SEMU_TEST_ASSERT(context, memcmp(input, output, sizeof(input)) == 0);
    SEMU_TEST_ASSERT(context, semu_snapshot_reader_done(&reader));
    semu_snapshot_writer_destroy(&writer);
}

static void test_u64_refusal_is_atomic(semu_test_context *context)
{
    semu_snapshot_writer writer;
    semu_error error;
    size_t marker_offset;
    uint8_t *data;

    semu_error_clear(&error);
    semu_snapshot_writer_init(&writer);
    data = (uint8_t *)calloc(1u, SEMU_SNAPSHOT_MAX_SECTION_SIZE);
    SEMU_TEST_ASSERT(context, data != NULL);
    marker_offset = SEMU_SNAPSHOT_MAX_SECTION_SIZE - 4u;
    memset(data + marker_offset, 0xa5, 4u);
    writer.data = data;
    writer.capacity = SEMU_SNAPSHOT_MAX_SECTION_SIZE;
    writer.size = marker_offset;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        semu_snapshot_writer_u64(&writer, UINT64_C(0x1122334455667788),
                                  &error));
    SEMU_TEST_EQ_U64(context, marker_offset, writer.size);
    SEMU_TEST_ASSERT(context,
        data[marker_offset] == 0xa5u && data[marker_offset + 3u] == 0xa5u);
    semu_snapshot_writer_destroy(&writer);
}

static void test_reader_u64_refusal_is_atomic(semu_test_context *context)
{
    semu_snapshot_reader reader;
    semu_error error;
    static const uint8_t truncated[] = { 0x88u, 0x77u, 0x66u, 0x55u };
    uint64_t value = UINT64_C(0xaabbccddeeff0011);

    semu_error_clear(&error);
    semu_snapshot_reader_init(&reader, truncated, sizeof(truncated));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_snapshot_reader_u64(&reader, &value, &error));
    SEMU_TEST_EQ_U64(context, 0u, reader.offset);
    SEMU_TEST_EQ_U64(context, UINT64_C(0xaabbccddeeff0011), value);
}

static void test_reader_null_destination_refusal(semu_test_context *context)
{
    semu_snapshot_reader reader;
    semu_error error;
    static const uint8_t data[8u] = { 0u };

    semu_error_clear(&error);
    semu_snapshot_reader_init(&reader, data, sizeof(data));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        semu_snapshot_reader_u8(&reader, NULL, &error));
    SEMU_TEST_EQ_U64(context, 0u, reader.offset);
    semu_snapshot_reader_init(&reader, data, sizeof(data));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        semu_snapshot_reader_u16(&reader, NULL, &error));
    SEMU_TEST_EQ_U64(context, 0u, reader.offset);
    semu_snapshot_reader_init(&reader, data, sizeof(data));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        semu_snapshot_reader_u32(&reader, NULL, &error));
    SEMU_TEST_EQ_U64(context, 0u, reader.offset);
    semu_snapshot_reader_init(&reader, data, sizeof(data));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        semu_snapshot_reader_u64(&reader, NULL, &error));
    SEMU_TEST_EQ_U64(context, 0u, reader.offset);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_writer_rejects_corrupt_size),
        SEMU_TEST_CASE(test_writer_round_trip),
        SEMU_TEST_CASE(test_u64_refusal_is_atomic),
        SEMU_TEST_CASE(test_reader_u64_refusal_is_atomic),
        SEMU_TEST_CASE(test_reader_null_destination_refusal)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
