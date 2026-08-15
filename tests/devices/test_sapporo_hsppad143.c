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
    uint8_t tx[] = { 0x00u, 0xAAu };
    uint8_t rx[1];

    semu_error_clear(&error);
    sensor = semu_sapporo_hsppad143_create(0x48u, &error);
    ep = semu_sapporo_hsppad143_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x48u, tx, 2u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x48u, 0x00u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0xAAu, rx[0u]);

    semu_sapporo_hsppad143_reset(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x48u, 0x00u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x49u, rx[0u]);

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

static void test_unknown_register(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_hsppad143 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[1];
    uint8_t tx[] = { 0x05u, 0x01u };

    semu_error_clear(&error);
    sensor = semu_sapporo_hsppad143_create(0x48u, &error);
    ep = semu_sapporo_hsppad143_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x48u, 0x05u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0x48u, tx, 2u, &error));

    semu_sapporo_hsppad143_destroy(sensor);
}

static void test_multi_byte_read_refuses_unknown(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_hsppad143 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_hsppad143_create(0x48u, &error);
    ep = semu_sapporo_hsppad143_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x48u, 0x00u, rx, 2u, &error));

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

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x48u, NULL, 0u, &error));

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

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_startup_transcript),
        SEMU_TEST_CASE(test_reset),
        SEMU_TEST_CASE(test_wrong_address),
        SEMU_TEST_CASE(test_unknown_register),
        SEMU_TEST_CASE(test_multi_byte_read_refuses_unknown),
        SEMU_TEST_CASE(test_empty_write),
        SEMU_TEST_CASE(test_repeated_transcript)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
