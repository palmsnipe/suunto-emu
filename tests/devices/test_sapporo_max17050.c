#include "test.h"

#include <stdint.h>
#include <string.h>

#include "../../src/devices/sapporo_max17050.h"

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

static void test_status_read(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_max17050 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_max17050_create(0x36u, &error);
    SEMU_TEST_ASSERT(context, sensor != NULL);
    ep = semu_sapporo_max17050_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x36u, 0x00u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x00u, rx[1u]);

    semu_sapporo_max17050_destroy(sensor);
}

static void test_vcell_read(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_max17050 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_max17050_create(0x36u, &error);
    ep = semu_sapporo_max17050_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x36u, 0x09u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0xC0u, rx[1u]);

    semu_sapporo_max17050_destroy(sensor);
}

static void test_repsoc_read(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_max17050 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_max17050_create(0x36u, &error);
    ep = semu_sapporo_max17050_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x36u, 0x06u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x32u, rx[1u]);

    semu_sapporo_max17050_destroy(sensor);
}

static void test_temperature_read(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_max17050 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_max17050_create(0x36u, &error);
    ep = semu_sapporo_max17050_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x36u, 0x08u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x19u, rx[1u]);

    semu_sapporo_max17050_destroy(sensor);
}

static void test_reset(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_max17050 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_max17050_create(0x36u, &error);
    ep = semu_sapporo_max17050_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x36u, 0x06u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x32u, rx[1u]);

    semu_sapporo_max17050_reset(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x36u, 0x09u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0xC0u, rx[1u]);

    semu_sapporo_max17050_destroy(sensor);
}

static void test_wrong_address(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_max17050 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_max17050_create(0x36u, &error);
    ep = semu_sapporo_max17050_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x37u, 0x00u, rx, 2u, &error));

    semu_sapporo_max17050_destroy(sensor);
}

static void test_unknown_register_refusal(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_max17050 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_max17050_create(0x36u, &error);
    ep = semu_sapporo_max17050_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x36u, 0x22u, rx, 2u, &error));

    semu_sapporo_max17050_destroy(sensor);
}

static void test_observed_later_registers(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_max17050 *sensor;
    semu_serial_endpoint ep;
    const uint8_t registers[] = { 0x0bu, 0x19u, 0x21u };
    const uint16_t values[] = { 0x0000u, 0xC000u, 0x0000u };
    uint8_t rx[2];
    size_t i;

    semu_error_clear(&error);
    sensor = semu_sapporo_max17050_create(0x36u, &error);
    ep = semu_sapporo_max17050_endpoint(sensor);
    for (i = 0u; i < sizeof(registers) / sizeof(registers[0]); ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         do_read(&ep, 0x36u, registers[i], rx, 2u,
                                 &error));
        SEMU_TEST_EQ_U64(context, (uint8_t)values[i], rx[0u]);
        SEMU_TEST_EQ_U64(context, (uint8_t)(values[i] >> 8u), rx[1u]);
    }

    semu_sapporo_max17050_destroy(sensor);
}

static void test_byte_order(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_max17050 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_max17050_create(0x36u, &error);
    ep = semu_sapporo_max17050_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x36u, 0x09u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0xC0u, rx[1u]);

    semu_sapporo_max17050_destroy(sensor);
}

static void test_write_and_shape_refusals(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_max17050 *sensor;
    semu_serial_endpoint ep;
    uint8_t write_frame[] = { 0x00u, 0x34u, 0x12u };
    uint8_t rx[2] = { 0xaau, 0xbbu };
    semu_serial_transaction malformed = {
        0x36u, 0u, NULL, 1u, rx, sizeof(rx)
    };

    semu_error_clear(&error);
    sensor = semu_sapporo_max17050_create(0x36u, &error);
    ep = semu_sapporo_max17050_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0x36u, write_frame,
                              sizeof(write_frame), &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     ep.transfer(ep.context, &malformed, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x36u, 0x00u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x36u, 0x00u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x00u, rx[1u]);

    semu_sapporo_max17050_destroy(sensor);
}

static void test_repeated_transcript(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_max17050 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2];
    unsigned i;

    semu_error_clear(&error);
    sensor = semu_sapporo_max17050_create(0x36u, &error);
    ep = semu_sapporo_max17050_endpoint(sensor);

    for (i = 0u; i < 2u; ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         do_read(&ep, 0x36u, 0x00u, rx, 2u, &error));
        SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
        SEMU_TEST_EQ_U64(context, 0x00u, rx[1u]);
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         do_read(&ep, 0x36u, 0x09u, rx, 2u, &error));
        SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
        SEMU_TEST_EQ_U64(context, 0xC0u, rx[1u]);
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         do_read(&ep, 0x36u, 0x06u, rx, 2u, &error));
        SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
        SEMU_TEST_EQ_U64(context, 0x32u, rx[1u]);
    }

    semu_sapporo_max17050_destroy(sensor);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_status_read),
        SEMU_TEST_CASE(test_vcell_read),
        SEMU_TEST_CASE(test_repsoc_read),
        SEMU_TEST_CASE(test_temperature_read),
        SEMU_TEST_CASE(test_reset),
        SEMU_TEST_CASE(test_wrong_address),
        SEMU_TEST_CASE(test_unknown_register_refusal),
        SEMU_TEST_CASE(test_observed_later_registers),
        SEMU_TEST_CASE(test_byte_order),
        SEMU_TEST_CASE(test_write_and_shape_refusals),
        SEMU_TEST_CASE(test_repeated_transcript)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
