#include "test.h"

#include "../../src/core/snapshot_io.h"

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

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_writer_rejects_corrupt_size),
        SEMU_TEST_CASE(test_writer_round_trip)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
