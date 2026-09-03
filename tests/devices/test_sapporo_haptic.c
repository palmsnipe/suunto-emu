#include "test.h"

#include <stdint.h>
#include <string.h>

#include "../../src/devices/sapporo_haptic.h"

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

static void test_autotune_trigger(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_haptic *sensor;
    semu_serial_endpoint ep;
    uint8_t tx[] = { 0x22u, 0x01u };
    uint8_t rx[1];

    semu_error_clear(&error);
    sensor = semu_sapporo_haptic_create(0x50u, &error);
    SEMU_TEST_ASSERT(context, sensor != NULL);
    ep = semu_sapporo_haptic_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x50u, tx, 2u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x50u, 0x22u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x03u, rx[0u]);

    semu_sapporo_haptic_destroy(sensor);
}

static void test_autotune_calibration_fixture(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_haptic *sensor;
    semu_serial_endpoint ep;
    uint8_t trigger[] = { 0x22u, 0x01u };
    uint8_t invalid[] = { 0x23u, 0x55u };
    uint8_t rx[2];

    semu_error_clear(&error);
    sensor = semu_sapporo_haptic_create(0x50u, &error);
    SEMU_TEST_ASSERT(context, sensor != NULL);
    ep = semu_sapporo_haptic_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x50u, trigger, sizeof(trigger), &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x50u, 0x23u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0u, rx[0u]);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x50u, 0x24u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0u, rx[0u]);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0x50u, invalid, sizeof(invalid), &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x50u, 0x23u, rx, sizeof(rx), &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x50u, 0x22u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x03u, rx[0u]);

    semu_sapporo_haptic_reset(sensor);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x50u, trigger, sizeof(trigger), &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x50u, 0x23u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0u, rx[0u]);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x50u, 0x24u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0u, rx[0u]);
    semu_sapporo_haptic_destroy(sensor);
}

static void test_idle_state(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_haptic *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[1];

    semu_error_clear(&error);
    sensor = semu_sapporo_haptic_create(0x50u, &error);
    ep = semu_sapporo_haptic_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x50u, 0x22u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);

    semu_sapporo_haptic_destroy(sensor);
}

static void test_reset(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_haptic *sensor;
    semu_serial_endpoint ep;
    uint8_t tx[] = { 0x22u, 0x01u };
    uint8_t rx[1];

    semu_error_clear(&error);
    sensor = semu_sapporo_haptic_create(0x50u, &error);
    ep = semu_sapporo_haptic_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x50u, tx, 2u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x50u, 0x22u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x03u, rx[0u]);

    semu_sapporo_haptic_reset(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x50u, 0x22u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);

    semu_sapporo_haptic_destroy(sensor);
}

static void test_wrong_address(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_haptic *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[1];

    semu_error_clear(&error);
    sensor = semu_sapporo_haptic_create(0x50u, &error);
    ep = semu_sapporo_haptic_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x51u, 0x22u, rx, 1u, &error));

    semu_sapporo_haptic_destroy(sensor);
}

static void test_configuration_registers(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_haptic *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[1];
    uint8_t tx[] = { 0x0du, 0x01u };

    semu_error_clear(&error);
    sensor = semu_sapporo_haptic_create(0x50u, &error);
    ep = semu_sapporo_haptic_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x50u, tx, 2u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x50u, 0x0du, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x01u, rx[0u]);

    semu_sapporo_haptic_destroy(sensor);
}

static void test_observed_status_read_only(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_haptic *sensor;
    semu_serial_endpoint ep;
    uint8_t tx[] = { 0x08u, 0x01u };
    uint8_t rx[1];

    semu_error_clear(&error);
    sensor = semu_sapporo_haptic_create(0x50u, &error);
    ep = semu_sapporo_haptic_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x50u, 0x08u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0x50u, tx, sizeof(tx), &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x50u, 0x08u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);

    semu_sapporo_haptic_destroy(sensor);
}

static void test_observed_waveform_selector(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_haptic *sensor;
    semu_serial_endpoint ep;
    uint8_t select[] = { 0x09u, 0x12u };
    uint8_t unknown[] = { 0x0au, 0x01u };
    uint8_t rx[1];

    semu_error_clear(&error);
    sensor = semu_sapporo_haptic_create(0x50u, &error);
    ep = semu_sapporo_haptic_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x50u, select, sizeof(select), &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x50u, 0x09u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x12u, rx[0u]);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0x50u, unknown, sizeof(unknown), &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x50u, 0x09u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x12u, rx[0u]);

    semu_sapporo_haptic_destroy(sensor);
}

