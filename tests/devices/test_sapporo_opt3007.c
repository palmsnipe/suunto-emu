#include "test.h"

#include <stdint.h>
#include <string.h>

#include "../../src/devices/sapporo_opt3007.h"

static semu_transaction_result do_read(semu_serial_endpoint *ep,
                                       uint8_t address, uint8_t ptr,
                                       uint8_t *rx, size_t rx_size,
                                       semu_error *error)
{
    uint8_t tx = ptr;
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

static void test_result_read(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_opt3007 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_opt3007_create(0x45u, &error);
    SEMU_TEST_ASSERT(context, sensor != NULL);
    ep = semu_sapporo_opt3007_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x45u, 0x00u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x00u, rx[1u]);

    semu_sapporo_opt3007_destroy(sensor);
}

static void test_config_write_and_readback(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_opt3007 *sensor;
    semu_serial_endpoint ep;
    uint8_t tx[] = { 0x01u, 0xC6u, 0x00u };
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_opt3007_create(0x45u, &error);
    ep = semu_sapporo_opt3007_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x45u, tx, 3u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x45u, 0x01u, rx, 2u, &error));
    /* Bit 4 is the documented read-only L bit and resets high. */
    SEMU_TEST_EQ_U64(context, 0xC6u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x10u, rx[1u]);

    semu_sapporo_opt3007_destroy(sensor);
}

static void test_result_unaffected_by_config(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_opt3007 *sensor;
    semu_serial_endpoint ep;
    uint8_t tx[] = { 0x01u, 0xC6u, 0x00u };
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_opt3007_create(0x45u, &error);
    ep = semu_sapporo_opt3007_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x45u, tx, 3u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x45u, 0x00u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x00u, rx[1u]);

    semu_sapporo_opt3007_destroy(sensor);
}

static void test_reset(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_opt3007 *sensor;
    semu_serial_endpoint ep;
    uint8_t tx[] = { 0x01u, 0xFFu, 0xFFu };
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_opt3007_create(0x45u, &error);
    ep = semu_sapporo_opt3007_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x45u, tx, 3u, &error));
    semu_sapporo_opt3007_reset(sensor);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x45u, 0x01u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0xC8u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x10u, rx[1u]);

    semu_sapporo_opt3007_destroy(sensor);
}

static void test_wrong_address(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_opt3007 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_opt3007_create(0x45u, &error);
    ep = semu_sapporo_opt3007_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x46u, 0x00u, rx, 2u, &error));

    semu_sapporo_opt3007_destroy(sensor);
}

static void test_unknown_pointer(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_opt3007 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_opt3007_create(0x45u, &error);
    ep = semu_sapporo_opt3007_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x45u, 0x04u, rx, 2u, &error));

    semu_sapporo_opt3007_destroy(sensor);
}

static void test_result_write_refused(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_opt3007 *sensor;
    semu_serial_endpoint ep;
    uint8_t tx[] = { 0x00u, 0x01u, 0x02u };

    semu_error_clear(&error);
    sensor = semu_sapporo_opt3007_create(0x45u, &error);
    ep = semu_sapporo_opt3007_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0x45u, tx, 3u, &error));

    semu_sapporo_opt3007_destroy(sensor);
}

static void test_standard_register_defaults(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_opt3007 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_opt3007_create(0x45u, &error);
    ep = semu_sapporo_opt3007_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x45u, 0x02u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0xC0u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x00u, rx[1u]);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x45u, 0x03u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0xBFu, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0xFFu, rx[1u]);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x45u, 0x7Eu, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0x54u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x49u, rx[1u]);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x45u, 0x7Fu, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0x30u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x01u, rx[1u]);

    semu_sapporo_opt3007_destroy(sensor);
}

static void test_limit_register_write_and_config_mask(
    semu_test_context *context)
{
    semu_error error;
    semu_sapporo_opt3007 *sensor;
    semu_serial_endpoint ep;
    uint8_t tx[] = { 0x02u, 0x12u, 0x34u };
    uint8_t config_tx[] = { 0x01u, 0xFFu, 0xFFu };
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_opt3007_create(0x45u, &error);
    ep = semu_sapporo_opt3007_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x45u, tx, 3u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x45u, 0x02u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0x12u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x34u, rx[1u]);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x45u, config_tx, 3u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x45u, 0x01u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0xFEu, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x1Fu, rx[1u]);

    semu_sapporo_opt3007_destroy(sensor);
}

static void test_invalid_lengths_preserve_state(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_opt3007 *sensor;
    semu_serial_endpoint ep;
    uint8_t short_write[] = { 0x01u, 0x12u };
    uint8_t pointer = 0x01u;
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_opt3007_create(0x45u, &error);
    ep = semu_sapporo_opt3007_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0x45u, short_write, 2u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x45u, pointer, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x45u, 0x01u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0xC8u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x10u, rx[1u]);

    semu_sapporo_opt3007_destroy(sensor);
}

static void test_repeated_transcript(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_opt3007 *sensor;
    semu_serial_endpoint ep;
    uint8_t tx[] = { 0x01u, 0xC6u, 0x00u };
    uint8_t rx[2];
    unsigned i;

    semu_error_clear(&error);
    sensor = semu_sapporo_opt3007_create(0x45u, &error);
    ep = semu_sapporo_opt3007_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x45u, tx, 3u, &error));
    for (i = 0u; i < 2u; ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         do_read(&ep, 0x45u, 0x00u, rx, 2u, &error));
        SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
        SEMU_TEST_EQ_U64(context, 0x00u, rx[1u]);
    }

    semu_sapporo_opt3007_destroy(sensor);
}

static void test_snapshot_deterministic_state_refuses(
    semu_test_context *context)
{
    semu_error error;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_sapporo_opt3007 *source;
    semu_sapporo_opt3007 *target;

    semu_error_clear(&error);
    semu_snapshot_writer_init(&writer);
    source = semu_sapporo_opt3007_create(0x45u, &error);
    target = semu_sapporo_opt3007_create(0x45u, &error);
    SEMU_TEST_ASSERT(context, source != NULL && target != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_opt3007_snapshot_write(source, &writer,
                                                         &error));
    /* Snapshot layout is address, result, config, low limit, high limit. */
    writer.data[3u] ^= 0x10u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_sapporo_opt3007_snapshot_read(target, &reader,
                                                        &error));
    writer.data[3u] ^= 0x10u;
    writer.data[1u] = 1u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_sapporo_opt3007_snapshot_read(target, &reader,
                                                        &error));
    semu_snapshot_writer_destroy(&writer);
    semu_sapporo_opt3007_destroy(target);
    semu_sapporo_opt3007_destroy(source);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_result_read),
        SEMU_TEST_CASE(test_config_write_and_readback),
        SEMU_TEST_CASE(test_result_unaffected_by_config),
        SEMU_TEST_CASE(test_reset),
        SEMU_TEST_CASE(test_wrong_address),
        SEMU_TEST_CASE(test_unknown_pointer),
        SEMU_TEST_CASE(test_result_write_refused),
        SEMU_TEST_CASE(test_standard_register_defaults),
        SEMU_TEST_CASE(test_limit_register_write_and_config_mask),
        SEMU_TEST_CASE(test_invalid_lengths_preserve_state),
        SEMU_TEST_CASE(test_repeated_transcript),
        SEMU_TEST_CASE(test_snapshot_deterministic_state_refuses)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
