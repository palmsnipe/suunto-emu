#include "test.h"

#include "sapporo_devices.h"

#include "semu/hash.h"
#include "semu/scheduler.h"

#include <string.h>

static void test_iom2_pressure_via_mux(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_sapporo_devices *devices;
    const semu_serial_endpoint *iom2;
    uint8_t tx[] = { 0x00u };
    uint8_t rx[1];
    semu_serial_transaction txn;

    semu_error_clear(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    devices = semu_sapporo_devices_create(scheduler, &error);
    SEMU_TEST_ASSERT(context, devices != NULL);
    iom2 = semu_sapporo_devices_iom_endpoint(devices, 2u);
    SEMU_TEST_ASSERT(context, iom2 != NULL);

    memset(&txn, 0, sizeof(txn));
    txn.address = 0x48u;
    txn.tx = tx;
    txn.tx_size = 1u;
    txn.rx = rx;
    txn.rx_size = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     iom2->transfer(iom2->context, &txn, &error));
    SEMU_TEST_EQ_U64(context, 0x49u, rx[0u]);

    txn.address = 0x49u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     iom2->transfer(iom2->context, &txn, &error));

    semu_sapporo_devices_destroy(devices);
    semu_scheduler_destroy(scheduler);
}

static void test_iom0_accelerometer(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_sapporo_devices *devices;
    const semu_serial_endpoint *iom0;
    uint8_t tx[] = { 0x8fu };
    uint8_t rx[1];
    semu_serial_transaction txn;

    semu_error_clear(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    devices = semu_sapporo_devices_create(scheduler, &error);
    SEMU_TEST_ASSERT(context, devices != NULL);
    iom0 = semu_sapporo_devices_iom_endpoint(devices, 0u);
    SEMU_TEST_ASSERT(context, iom0 != NULL);

    memset(&txn, 0, sizeof(txn));
    txn.tx = tx;
    txn.tx_size = 1u;
    txn.rx = rx;
    txn.rx_size = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     iom0->transfer(iom0->context, &txn, &error));
    SEMU_TEST_EQ_U64(context, 0x6au, rx[0u]);

    semu_sapporo_devices_destroy(devices);
    semu_scheduler_destroy(scheduler);
}

static void test_iom2_ohr2_via_mux(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_sapporo_devices *devices;
    const semu_serial_endpoint *iom2;
    uint8_t tx[59];
    uint32_t crc;
    semu_serial_transaction txn;

    semu_error_clear(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    devices = semu_sapporo_devices_create(scheduler, &error);
    SEMU_TEST_ASSERT(context, devices != NULL);
    iom2 = semu_sapporo_devices_iom_endpoint(devices, 2u);
    SEMU_TEST_ASSERT(context, iom2 != NULL);

    memset(tx, 0, sizeof(tx));
    tx[1u] = 0u;
    tx[3u] = 0u;
    crc = semu_crc32(0u, tx + 1u, 54u);
    tx[55u] = (uint8_t)crc;
    tx[56u] = (uint8_t)(crc >> 8u);
    tx[57u] = (uint8_t)(crc >> 16u);
    tx[58u] = (uint8_t)(crc >> 24u);

    memset(&txn, 0, sizeof(txn));
    txn.address = 0x10u;
    txn.tx = tx;
    txn.tx_size = 59u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     iom2->transfer(iom2->context, &txn, &error));

    txn.address = 0x11u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     iom2->transfer(iom2->context, &txn, &error));

    semu_sapporo_devices_destroy(devices);
    semu_scheduler_destroy(scheduler);
}

static void test_mspi_flash_refuses(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_sapporo_devices *devices;
    const semu_serial_endpoint *mspi;
    semu_serial_transaction txn;

    semu_error_clear(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    devices = semu_sapporo_devices_create(scheduler, &error);
    SEMU_TEST_ASSERT(context, devices != NULL);
    mspi = semu_sapporo_devices_mspi_flash_endpoint(devices);
    SEMU_TEST_ASSERT(context, mspi != NULL);

    memset(&txn, 0, sizeof(txn));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     mspi->transfer(mspi->context, &txn, &error));

    semu_sapporo_devices_destroy(devices);
    semu_scheduler_destroy(scheduler);
}

static void test_unknown_iom_refuses(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_sapporo_devices *devices;
    const semu_serial_endpoint *iom6;
    semu_serial_transaction txn;

    semu_error_clear(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    devices = semu_sapporo_devices_create(scheduler, &error);
    SEMU_TEST_ASSERT(context, devices != NULL);
    iom6 = semu_sapporo_devices_iom_endpoint(devices, 6u);
    SEMU_TEST_ASSERT(context, iom6 != NULL);

    memset(&txn, 0, sizeof(txn));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     iom6->transfer(iom6->context, &txn, &error));

    semu_sapporo_devices_destroy(devices);
    semu_scheduler_destroy(scheduler);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_iom2_pressure_via_mux),
        SEMU_TEST_CASE(test_iom0_accelerometer),
        SEMU_TEST_CASE(test_iom2_ohr2_via_mux),
        SEMU_TEST_CASE(test_mspi_flash_refuses),
        SEMU_TEST_CASE(test_unknown_iom_refuses)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