static void test_write_without_trigger(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_haptic *sensor;
    semu_serial_endpoint ep;
    uint8_t tx[] = { 0x22u, 0x04u };
    uint8_t rx[1];

    semu_error_clear(&error);
    sensor = semu_sapporo_haptic_create(0x50u, &error);
    ep = semu_sapporo_haptic_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x50u, tx, 2u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x50u, 0x22u, rx, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0x04u, rx[0u]);

    semu_sapporo_haptic_destroy(sensor);
}

static void test_waveform_and_refusal_boundary(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_haptic *sensor;
    semu_serial_endpoint ep;
    uint8_t waveform[] = { 0x40u, 0x00u, 0x20u, 0x00u, 0x00u };
    uint8_t invalid[] = { 0x40u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u };
    uint8_t read_only[] = { 0x01u, 0xffu };
    uint8_t rx[4];

    semu_error_clear(&error);
    sensor = semu_sapporo_haptic_create(0x50u, &error);
    ep = semu_sapporo_haptic_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&ep, 0x50u, waveform, sizeof(waveform),
                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x50u, 0x40u, rx, sizeof(rx), &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x20u, rx[1u]);
    SEMU_TEST_EQ_U64(context, 0x00u, rx[2u]);
    SEMU_TEST_EQ_U64(context, 0x00u, rx[3u]);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0x50u, invalid, sizeof(invalid), &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_write(&ep, 0x50u, read_only, sizeof(read_only),
                              &error));

    semu_sapporo_haptic_destroy(sensor);
}

static void test_repeated_transcript(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_haptic *sensor;
    semu_serial_endpoint ep;
    uint8_t tx[] = { 0x22u, 0x01u };
    uint8_t rx[1];
    unsigned i;

    semu_error_clear(&error);
    sensor = semu_sapporo_haptic_create(0x50u, &error);
    ep = semu_sapporo_haptic_endpoint(sensor);

    for (i = 0u; i < 2u; ++i) {
        semu_sapporo_haptic_reset(sensor);
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         do_write(&ep, 0x50u, tx, 2u, &error));
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         do_read(&ep, 0x50u, 0x22u, rx, 1u, &error));
        SEMU_TEST_EQ_U64(context, 0x03u, rx[0u]);
    }

    semu_sapporo_haptic_destroy(sensor);
}

static void test_snapshot_cursor_and_atomic_refusal(
    semu_test_context *context)
{
    semu_error error;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_sapporo_haptic *source;
    semu_sapporo_haptic *target;
    semu_serial_endpoint source_ep;
    semu_serial_endpoint target_ep;
    semu_serial_transaction no_selector;
    uint8_t rx[1];
    uint8_t source_selector = 0x22u;
    uint8_t target_selector = 0x09u;

    semu_error_clear(&error);
    semu_snapshot_writer_init(&writer);
    source = semu_sapporo_haptic_create(0x50u, &error);
    target = semu_sapporo_haptic_create(0x50u, &error);
    source_ep = semu_sapporo_haptic_endpoint(source);
    target_ep = semu_sapporo_haptic_endpoint(target);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&source_ep, 0x50u, &source_selector, 1u,
                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_haptic_snapshot_write(
                         source, &writer, &error));
    SEMU_TEST_EQ_U64(context, 14u, writer.size);
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_haptic_snapshot_read(
                         target, &reader, &error));
    no_selector = (semu_serial_transaction){
        0x50u, 0u, NULL, 0u, rx, sizeof(rx)
    };
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     target_ep.transfer(target_ep.context, &no_selector,
                                        &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_write(&target_ep, 0x50u, &target_selector, 1u,
                              &error));
    writer.data[0u] = 0x51u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
                     semu_sapporo_haptic_snapshot_read(
                         target, &reader, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     target_ep.transfer(target_ep.context, &no_selector,
                                        &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);

    writer.data[0u] = 0x50u;
    writer.data[9u] = 0x01u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_sapporo_haptic_snapshot_read(
                         target, &reader, &error));
    semu_snapshot_writer_destroy(&writer);
    semu_sapporo_haptic_destroy(source);
    semu_sapporo_haptic_destroy(target);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_autotune_trigger),
        SEMU_TEST_CASE(test_autotune_calibration_fixture),
        SEMU_TEST_CASE(test_idle_state),
        SEMU_TEST_CASE(test_reset),
        SEMU_TEST_CASE(test_wrong_address),
        SEMU_TEST_CASE(test_configuration_registers),
        SEMU_TEST_CASE(test_observed_status_read_only),
        SEMU_TEST_CASE(test_observed_waveform_selector),
        SEMU_TEST_CASE(test_write_without_trigger),
        SEMU_TEST_CASE(test_waveform_and_refusal_boundary),
        SEMU_TEST_CASE(test_repeated_transcript),
        SEMU_TEST_CASE(test_snapshot_cursor_and_atomic_refusal)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
