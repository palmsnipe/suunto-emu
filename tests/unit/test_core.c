#include "semu/bus.h"
#include "semu/log.h"
#include "semu/scheduler.h"
#include "../../src/core/bus_internal.h"
#include "../../src/core/scheduler_internal.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

typedef struct event_log {
    unsigned values[8];
    size_t count;
} event_log;

typedef struct event_item { event_log *log; unsigned value; } event_item;

typedef struct test_device {
    uint32_t value;
    unsigned resets;
    unsigned writes;
} test_device;

static void record_event(void *opaque, uint64_t now_ns)
{
    event_item *item = (event_item *)opaque;
    item->log->values[item->log->count++] = item->value + (unsigned)now_ns;
}

static semu_status device_read(void *opaque, uint32_t offset, unsigned width,
                               uint32_t *value, semu_error *error)
{
    test_device *device = (test_device *)opaque;
    if (offset != 0u || width != 4u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "bad register");
        return SEMU_ERR_UNSUPPORTED;
    }
    *value = device->value;
    return SEMU_OK;
}

static semu_status device_write(void *opaque, uint32_t offset, unsigned width,
                                uint32_t value, semu_error *error)
{
    test_device *device = (test_device *)opaque;
    if (offset != 0u || width != 4u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "bad register");
        return SEMU_ERR_UNSUPPORTED;
    }
    ++device->writes;
    device->value = value;
    return SEMU_OK;
}

static void device_reset(void *opaque)
{
    test_device *device = (test_device *)opaque;
    device->value = 0u;
    ++device->resets;
}

static void test_status_names(semu_test_context *context)
{
    semu_error error;
    semu_error_set(&error, SEMU_ERR_RANGE, "value %u", 7u);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, error.code);
    SEMU_TEST_ASSERT(context, strcmp(error.text, "value 7") == 0);
    SEMU_TEST_ASSERT(context, strcmp(semu_status_name(SEMU_ERR_IO), "io") == 0);
    SEMU_TEST_ASSERT(context,
        strcmp(semu_stop_reason_name(SEMU_STOP_WFI_DEADLOCK), "wfi-deadlock") == 0);
}

static void test_scheduler_order_and_cancel(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    event_log log = {{0u}, 0u};
    event_item first = {&log, 100u};
    event_item second = {&log, 200u};
    semu_event_id cancelled;
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_scheduler_schedule(scheduler, 5u, record_event, &first, NULL, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_scheduler_schedule(scheduler, 5u, record_event, &second, NULL, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_scheduler_schedule(scheduler, 2u, record_event, &second,
                                &cancelled, &error));
    SEMU_TEST_ASSERT(context, semu_scheduler_cancel(scheduler, cancelled));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(scheduler, 5u, &error));
    SEMU_TEST_EQ_U64(context, 105u, log.values[0]);
    SEMU_TEST_EQ_U64(context, 205u, log.values[1]);
    SEMU_TEST_EQ_U64(context, 5u, semu_scheduler_now(scheduler));
    SEMU_TEST_ASSERT(context, !semu_scheduler_has_events(scheduler));
    semu_scheduler_destroy(scheduler);
}

static void test_scheduler_one_tick_fast_and_refusal(
    semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    event_log log = {{0u}, 0u};
    event_item item = {&log, 40u};

    SEMU_TEST_ASSERT(context, scheduler != NULL);
    semu_error_set(&error, SEMU_ERR_STATE, "stale");
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance_one(scheduler, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, error.code);
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_now(scheduler));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_scheduler_schedule(scheduler, 1u, record_event, &item, NULL,
                                &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance_one(scheduler, &error));
    SEMU_TEST_EQ_U64(context, 42u, log.values[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_scheduler_restore_begin(scheduler, UINT64_MAX, 0u, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
                     semu_scheduler_advance_one(scheduler, &error));
    SEMU_TEST_EQ_U64(context, UINT64_MAX, semu_scheduler_now(scheduler));
    semu_scheduler_destroy(scheduler);
}

static void test_scheduler_id_overflow_refusal(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    event_log log = {{0u}, 0u};
    event_item item = {&log, 1u};

    SEMU_TEST_ASSERT(context, scheduler != NULL);
    scheduler->next_id = UINT64_MAX;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        semu_scheduler_schedule(scheduler, 0u, record_event, &item,
                                 NULL, &error));
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(scheduler));
    SEMU_TEST_EQ_U64(context, UINT64_MAX, scheduler->next_id);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        semu_scheduler_restore_begin(scheduler, 0u, 0u, UINT64_MAX,
                                      &error));
    semu_scheduler_destroy(scheduler);
}

