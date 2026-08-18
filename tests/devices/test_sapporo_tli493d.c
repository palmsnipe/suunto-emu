#include "test.h"

#include <stdint.h>
#include <string.h>

#include "../../src/devices/sapporo_tli493d.h"

static semu_transaction_result do_read(semu_serial_endpoint *ep,
                                       uint8_t address,
                                       uint8_t *rx, size_t rx_size,
                                       semu_error *error)
{
    semu_serial_transaction t = { address, 0u, NULL, 0u, rx, rx_size };
    return ep->transfer(ep->context, &t, error);
}

static semu_transaction_result do_write(semu_serial_endpoint *ep,
                                        uint8_t address, const uint8_t *tx,
                                        size_t tx_size, semu_error *error)
{
    semu_serial_transaction t = { address, 0u, tx, tx_size, NULL, 0u };
    return ep->transfer(ep->context, &t, error);
}

static void test_initialization_read(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_tli493d *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[23];

    semu_error_clear(&error);
    sensor = semu_sapporo_tli493d_create(0x35u, &error);
    SEMU_TEST_ASSERT(context, sensor != NULL);
    ep = semu_sapporo_tli493d_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x35u, rx, 23u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x44u, rx[6u]);

    semu_sapporo_tli493d_destroy(sensor);
}

static void test_config_write_and_readback(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_tli493d *sensor;
    semu_serial_endpoint ep;
    uint8_t tx_mod1[] = { 0x11u, 0x9du };
    uint8_t tx_cfg[] = { 0x10u, 0xa8u };
    uint8_t rx[23];

    semu_error_clear(&error);
    sensor = semu_sapporo_tli493d_create(0x35u, &error);
    ep = semu_sapporo_tli493d_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x35u, tx_mod1, 2u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x35u, tx_cfg, 2u, &error));

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x35u, rx, 23u, &error));
    SEMU_TEST_EQ_U64(context, 0xa8u, rx[0x10u]);
    SEMU_TEST_EQ_U64(context, 0x9du, rx[0x11u]);

    semu_sapporo_tli493d_destroy(sensor);
}

static void test_sample_read(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_tli493d *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[7];

    semu_error_clear(&error);
    sensor = semu_sapporo_tli493d_create(0x35u, &error);
    ep = semu_sapporo_tli493d_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x35u, rx, sizeof(rx), &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x00u, rx[1u]);
    SEMU_TEST_EQ_U64(context, 0x00u, rx[2u]);
    SEMU_TEST_EQ_U64(context, 0x00u, rx[3u]);
    SEMU_TEST_EQ_U64(context, 0x00u, rx[4u]);
    SEMU_TEST_EQ_U64(context, 0x00u, rx[5u]);
    SEMU_TEST_EQ_U64(context, 0x44u, rx[6u]);

    semu_sapporo_tli493d_destroy(sensor);
}

static void test_reset(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_tli493d *sensor;
    semu_serial_endpoint ep;
    uint8_t tx[] = { 0x10u, 0xFFu };
    uint8_t rx[23];

    semu_error_clear(&error);
    sensor = semu_sapporo_tli493d_create(0x35u, &error);
    ep = semu_sapporo_tli493d_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x35u, tx, 2u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x35u, rx, 23u, &error));
    SEMU_TEST_EQ_U64(context, 0xFFu, rx[0x10u]);

    semu_sapporo_tli493d_reset(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x35u, rx, 23u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0x10u]);
    SEMU_TEST_EQ_U64(context, 0x44u, rx[0x06u]);

    semu_sapporo_tli493d_destroy(sensor);
}

static void test_wrong_address(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_tli493d *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[1];

    semu_error_clear(&error);
    sensor = semu_sapporo_tli493d_create(0x35u, &error);
    ep = semu_sapporo_tli493d_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x36u, rx, 1u, &error));

    semu_sapporo_tli493d_destroy(sensor);
}

static void test_register_boundary_22_23(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_tli493d *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[24];

    semu_error_clear(&error);
    sensor = semu_sapporo_tli493d_create(0x35u, &error);
    ep = semu_sapporo_tli493d_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x35u, rx, 23u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x35u, rx, 24u, &error));

    semu_sapporo_tli493d_destroy(sensor);
}

static void test_unknown_write_register(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_tli493d *sensor;
    semu_serial_endpoint ep;
    uint8_t tx[] = { 0x05u, 0x01u };

    semu_error_clear(&error);
    sensor = semu_sapporo_tli493d_create(0x35u, &error);
    ep = semu_sapporo_tli493d_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0x35u, tx, 2u, &error));

    semu_sapporo_tli493d_destroy(sensor);
}

static void test_invalid_frames_are_atomic(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_tli493d *sensor;
    semu_serial_endpoint ep;
    uint8_t valid[] = { 0x11u, 0x9du };
    uint8_t too_long[] = { 0x11u, 0x22u, 0x33u };
    uint8_t aliased[] = { 0x30u, 0x55u };
    uint8_t rx[23];
    semu_serial_transaction malformed_read = {
        0x35u, 0u, NULL, 0u, NULL, 1u
    };

    semu_error_clear(&error);
    sensor = semu_sapporo_tli493d_create(0x35u, &error);
    ep = semu_sapporo_tli493d_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x35u, valid, sizeof(valid), &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0x35u, too_long, sizeof(too_long),
                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0x35u, aliased, sizeof(aliased), &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     ep.transfer(ep.context, &malformed_read, &error));

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x35u, rx, sizeof(rx), &error));
    SEMU_TEST_EQ_U64(context, 0x9du, rx[0x11u]);
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0x10u]);

    semu_sapporo_tli493d_destroy(sensor);
}

static void test_unsupported_read_lengths_refuse(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_tli493d *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[24];

    semu_error_clear(&error);
    sensor = semu_sapporo_tli493d_create(0x35u, &error);
    ep = semu_sapporo_tli493d_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x35u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x35u, rx, 8u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x35u, rx, sizeof(rx), &error));

    semu_sapporo_tli493d_destroy(sensor);
}

static void test_repeated_transcript(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_tli493d *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[23];
    unsigned i;

    semu_error_clear(&error);
    sensor = semu_sapporo_tli493d_create(0x35u, &error);
    ep = semu_sapporo_tli493d_endpoint(sensor);

    for (i = 0u; i < 2u; ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         do_read(&ep, 0x35u, rx, 23u, &error));
        SEMU_TEST_EQ_U64(context, 0x44u, rx[6u]);
    }

    semu_sapporo_tli493d_destroy(sensor);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_initialization_read),
        SEMU_TEST_CASE(test_config_write_and_readback),
        SEMU_TEST_CASE(test_sample_read),
        SEMU_TEST_CASE(test_reset),
        SEMU_TEST_CASE(test_wrong_address),
        SEMU_TEST_CASE(test_register_boundary_22_23),
        SEMU_TEST_CASE(test_unknown_write_register),
        SEMU_TEST_CASE(test_invalid_frames_are_atomic),
        SEMU_TEST_CASE(test_unsupported_read_lengths_refuse),
        SEMU_TEST_CASE(test_repeated_transcript)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
