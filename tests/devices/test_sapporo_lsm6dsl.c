#include "test.h"

#include <stdint.h>
#include <string.h>

#include "../../src/devices/sapporo_lsm6dsl.h"

static semu_transaction_result do_read(semu_serial_endpoint *ep,
                                       uint8_t cs, uint8_t cmd,
                                       uint8_t *rx, size_t rx_size,
                                       semu_error *error)
{
    uint8_t tx = cmd;
    semu_serial_transaction t = { 0u, cs, &tx, 1u, rx, rx_size };
    return ep->transfer(ep->context, &t, error);
}

static semu_transaction_result do_read_phase2(semu_serial_endpoint *ep,
                                               uint8_t cs,
                                               uint8_t *rx, size_t rx_size,
                                               semu_error *error)
{
    semu_serial_transaction t = { 0u, cs, NULL, 0u, rx, rx_size };
    return ep->transfer(ep->context, &t, error);
}

static semu_transaction_result do_write(semu_serial_endpoint *ep,
                                         uint8_t cs, const uint8_t *tx,
                                         size_t tx_size, semu_error *error)
{
    semu_serial_transaction t = { 0u, cs, tx, tx_size, NULL, 0u };
    return ep->transfer(ep->context, &t, error);
}

static void test_who_am_i(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_lsm6dsl *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[1];

    semu_error_clear(&error);
    sensor = semu_sapporo_lsm6dsl_create(0u, &error);
    SEMU_TEST_ASSERT(context, sensor != NULL);
    ep = semu_sapporo_lsm6dsl_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0u, 0x8fu, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x6au, rx[0u]);

    semu_sapporo_lsm6dsl_destroy(sensor);
}

static void test_fifo_status(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_lsm6dsl *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_lsm6dsl_create(0u, &error);
    ep = semu_sapporo_lsm6dsl_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0u, 0xbau, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x10u, rx[1u]);

    semu_sapporo_lsm6dsl_destroy(sensor);
}

static void test_two_phase_read(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_lsm6dsl *sensor;
    semu_serial_endpoint ep;
    uint8_t cmd = 0x8fu;
    uint8_t rx[1];

    semu_error_clear(&error);
    sensor = semu_sapporo_lsm6dsl_create(0u, &error);
    ep = semu_sapporo_lsm6dsl_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0u, &cmd, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read_phase2(&ep, 0u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x6au, rx[0u]);

    semu_sapporo_lsm6dsl_destroy(sensor);
}

static void test_reset(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_lsm6dsl *sensor;
    semu_serial_endpoint ep;
    uint8_t tx[] = { 0x12u, 0xAAu };
    uint8_t rx[1];

    semu_error_clear(&error);
    sensor = semu_sapporo_lsm6dsl_create(0u, &error);
    ep = semu_sapporo_lsm6dsl_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0u, tx, 2u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0u, 0x92u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0xAAu, rx[0u]);

    semu_sapporo_lsm6dsl_reset(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0u, 0x92u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x04u, rx[0u]);

    semu_sapporo_lsm6dsl_destroy(sensor);
}

static void test_wrong_chip_select(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_lsm6dsl *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[1];

    semu_error_clear(&error);
    sensor = semu_sapporo_lsm6dsl_create(0u, &error);
    ep = semu_sapporo_lsm6dsl_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 1u, 0x8fu, rx, 1u, &error));

    semu_sapporo_lsm6dsl_destroy(sensor);
}

static void test_configuration_register(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_lsm6dsl *sensor;
    semu_serial_endpoint ep;
    uint8_t tx[] = { 0x12u, 0x42u };
    uint8_t rx[1];

    semu_error_clear(&error);
    sensor = semu_sapporo_lsm6dsl_create(0u, &error);
    ep = semu_sapporo_lsm6dsl_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0u, tx, sizeof(tx), &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0u, 0x92u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x42u, rx[0u]);

    semu_sapporo_lsm6dsl_destroy(sensor);
}

