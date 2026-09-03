#include "test.h"

#include <string.h>

#include "../../src/devices/sapporo_devices_internal.h"

typedef struct devices_fixture {
    semu_error error;
    semu_scheduler *scheduler;
    semu_sapporo_devices *devices;
} devices_fixture;

static int fixture_init(devices_fixture *fixture)
{
    semu_error_clear(&fixture->error);
    fixture->scheduler = semu_scheduler_create(&fixture->error);
    if (fixture->scheduler == NULL) return 0;
    fixture->devices = semu_sapporo_devices_create(fixture->scheduler, NULL,
                                                   &fixture->error);
    return fixture->devices != NULL;
}

static void fixture_destroy(devices_fixture *fixture)
{
    semu_sapporo_devices_destroy(fixture->devices);
    semu_scheduler_destroy(fixture->scheduler);
}

static semu_transaction_result lps22_read(
    devices_fixture *fixture, uint8_t reg, uint8_t *value)
{
    const semu_serial_endpoint *iom2 =
        semu_sapporo_devices_iom_endpoint(fixture->devices, 2u);
    semu_serial_transaction transaction = {
        0x5cu, 0u, &reg, 1u, value, 1u
    };
    return iom2->transfer(iom2->context, &transaction, &fixture->error);
}

static semu_transaction_result lps22_write(
    devices_fixture *fixture, uint8_t reg, uint8_t value)
{
    const semu_serial_endpoint *iom2 =
        semu_sapporo_devices_iom_endpoint(fixture->devices, 2u);
    uint8_t bytes[2] = { reg, value };
    semu_serial_transaction transaction = {
        0x5cu, 0u, bytes, sizeof(bytes), NULL, 0u
    };
    return iom2->transfer(iom2->context, &transaction, &fixture->error);
}

static void test_late_child_refusal_is_atomic(semu_test_context *context)
{
    devices_fixture source = {0};
    devices_fixture target = {0};
    semu_snapshot_writer writer;
    semu_snapshot_writer before;
    semu_snapshot_writer after;
    semu_snapshot_reader reader;
    const semu_serial_endpoint *iom2;
    const uint8_t tx[] = { 0x13u, 0x3cu };
    semu_serial_transaction transaction = {
        0x48u, 0u, tx, sizeof(tx), NULL, 0u
    };

    SEMU_TEST_ASSERT(context, fixture_init(&source));
    SEMU_TEST_ASSERT(context, fixture_init(&target));
    semu_snapshot_writer_init(&writer);
    semu_snapshot_writer_init(&before);
    semu_snapshot_writer_init(&after);
    iom2 = semu_sapporo_devices_iom_endpoint(source.devices, 2u);
    SEMU_TEST_ASSERT(context, iom2 != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     iom2->transfer(iom2->context, &transaction,
                                    &source.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_devices_snapshot_write(
                         target.devices, &before, &target.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_devices_snapshot_write(
                         source.devices, &writer, &source.error));
    writer.data[writer.size - 1u] = 2u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_sapporo_devices_snapshot_read(
                         target.devices, &reader, &target.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_devices_snapshot_write(
                         target.devices, &after, &target.error));
    SEMU_TEST_EQ_U64(context, before.size, after.size);
    SEMU_TEST_ASSERT(context,
                     memcmp(before.data, after.data, before.size) == 0);
    semu_snapshot_writer_destroy(&after);
    semu_snapshot_writer_destroy(&before);
    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&target);
    fixture_destroy(&source);
}

static void test_common_profile_bytes_unchanged(semu_test_context *context)
{
    static const char *const profiles[] = {
        "sapporo-2.22.60", "sapporo-2.33.16"
    };
    devices_fixture baseline = {0};
    devices_fixture selected = {0};
    semu_snapshot_writer before;
    semu_snapshot_writer after;
    size_t index;

    for (index = 0u; index < SEMU_ARRAY_LEN(profiles); ++index) {
        SEMU_TEST_ASSERT(context, fixture_init(&baseline));
        SEMU_TEST_ASSERT(context, fixture_init(&selected));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_devices_select_profile(
                selected.devices, profiles[index], &selected.error));
        semu_snapshot_writer_init(&before);
        semu_snapshot_writer_init(&after);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_devices_snapshot_write(
                baseline.devices, &before, &baseline.error));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_devices_snapshot_write(
                selected.devices, &after, &selected.error));
        SEMU_TEST_EQ_U64(context, before.size, after.size);
        SEMU_TEST_ASSERT(context,
                         memcmp(before.data, after.data, before.size) == 0);
        semu_snapshot_writer_destroy(&after);
        semu_snapshot_writer_destroy(&before);
        fixture_destroy(&selected);
        fixture_destroy(&baseline);
    }
}

static void test_lps22_round_trip_old_restore_and_atomic_refusal(
    semu_test_context *context)
{
    devices_fixture source = {0};
    devices_fixture target = {0};
    semu_snapshot_writer writer;
    semu_snapshot_writer before;
    semu_snapshot_writer after;
    semu_snapshot_reader reader;
    uint8_t value = 0xffu;

    SEMU_TEST_ASSERT(context, fixture_init(&source));
    SEMU_TEST_ASSERT(context, fixture_init(&target));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_devices_select_profile(
            source.devices, "sapporo-2.39.20", &source.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_devices_select_profile(
            target.devices, "sapporo-2.39.20", &target.error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     lps22_write(&source, 0x10u, 0x1eu));
    semu_snapshot_writer_init(&writer);
    semu_snapshot_writer_init(&before);
    semu_snapshot_writer_init(&after);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_devices_snapshot_write(
            source.devices, &writer, &source.error));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_devices_snapshot_read(
            target.devices, &reader, &target.error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     lps22_read(&target, 0x10u, &value));
    SEMU_TEST_EQ_U64(context, 0x1eu, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_devices_snapshot_write(
            target.devices, &before, &target.error));
    writer.data[writer.size - 1u] = 0x84u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_sapporo_devices_snapshot_read(
            target.devices, &reader, &target.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_devices_snapshot_write(
            target.devices, &after, &target.error));
    SEMU_TEST_EQ_U64(context, before.size, after.size);
    SEMU_TEST_ASSERT(context,
                     memcmp(before.data, after.data, before.size) == 0);
    writer.data[writer.size - 1u] = 0u;
    writer.size -= 4u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     lps22_write(&target, 0x10u, 0x02u));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_devices_snapshot_read(
            target.devices, &reader, &target.error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     lps22_read(&target, 0x10u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    semu_snapshot_writer_destroy(&after);
    semu_snapshot_writer_destroy(&before);
    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&target);
    fixture_destroy(&source);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_late_child_refusal_is_atomic),
        SEMU_TEST_CASE(test_common_profile_bytes_unchanged),
        SEMU_TEST_CASE(test_lps22_round_trip_old_restore_and_atomic_refusal)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
