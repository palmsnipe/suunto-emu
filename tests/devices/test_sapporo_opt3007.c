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
    SEMU_TEST_EQ_U64(context, 0xC6u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x00u, rx[1u]);

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
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x00u, rx[1u]);

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
                     do_read(&ep, 0x45u, 0x02u, rx, 2u, &error));

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
        SEMU_TEST_CASE(test_repeated_transcript)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
