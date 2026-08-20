#include "test.h"

#include <stdint.h>

#include "../../src/devices/sapporo_lsm6dsl.h"

static void test_reserved_state_refuses(semu_test_context *context)
{
    semu_error error;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_sapporo_lsm6dsl *source;
    semu_sapporo_lsm6dsl *target;

    semu_error_clear(&error);
    semu_snapshot_writer_init(&writer);
    source = semu_sapporo_lsm6dsl_create(0u, &error);
    target = semu_sapporo_lsm6dsl_create(0u, &error);
    SEMU_TEST_ASSERT(context, source != NULL && target != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_lsm6dsl_snapshot_write(source, &writer,
                                                         &error));

    /* Snapshot layout: chip-select, 0x1f config bytes, register, flags. */
    writer.data[1u + 0x0bu] = 0x01u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_sapporo_lsm6dsl_snapshot_read(target, &reader,
                                                        &error));

    writer.data[1u + 0x0bu] = 0u;
    writer.data[32u] = 0x40u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_sapporo_lsm6dsl_snapshot_read(target, &reader,
                                                        &error));

    semu_snapshot_writer_destroy(&writer);
    semu_sapporo_lsm6dsl_destroy(target);
    semu_sapporo_lsm6dsl_destroy(source);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_reserved_state_refuses)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