static void test_observed_startup_configuration(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_lsm6dsl *sensor;
    semu_serial_endpoint ep;
    const uint8_t writes[][6] = {
        { 0x11u, 0x00u },
        { 0x15u, 0x00u },
        { 0x12u, 0x01u },
        { 0x06u, 0x18u, 0x00u, 0x01u, 0x01u, 0x1eu },
        { 0x1bu, 0x01u },
        { 0x1cu, 0x00u },
        { 0x1eu, 0x00u },
        { 0x1eu, 0x20u }
    };
    const size_t sizes[] = { 2u, 2u, 2u, 6u, 2u, 2u, 2u, 2u };
    uint8_t invalid[] = { 0x1fu, 0xa5u };
    uint8_t read_command = 0x91u;
    uint8_t later_read_command = 0x9eu;
    uint8_t rx[1];
    size_t i;

    semu_error_clear(&error);
    sensor = semu_sapporo_lsm6dsl_create(0u, &error);
    ep = semu_sapporo_lsm6dsl_endpoint(sensor);
    for (i = 0u; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         do_write(&ep, 0u, writes[i], sizes[i], &error));
    }
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0u, read_command, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0u, later_read_command, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x20u, rx[0u]);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0u, invalid, sizeof(invalid), &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0u, later_read_command, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x20u, rx[0u]);

    semu_sapporo_lsm6dsl_destroy(sensor);
}

static void test_wrong_direction(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_lsm6dsl *sensor;
    semu_serial_endpoint ep;
    uint8_t tx_write_with_read[] = { 0x8fu, 0x01u };

    semu_error_clear(&error);
    sensor = semu_sapporo_lsm6dsl_create(0u, &error);
    ep = semu_sapporo_lsm6dsl_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0u, tx_write_with_read, 2u, &error));

    semu_sapporo_lsm6dsl_destroy(sensor);
}

static void test_register_refusals_are_atomic(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_lsm6dsl *sensor;
    semu_serial_endpoint ep;
    uint8_t read_only[] = { 0x0fu, 0xaau };
    uint8_t invalid_burst[] = { 0x5fu, 0x11u, 0x22u };
    uint8_t rx[2] = { 0xaau, 0xbbu };

    semu_error_clear(&error);
    sensor = semu_sapporo_lsm6dsl_create(0u, &error);
    ep = semu_sapporo_lsm6dsl_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0u, read_only, sizeof(read_only), &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0u, 0x8bu, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0u, invalid_burst,
                              sizeof(invalid_burst), &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0u, 0xbfu, rx, sizeof(rx), &error));

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0u, 0x8fu, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x6au, rx[0u]);

    semu_sapporo_lsm6dsl_destroy(sensor);
}

static void test_repeated_transcript(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_lsm6dsl *sensor;
    semu_serial_endpoint ep;
    uint8_t rx_id[1], rx_fifo[2];
    unsigned i;

    semu_error_clear(&error);
    sensor = semu_sapporo_lsm6dsl_create(0u, &error);
    ep = semu_sapporo_lsm6dsl_endpoint(sensor);

    for (i = 0u; i < 2u; ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         do_read(&ep, 0u, 0x8fu, rx_id, 1u, &error));
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         do_read(&ep, 0u, 0xbau, rx_fifo, 2u, &error));
        SEMU_TEST_EQ_U64(context, 0x6au, rx_id[0u]);
        SEMU_TEST_EQ_U64(context, 0x00u, rx_fifo[0u]);
        SEMU_TEST_EQ_U64(context, 0x10u, rx_fifo[1u]);
    }

    semu_sapporo_lsm6dsl_destroy(sensor);
}

static void test_snapshot_command_flags_refuse(semu_test_context *context)
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
    /* Snapshot layout: chip-select, 0x1f config bytes, register, then flags. */
    writer.data[34u] = 1u;
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
        SEMU_TEST_CASE(test_who_am_i),
        SEMU_TEST_CASE(test_fifo_status),
        SEMU_TEST_CASE(test_two_phase_read),
        SEMU_TEST_CASE(test_reset),
        SEMU_TEST_CASE(test_wrong_chip_select),
        SEMU_TEST_CASE(test_configuration_register),
        SEMU_TEST_CASE(test_observed_startup_configuration),
        SEMU_TEST_CASE(test_wrong_direction),
        SEMU_TEST_CASE(test_register_refusals_are_atomic),
        SEMU_TEST_CASE(test_repeated_transcript),
        SEMU_TEST_CASE(test_snapshot_command_flags_refuse)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
