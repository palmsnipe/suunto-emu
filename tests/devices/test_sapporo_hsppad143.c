#include "test.h"

#include <stdint.h>
#include <string.h>

#include "../../src/devices/sapporo_hsppad143.h"

static semu_transaction_result do_read(semu_serial_endpoint *ep,
                                       uint8_t address, uint8_t reg,
                                       uint8_t *rx, size_t rx_size,
                                       semu_error *error)
{
    uint8_t tx = reg;
    semu_serial_transaction t = { address, 0u, &tx, 1u, rx, rx_size };
    return ep->transfer(ep->context, &t, error);
}

static semu_transaction_result do_write(semu_serial_endpoint *ep,
                                        uint8_t address, const uint8_t *tx,
                                        size_t tx_size, semu_error *error)
{
    semu_serial_transaction t = { address, 0u, tx, tx_size, NULL, 0u };
    return ep->transfer(ep->context, &t, error);
}

static void test_startup_transcript(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_hsppad143 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_hsppad143_create(0x48u, &error);
    SEMU_TEST_ASSERT(context, sensor != NULL);
    ep = semu_sapporo_hsppad143_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x48u, 0x00u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x49u, rx[0u]);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x48u, 0x03u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x11u, rx[0u]);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x48u, 0x1cu, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0xe0u, rx[0u]);

    semu_sapporo_hsppad143_destroy(sensor);
}

static void test_reset(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_hsppad143 *sensor;
    semu_serial_endpoint ep;
    uint8_t tx[] = { 0x0eu, 0xAAu };
    uint8_t rx[1];

    semu_error_clear(&error);
    sensor = semu_sapporo_hsppad143_create(0x48u, &error);
    ep = semu_sapporo_hsppad143_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x48u, tx, 2u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x48u, 0x0eu, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x12u, rx[0u]);

    semu_sapporo_hsppad143_reset(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x48u, 0x0eu, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x13u, rx[0u]);

    semu_sapporo_hsppad143_destroy(sensor);
}

static void test_wrong_address(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_hsppad143 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[1];

    semu_error_clear(&error);
    sensor = semu_sapporo_hsppad143_create(0x48u, &error);
    ep = semu_sapporo_hsppad143_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x49u, 0x00u, rx, 1u, &error));

    semu_sapporo_hsppad143_destroy(sensor);
}

static void test_register_overflow(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_hsppad143 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2];
    uint8_t tx[] = { 0xffu, 0x01u, 0x02u };

    semu_error_clear(&error);
    sensor = semu_sapporo_hsppad143_create(0x48u, &error);
    ep = semu_sapporo_hsppad143_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x48u, 0xffu, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0x48u, tx, 3u, &error));

    semu_sapporo_hsppad143_destroy(sensor);
}

static void test_observed_sample_and_configuration(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_hsppad143 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[3];
    const uint8_t writes[][2] = {
        { 0x11u, 0x80u }, { 0x0eu, 0x03u },
        { 0x0fu, 0xa5u }, { 0x13u, 0x3cu }
    };
    size_t i;

    semu_error_clear(&error);
    sensor = semu_sapporo_hsppad143_create(0x48u, &error);
    ep = semu_sapporo_hsppad143_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x48u, 0x04u, rx, 3u, &error));
    SEMU_TEST_EQ_U64(context, 0u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0u, rx[1u]);
    SEMU_TEST_EQ_U64(context, 0u, rx[2u]);
    for (i = 0u; i < sizeof(writes) / sizeof(writes[0]); ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         do_write(&ep, 0x48u, writes[i], 2u, &error));
    }

    semu_sapporo_hsppad143_destroy(sensor);
}

static void test_empty_write(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_hsppad143 *sensor;
    semu_serial_endpoint ep;

    semu_error_clear(&error);
    sensor = semu_sapporo_hsppad143_create(0x48u, &error);
    ep = semu_sapporo_hsppad143_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0x48u, NULL, 0u, &error));

    semu_sapporo_hsppad143_destroy(sensor);
}

static void test_register_map_defaults(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_hsppad143 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[1];

    semu_error_clear(&error);
    sensor = semu_sapporo_hsppad143_create(0x48u, &error);
    ep = semu_sapporo_hsppad143_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x48u, 0x01u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x31u, rx[0u]);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x48u, 0x0eu, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x13u, rx[0u]);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x48u, 0x0fu, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0xa0u, rx[0u]);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x48u, 0x12u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x10u, rx[0u]);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x48u, 0x13u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x38u, rx[0u]);

    semu_sapporo_hsppad143_destroy(sensor);
}

static void test_configuration_masks(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_hsppad143 *sensor;
    semu_serial_endpoint ep;
    const uint8_t writes[][2] = {
        { 0x0eu, 0xffu }, { 0x0fu, 0xffu }, { 0x10u, 0xffu },
        { 0x11u, 0xffu }, { 0x12u, 0xffu }, { 0x13u, 0xffu }
    };
    const uint8_t expected[] = { 0x13u, 0xafu, 0x0au,
                                 0x80u, 0x9fu, 0x3fu };
    uint8_t rx[1];
    size_t i;

    semu_error_clear(&error);
    sensor = semu_sapporo_hsppad143_create(0x48u, &error);
    ep = semu_sapporo_hsppad143_endpoint(sensor);

    for (i = 0u; i < sizeof(writes) / sizeof(writes[0]); ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         do_write(&ep, 0x48u, writes[i], 2u, &error));
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         do_read(&ep, 0x48u, writes[i][0], rx, 1u, &error));
        SEMU_TEST_EQ_U64(context, expected[i], rx[0u]);
    }

    semu_sapporo_hsppad143_destroy(sensor);
}

