#include "../../src/display/nema_completion.h"
#include "../../src/core/snapshot_io.h"
#include "semu/scheduler.h"
#include "test.h"

#include <stddef.h>
#include <string.h>

enum {
    COMPLETION_COUNT_OFFSET = 0u,
    COMPLETION_FIRST_ENTRY_OFFSET = 4u,
    COMPLETION_ENTRY_SIZE = 13u,
    COMPLETION_LIST_ID_OFFSET = 0u,
    COMPLETION_EVENT_ID_OFFSET = 4u,
    COMPLETION_ACTIVE_OFFSET = 12u
};

static void put_u32le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static int write_snapshot(nema_completion *comp, semu_scheduler *scheduler,
                          semu_snapshot_writer *writer, semu_error *error,
                          unsigned entries)
{
    unsigned index;

    for (index = 0u; index < entries; ++index) {
        if (nema_completion_schedule(comp, scheduler, 7u + index,
                                     NULL, NULL, NULL, NULL, error) != SEMU_OK) {
            return 0;
        }
    }
    semu_snapshot_writer_init(writer);
    return nema_completion_snapshot_write(comp, writer, error) == SEMU_OK;
}

static void test_active_entry_round_trip(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    semu_scheduler *target_scheduler = NULL;
    nema_completion *comp = NULL;
    nema_completion *target = NULL;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_event_callback callback = NULL;
    void *event_context = NULL;

    SEMU_TEST_ASSERT(context, scheduler != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_completion_create(&comp, &error));
    SEMU_TEST_ASSERT(context,
                     write_snapshot(comp, scheduler, &writer, &error, 1u));

    target_scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, target_scheduler != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_completion_create(&target, &error));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_completion_snapshot_read(target, &reader, &error));
    SEMU_TEST_ASSERT(context, semu_snapshot_reader_done(&reader));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_completion_snapshot_resolve_event(
                         target, 0u, &callback, &event_context, &error));
    SEMU_TEST_ASSERT(context, callback != NULL && event_context != NULL);
    SEMU_TEST_ASSERT(context, nema_completion_pending(target, 7u));

    semu_snapshot_writer_destroy(&writer);
    nema_completion_destroy(target);
    semu_scheduler_destroy(target_scheduler);
    nema_completion_destroy(comp);
    semu_scheduler_destroy(scheduler);
}

static void test_missing_event_id_refuses(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    nema_completion *comp = NULL;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;

    SEMU_TEST_ASSERT(context, scheduler != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_completion_create(&comp, &error));
    SEMU_TEST_ASSERT(context,
                     write_snapshot(comp, scheduler, &writer, &error, 1u));
    memset(writer.data + COMPLETION_FIRST_ENTRY_OFFSET +
               COMPLETION_EVENT_ID_OFFSET, 0, sizeof(uint64_t));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     nema_completion_snapshot_read(comp, &reader, &error));
    SEMU_TEST_ASSERT(context, nema_completion_pending(comp, 7u));
    SEMU_TEST_EQ_U64(context, 1u, nema_completion_count(comp));

    semu_snapshot_writer_destroy(&writer);
    nema_completion_destroy(comp);
    semu_scheduler_destroy(scheduler);
}

static void test_active_identity_mismatch_refuses(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    nema_completion *comp = NULL;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    size_t second = COMPLETION_FIRST_ENTRY_OFFSET + COMPLETION_ENTRY_SIZE;

    SEMU_TEST_ASSERT(context, scheduler != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_completion_create(&comp, &error));
    SEMU_TEST_ASSERT(context,
                     write_snapshot(comp, scheduler, &writer, &error, 2u));
    put_u32le(writer.data + COMPLETION_COUNT_OFFSET, 1u);
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     nema_completion_snapshot_read(comp, &reader, &error));

    put_u32le(writer.data + COMPLETION_COUNT_OFFSET, 2u);
    put_u32le(writer.data + second + COMPLETION_LIST_ID_OFFSET, 7u);
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     nema_completion_snapshot_read(comp, &reader, &error));

    put_u32le(writer.data + second + COMPLETION_LIST_ID_OFFSET, 8u);
    memcpy(writer.data + second + COMPLETION_EVENT_ID_OFFSET,
           writer.data + COMPLETION_FIRST_ENTRY_OFFSET +
               COMPLETION_EVENT_ID_OFFSET, sizeof(uint64_t));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     nema_completion_snapshot_read(comp, &reader, &error));
    SEMU_TEST_ASSERT(context, nema_completion_pending(comp, 7u));
    SEMU_TEST_ASSERT(context, nema_completion_pending(comp, 8u));

    semu_snapshot_writer_destroy(&writer);
    nema_completion_destroy(comp);
    semu_scheduler_destroy(scheduler);
}

static void test_active_count_mismatch_refuses(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    nema_completion *comp = NULL;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;

    SEMU_TEST_ASSERT(context, scheduler != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_completion_create(&comp, &error));
    SEMU_TEST_ASSERT(context,
                     write_snapshot(comp, scheduler, &writer, &error, 1u));
    put_u32le(writer.data + COMPLETION_COUNT_OFFSET, 0u);
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     nema_completion_snapshot_read(comp, &reader, &error));
    SEMU_TEST_ASSERT(context, nema_completion_pending(comp, 7u));

    semu_snapshot_writer_destroy(&writer);
    nema_completion_destroy(comp);
    semu_scheduler_destroy(scheduler);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_active_entry_round_trip),
        SEMU_TEST_CASE(test_missing_event_id_refuses),
        SEMU_TEST_CASE(test_active_identity_mismatch_refuses),
        SEMU_TEST_CASE(test_active_count_mismatch_refuses)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