static void test_scheduler_restore_slot_identity_refusal(
    semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    event_log log = {{0u}, 0u};
    event_item item = {&log, 1u};
    semu_scheduled_event_state first = {
        0u, 0u, 1u, SEMU_SCHED_EVENT_SYSTICK, 0u
    };
    semu_scheduled_event_state duplicate = {
        0u, 1u, 2u, SEMU_SCHED_EVENT_SYSTICK, 0u
    };

    SEMU_TEST_ASSERT(context, scheduler != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_restore_begin(scheduler, 0u, 2u, 3u,
                                                   &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_restore_event(scheduler, &first,
                                                   record_event, &item,
                                                   &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_scheduler_restore_event(scheduler, &duplicate,
                                                   record_event, &item,
                                                   &error));
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_count(scheduler));
    semu_scheduler_destroy(scheduler);
}

static void test_scheduler_restore_kind_refusal(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    event_log log = {{0u}, 0u};
    event_item item = {&log, 1u};
    semu_scheduled_event_state state = {
        0u, 0u, 1u, SEMU_SCHED_EVENT_NEMA_COMPLETION + 1u, 0u
    };

    SEMU_TEST_ASSERT(context, scheduler != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_restore_begin(scheduler, 0u, 1u, 2u,
                                                   &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_scheduler_restore_event(scheduler, &state,
                                                  record_event, &item,
                                                  &error));
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(scheduler));
    semu_scheduler_destroy(scheduler);
}

static void test_bus_memory_and_device(semu_test_context *context)
{
    static const uint8_t rom[] = {1u, 2u, 3u, 4u};
    semu_bus_device_ops ops = {device_read, device_write, device_reset};
    test_device device = {0u, 0u, 0u};
    test_device overlay = {0xdeadbeefu, 0u, 0u};
    semu_error error;
    semu_bus *bus = semu_bus_create(&error);
    uint32_t value;
    uint8_t output[4];
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_map_ram(bus, "ram", 0x1000u, 16u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_map_rom(bus, "rom", 0x2000u, rom, 4u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_map_device(bus, "dev", 0x3000u, 4u, &ops, &device, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
                     semu_bus_map_ram(bus, "overlap", 0x100fu, 2u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x1000u, 4u, 0x44332211u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_validate_write(bus, 0x1000u, 4u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_copy_out(bus, 0x1000u, output, 4u, &error));
    SEMU_TEST_ASSERT(context, memcmp(output, "\x11\x22\x33\x44", 4u) == 0);
    semu_error_set(&error, SEMU_ERR_STATE, "stale bus error");
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read_u16(bus, 0x1001u, &value, &error));
    SEMU_TEST_ASSERT(context, error.code == SEMU_OK && error.text[0] == '\0');
    SEMU_TEST_EQ_U64(context, 0x3322u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_map_overlay(bus, "overlay", 0x1004u, 4u, &ops, &overlay,
                             &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x1004u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0xdeadbeefu, value);
    value = 0x12345678u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read_u16(bus, 0x1004u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x12345678u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read_below(bus, 0x1004u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    semu_bus_unmap_overlay(bus, &overlay);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x1004u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(bus, 0x2000u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x04030201u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
                     semu_bus_write(bus, 0x2000u, 1u, 0u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
                     semu_bus_validate_write(bus, 0x2000u, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_validate_write(bus, 0x3000u, 4u, &error));
    SEMU_TEST_EQ_U64(context, 0u, device.writes);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x3000u, 4u, 0xa5u, &error));
    SEMU_TEST_EQ_U64(context, 1u, device.writes);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(bus, 0x3000u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0xa5u, value);
    semu_bus_reset(bus);
    SEMU_TEST_EQ_U64(context, 1u, device.resets);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(bus, 0x1000u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
                     semu_bus_read(bus, 0x4000u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
                     semu_bus_read_u16(bus, 0x4000u, &value, &error));
    semu_bus_destroy(bus);
}

static void test_bus_snapshot_region_set_is_exact(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    uint8_t *image;
    size_t image_size;
    uint32_t value;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_map_ram(bus, "first", 0x1000u, 4u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_map_ram(bus, "second", 0x2000u, 4u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(bus, 0x1000u, 4u, 0x11111111u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(bus, 0x2000u, 4u, 0x22222222u, &error));

    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_snapshot_write(bus, &writer, &error));
    image = writer.data;
    image_size = writer.size;
    SEMU_TEST_EQ_U64(context, 28u, image_size);

    /* An image with a missing region is not a complete bus snapshot. */
    image[0] = 1u;
    image[1] = image[2] = image[3] = 0u;
    semu_snapshot_reader_init(&reader, image, image_size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
        semu_bus_snapshot_read(bus, &reader, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_read(bus, 0x1000u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x11111111u, value);

    /* Restore the count and duplicate the first region's base in record 2. */
    image[0] = 2u;
    image[16u] = image[4u];
    image[17u] = image[5u];
    image[18u] = image[6u];
    image[19u] = image[7u];
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(bus, 0x1000u, 4u, 0xaaaaaaaaU, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(bus, 0x2000u, 4u, 0xbbbbbbbbU, &error));
    semu_snapshot_reader_init(&reader, image, image_size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_bus_snapshot_read(bus, &reader, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_read(bus, 0x1000u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0xaaaaaaaau, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_read(bus, 0x2000u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0xbbbbbbbbu, value);

    /* A truncated later payload must release staging and preserve both RAM regions. */
    image[16u] = 0u;
    image[17u] = 0x20u;
    image[18u] = image[19u] = 0u;
    semu_snapshot_reader_init(&reader, image, image_size - 1u);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_bus_snapshot_read(bus, &reader, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_read(bus, 0x1000u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0xaaaaaaaau, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_read(bus, 0x2000u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0xbbbbbbbbu, value);

    semu_snapshot_writer_destroy(&writer);
    semu_bus_destroy(bus);
}

static void test_log_filter(semu_test_context *context)
{
    FILE *stream = tmpfile();
    semu_logger logger;
    char output[160];
    size_t count;
    SEMU_TEST_ASSERT(context, stream != NULL);
    semu_log_init(&logger, stream, SEMU_LOG_INFO);
    semu_log_write(&logger, SEMU_LOG_DEBUG, "cpu", "step", "hidden");
    semu_log_set_time(&logger, 42u);
    semu_log_write(&logger, SEMU_LOG_INFO, "core", "ready", "value=%u", 7u);
    rewind(stream);
    count = fread(output, 1u, sizeof(output) - 1u, stream);
    output[count] = '\0';
    SEMU_TEST_ASSERT(context, strstr(output, "time_ns=42 level=info") != NULL);
    SEMU_TEST_ASSERT(context, strstr(output, "hidden") == NULL);
    (void)fclose(stream);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_status_names),
        SEMU_TEST_CASE(test_scheduler_order_and_cancel),
        SEMU_TEST_CASE(test_scheduler_one_tick_fast_and_refusal),
        SEMU_TEST_CASE(test_scheduler_id_overflow_refusal),
        SEMU_TEST_CASE(test_scheduler_restore_slot_identity_refusal),
        SEMU_TEST_CASE(test_scheduler_restore_kind_refusal),
        SEMU_TEST_CASE(test_bus_memory_and_device),
        SEMU_TEST_CASE(test_bus_snapshot_region_set_is_exact),
        SEMU_TEST_CASE(test_log_filter)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