static void test_unknown_and_atomic_frames(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_hsppad143 *sensor;
    semu_serial_endpoint ep;
    uint8_t read_only[] = { 0x00u, 0xaau };
    uint8_t invalid_span[] = { 0x0eu, 0x11u, 0x22u, 0x33u,
                               0x44u, 0x55u, 0x66u, 0x77u };
    uint8_t rx[1];
    semu_serial_transaction malformed = { 0x48u, 0u, NULL, 1u, NULL, 0u };

    semu_error_clear(&error);
    sensor = semu_sapporo_hsppad143_create(0x48u, &error);
    ep = semu_sapporo_hsppad143_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0x48u, read_only, 2u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x48u, 0x07u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0x48u, invalid_span,
                              sizeof(invalid_span), &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     ep.transfer(ep.context, &malformed, &error));

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x48u, 0x0eu, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x13u, rx[0u]);

    semu_sapporo_hsppad143_destroy(sensor);
}

static void test_repeated_start_retains_selector(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_hsppad143 *sensor;
    semu_serial_endpoint ep;
    uint8_t selector = 0x1cu;
    uint8_t rx[1];
    semu_serial_transaction read_without_selector = {
        0x48u, 0u, NULL, 0u, rx, sizeof(rx)
    };

    semu_error_clear(&error);
    sensor = semu_sapporo_hsppad143_create(0x48u, &error);
    ep = semu_sapporo_hsppad143_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x48u, &selector, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &read_without_selector, &error));
    SEMU_TEST_EQ_U64(context, 0xe0u, rx[0u]);

    semu_sapporo_hsppad143_destroy(sensor);
}

static void test_repeated_transcript(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_hsppad143 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx1[1], rx2[1];
    unsigned i;

    semu_error_clear(&error);
    sensor = semu_sapporo_hsppad143_create(0x48u, &error);
    ep = semu_sapporo_hsppad143_endpoint(sensor);

    for (i = 0u; i < 2u; ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         do_read(&ep, 0x48u, 0x00u, rx1, 1u, &error));
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         do_read(&ep, 0x48u, 0x03u, rx2, 1u, &error));
        SEMU_TEST_EQ_U64(context, 0x49u, rx1[0u]);
        SEMU_TEST_EQ_U64(context, 0x11u, rx2[0u]);
    }

    semu_sapporo_hsppad143_destroy(sensor);
}

static void test_snapshot_cursor_and_atomic_refusal(
    semu_test_context *context)
{
    semu_error error;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_sapporo_hsppad143 *source;
    semu_sapporo_hsppad143 *target;
    semu_serial_endpoint source_ep;
    semu_serial_endpoint target_ep;
    semu_serial_transaction no_selector;
    uint8_t rx[1];
    uint8_t selector = 0x0eu;

    semu_error_clear(&error);
    semu_snapshot_writer_init(&writer);
    source = semu_sapporo_hsppad143_create(0x48u, &error);
    target = semu_sapporo_hsppad143_create(0x48u, &error);
    source_ep = semu_sapporo_hsppad143_endpoint(source);
    target_ep = semu_sapporo_hsppad143_endpoint(target);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&source_ep, 0x48u, 0x0eu, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x13u, rx[0u]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_hsppad143_snapshot_write(
                         source, &writer, &error));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_hsppad143_snapshot_read(
                         target, &reader, &error));
    no_selector = (semu_serial_transaction){
        0x48u, 0u, NULL, 0u, rx, sizeof(rx)
    };
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     target_ep.transfer(target_ep.context, &no_selector,
                                        &error));
    SEMU_TEST_EQ_U64(context, 0xa0u, rx[0u]);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&target_ep, 0x48u, &selector, 1u, &error));
    writer.data[0u] = 0x49u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
                     semu_sapporo_hsppad143_snapshot_read(
                         target, &reader, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     target_ep.transfer(target_ep.context, &no_selector,
                                        &error));
    SEMU_TEST_EQ_U64(context, 0x13u, rx[0u]);
    semu_snapshot_writer_destroy(&writer);
    semu_sapporo_hsppad143_destroy(source);
    semu_sapporo_hsppad143_destroy(target);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_startup_transcript),
        SEMU_TEST_CASE(test_reset),
        SEMU_TEST_CASE(test_wrong_address),
        SEMU_TEST_CASE(test_register_overflow),
        SEMU_TEST_CASE(test_observed_sample_and_configuration),
        SEMU_TEST_CASE(test_empty_write),
        SEMU_TEST_CASE(test_register_map_defaults),
        SEMU_TEST_CASE(test_configuration_masks),
        SEMU_TEST_CASE(test_unknown_and_atomic_frames),
        SEMU_TEST_CASE(test_repeated_start_retains_selector),
        SEMU_TEST_CASE(test_repeated_transcript),
        SEMU_TEST_CASE(test_snapshot_cursor_and_atomic_refusal)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
