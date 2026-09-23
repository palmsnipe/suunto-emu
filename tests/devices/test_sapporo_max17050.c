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
    uint8_t rx[2] = { 0xaau, 0xbbu };

    semu_error_clear(&error);
    sensor = semu_sapporo_max17050_create(0x36u, &error);
    ep = semu_sapporo_max17050_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x36u, 0x22u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0xaau, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0xbbu, rx[1u]);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x36u, 0x06u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0x32u, rx[1u]);

    semu_sapporo_max17050_destroy(sensor);
}

/*
 * Unobserved selectors stay refused fail-closed even when the guest asks:
 * the 2.33.16 boot's 21st gauge transaction reads 0xF4, but under the
 * current lane fixture (E-SAP-MAX17050-001 lane table, pair
 * 9119ef13…, normalized sha in E-EMU-SAP233-GAUGE-FIXTURE-001) the boot
 * never issues that read; 0xF4 remains outside the pinned observed set.
 */
static void test_unobserved_f4_and_ff_refuse(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_max17050 *sensor;
    semu_serial_endpoint ep;
    uint8_t rx[2] = { 0xaau, 0xbbu };

    semu_error_clear(&error);
    sensor = semu_sapporo_max17050_create(0x36u, &error);
    ep = semu_sapporo_max17050_endpoint(sensor);

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x36u, 0xf4u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0xaau, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0xbbu, rx[1u]);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     do_read(&ep, 0x36u, 0xffu, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0xaau, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0xbbu, rx[1u]);

    semu_sapporo_max17050_destroy(sensor);
}

/*
 * Every selector in the pinned observed set must answer with the value of
 * the CURRENT lane fixture table (SapporoApollo4Iom4.cs:SapporoMax17050
 * Reset(), read-only lane sha b1d1dc8e…, reproduced twice via the
 * payload-bearing class-log census, pair normalized sha256
 * 9119ef13e60dbe71483f28bf02a962997252d7c476b4444a47f3fab9f06b6d76):
 * 0x00=0x0000 0x05=0x0000 0x06=0x3200 0x08=0x1900 0x09=0xC000
 * 0x0b=0x0000 0x10=0x0000 0x19=0xC000 0x1a=0x0000 0x21=0x0000
 * 0x28=0x0000 0x54=0x0000 0xec=0x0000.
 * This is the narrowest regression for the 2.33.16 first-fault fixture
 * divergence: 0x19 (AvgVCell) was pinned 0x0000 in-tree while the current
 * lane fixture answers 0xC000.
 */
static void test_lane_fixture_table(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_max17050 *sensor;
    semu_serial_endpoint ep;
    const uint8_t registers[] = {
        0x00u, 0x05u, 0x06u, 0x08u, 0x09u, 0x0bu, 0x10u,
        0x19u, 0x1au, 0x21u, 0x28u, 0x54u, 0xecu
    };
    const uint16_t values[] = {
        0x0000u, 0x0000u, 0x3200u, 0x1900u, 0xC000u, 0x0000u, 0x0000u,
        0xC000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u
    };
    uint8_t rx[2];
    size_t i;

    semu_error_clear(&error);
    sensor = semu_sapporo_max17050_create(0x36u, &error);
    SEMU_TEST_ASSERT(context, sensor != NULL);
    ep = semu_sapporo_max17050_endpoint(sensor);
    for (i = 0u; i < sizeof(registers) / sizeof(registers[0]); ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         do_read(&ep, 0x36u, registers[i], rx, 2u, &error));
        SEMU_TEST_EQ_U64(context, (uint8_t)values[i], rx[0u]);
        SEMU_TEST_EQ_U64(context, (uint8_t)(values[i] >> 8u), rx[1u]);
    }

    semu_sapporo_max17050_reset(sensor);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     do_read(&ep, 0x36u, 0x19u, rx, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0x00u, rx[0u]);
    SEMU_TEST_EQ_U64(context, 0xC0u, rx[1u]);

    semu_sapporo_max17050_destroy(sensor);
}

static void test_observed_later_registers(semu_test_context *context)
{
    semu_error error;
    semu_sapporo_max17050 *sensor;
    semu_serial_endpoint ep;
    const uint8_t registers[] = {
        0x05u, 0x0bu, 0x10u, 0x19u, 0x1au, 0x21u, 0x28u, 0x54u, 0xecu
    };
    const uint16_t values[] = {
        0x0000u, 0x0000u, 0x0000u, 0xC000u, 0x0000u,
        0x0000u, 0x0000u, 0x0000u, 0x0000u
    };
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

static void test_snapshot_fixture_values_refuse(semu_test_context *context)
{
    semu_error error;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_sapporo_max17050 *source;
    semu_sapporo_max17050 *target;
    const size_t value_offsets[] = { 1u, 3u, 5u, 7u };
    size_t i;

    semu_error_clear(&error);
    semu_snapshot_writer_init(&writer);
    source = semu_sapporo_max17050_create(0x36u, &error);
    target = semu_sapporo_max17050_create(0x36u, &error);
    SEMU_TEST_ASSERT(context, source != NULL && target != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_max17050_snapshot_write(source, &writer,
                                                          &error));
    for (i = 0u; i < sizeof(value_offsets) / sizeof(value_offsets[0]); ++i) {
        writer.data[value_offsets[i]] ^= 0x01u;
        semu_snapshot_reader_init(&reader, writer.data, writer.size);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                         semu_sapporo_max17050_snapshot_read(target, &reader,
                                                             &error));
        writer.data[value_offsets[i]] ^= 0x01u;
    }
    semu_snapshot_writer_destroy(&writer);
    semu_sapporo_max17050_destroy(target);
    semu_sapporo_max17050_destroy(source);
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
        SEMU_TEST_CASE(test_unobserved_f4_and_ff_refuse),
        SEMU_TEST_CASE(test_lane_fixture_table),
        SEMU_TEST_CASE(test_observed_later_registers),
        SEMU_TEST_CASE(test_byte_order),
        SEMU_TEST_CASE(test_write_and_shape_refusals),
        SEMU_TEST_CASE(test_repeated_transcript),
        SEMU_TEST_CASE(test_snapshot_fixture_values_refuse)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
