#include "test.h"

#include <stdio.h>
#include "sapporo_devices.h"
#include "sapporo_ohr2.h"

#include "semu/hash.h"
#include "semu/scheduler.h"
#include "semu/storage.h"

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
    devices = semu_sapporo_devices_create(scheduler, NULL, &error);
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

static void test_versioned_lps22_selection(semu_test_context *context)
{
    static const char *const common_profiles[] = {
        "sapporo-2.22.60", "sapporo-2.33.16"
    };
    semu_error error;
    semu_scheduler *scheduler;
    semu_sapporo_devices *devices;
    const semu_serial_endpoint *iom2;
    uint8_t selector = 0x0fu;
    uint8_t value = 0u;
    semu_serial_transaction transaction;
    size_t index;

    semu_error_clear(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    memset(&transaction, 0, sizeof(transaction));
    transaction.address = 0x5cu;
    transaction.tx = &selector;
    transaction.tx_size = 1u;
    transaction.rx = &value;
    transaction.rx_size = 1u;
    for (index = 0u; index < SEMU_ARRAY_LEN(common_profiles); ++index) {
        devices = semu_sapporo_devices_create(scheduler, NULL, &error);
        SEMU_TEST_ASSERT(context, devices != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_devices_select_profile(
                devices, common_profiles[index], &error));
        iom2 = semu_sapporo_devices_iom_endpoint(devices, 2u);
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
            iom2->transfer(iom2->context, &transaction, &error));
        semu_sapporo_devices_destroy(devices);
    }
    devices = semu_sapporo_devices_create(scheduler, NULL, &error);
    SEMU_TEST_ASSERT(context, devices != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        semu_sapporo_devices_select_profile(devices, "sapporo-9", &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_devices_select_profile(
            devices, "sapporo-2.39.20", &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_sapporo_devices_select_profile(
            devices, "sapporo-2.39.20", &error));
    iom2 = semu_sapporo_devices_iom_endpoint(devices, 2u);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        iom2->transfer(iom2->context, &transaction, &error));
    SEMU_TEST_EQ_U64(context, 0xb1u, value);
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
    devices = semu_sapporo_devices_create(scheduler, NULL, &error);
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

static void test_iom4_observed_endpoint(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_sapporo_devices *devices;
    const semu_serial_endpoint *iom4;
    uint8_t tx[4] = { 0x7fu, 0x01u, 0xd0u, 0xf0u };
    uint8_t rx = 0xffu;
    semu_serial_transaction txn;

    semu_error_clear(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    devices = semu_sapporo_devices_create(scheduler, NULL, &error);
    SEMU_TEST_ASSERT(context, devices != NULL);
    iom4 = semu_sapporo_devices_iom_endpoint(devices, 4u);
    SEMU_TEST_ASSERT(context, iom4 != NULL);

    memset(&txn, 0, sizeof(txn));
    txn.address = 0x28u;
    txn.tx = tx;
    txn.tx_size = sizeof(tx);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     iom4->transfer(iom4->context, &txn, &error));
    memset(&txn, 0, sizeof(txn));
    txn.address = 0x28u;
    txn.rx = &rx;
    txn.rx_size = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     iom4->transfer(iom4->context, &txn, &error));
    SEMU_TEST_EQ_U64(context, 0u, rx);
    txn.rx_size = 2u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     iom4->transfer(iom4->context, &txn, &error));

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
    uint8_t rx[58];
    uint32_t crc;
    semu_serial_transaction txn;
    size_t i;

    semu_error_clear(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    devices = semu_sapporo_devices_create(scheduler, NULL, &error);
    SEMU_TEST_ASSERT(context, devices != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_devices_select_profile(
            devices, "sapporo-2.22.60", &error));
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

    memset(tx, 0xff, sizeof(tx));
    tx[0u] = 0u;
    tx[1u] = 0x10u;
    tx[2u] = 0u;
    tx[3u] = 0u;
    tx[4u] = 0u;
    tx[5u] = 1u;
    crc = semu_crc32(0u, tx + 1u, 54u);
    tx[55u] = (uint8_t)crc;
    tx[56u] = (uint8_t)(crc >> 8u);
    tx[57u] = (uint8_t)(crc >> 16u);
    tx[58u] = (uint8_t)(crc >> 24u);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     iom2->transfer(iom2->context, &txn, &error));

    txn.address = 0x11u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     iom2->transfer(iom2->context, &txn, &error));

    semu_sapporo_devices_destroy(devices);

    devices = semu_sapporo_devices_create(scheduler, NULL, &error);
    SEMU_TEST_ASSERT(context, devices != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_devices_select_profile(
            devices, "sapporo-2.39.20", &error));
    iom2 = semu_sapporo_devices_iom_endpoint(devices, 2u);
    memset(tx, 0xff, sizeof(tx));
    tx[0u] = 0u;
    tx[1u] = 0x10u;
    tx[2u] = 0u;
    tx[3u] = 0u;
    tx[4u] = 0u;
    tx[5u] = 1u;
    crc = semu_crc32(0u, tx + 1u, 54u);
    tx[55u] = (uint8_t)crc;
    tx[56u] = (uint8_t)(crc >> 8u);
    tx[57u] = (uint8_t)(crc >> 16u);
    tx[58u] = (uint8_t)(crc >> 24u);
    txn.address = 0x10u;
    txn.tx = tx;
    txn.tx_size = sizeof(tx);
    txn.rx = NULL;
    txn.rx_size = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     iom2->transfer(iom2->context, &txn, &error));
    tx[0u] = SEMU_SAPPORO_OHR2_RESPONSE_SELECTOR;
    txn.tx_size = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     iom2->transfer(iom2->context, &txn, &error));
    memset(rx, 0xa5, sizeof(rx));
    txn.tx = NULL;
    txn.tx_size = 0u;
    txn.rx = rx;
    txn.rx_size = sizeof(rx);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     iom2->transfer(iom2->context, &txn, &error));
    SEMU_TEST_EQ_U64(context, 0x10u, rx[0u]);
    for (i = 4u; i < 54u; ++i) SEMU_TEST_EQ_U64(context, 0u, rx[i]);
    SEMU_TEST_EQ_U64(context, semu_crc32(0u, rx, 54u),
        (uint32_t)rx[54u] | (uint32_t)rx[55u] << 8u |
        (uint32_t)rx[56u] << 16u | (uint32_t)rx[57u] << 24u);
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
    devices = semu_sapporo_devices_create(scheduler, NULL, &error);
    SEMU_TEST_ASSERT(context, devices != NULL);
    mspi = semu_sapporo_devices_mspi_flash_endpoint(devices);
    SEMU_TEST_ASSERT(context, mspi != NULL);

    memset(&txn, 0, sizeof(txn));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     mspi->transfer(mspi->context, &txn, &error));

    semu_sapporo_devices_destroy(devices);
    semu_scheduler_destroy(scheduler);
}

static void test_mspi_flash_wired(semu_test_context *context)
{
    char path[128];
    FILE *stream;
    semu_error error;
    semu_storage *storage;
    semu_scheduler *scheduler;
    semu_sapporo_devices *devices;
    const semu_serial_endpoint *mspi;
    semu_serial_transaction txn;
    uint8_t id[3] = { 0u, 0u, 0u };
    const uint8_t command[] = { 0x9fu };

    SEMU_TEST_ASSERT(context,
        semu_test_temp_path(path, sizeof(path), "devices-flash.bin"));
    stream = fopen(path, "wb");
    SEMU_TEST_ASSERT(context, stream != NULL);
    SEMU_TEST_ASSERT(context, fputc(0xff, stream) != EOF);
    SEMU_TEST_ASSERT(context, fclose(stream) == 0);
    semu_error_clear(&error);
    storage = semu_storage_open(path, 0x02000000u, 0xffu, &error);
    SEMU_TEST_ASSERT(context, storage != NULL);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    devices = semu_sapporo_devices_create(scheduler, storage, &error);
    SEMU_TEST_ASSERT(context, devices != NULL);
    mspi = semu_sapporo_devices_mspi_flash_endpoint(devices);
    SEMU_TEST_ASSERT(context, mspi != NULL);

    memset(&txn, 0, sizeof(txn));
    txn.tx = command;
    txn.tx_size = sizeof(command);
    txn.rx = id;
    txn.rx_size = sizeof(id);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     mspi->transfer(mspi->context, &txn, &error));
    SEMU_TEST_EQ_U64(context, 0x20u, id[0u]);
    SEMU_TEST_EQ_U64(context, 0xbbu, id[1u]);
    SEMU_TEST_EQ_U64(context, 0x19u, id[2u]);

    semu_sapporo_devices_destroy(devices);
    semu_scheduler_destroy(scheduler);
    semu_storage_destroy(storage);
    (void)remove(path);
}

static void test_mspi1_completion_endpoint(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_sapporo_devices *devices;
    const semu_serial_endpoint *mspi1;
    semu_serial_transaction txn;
    uint8_t descriptor[16] = { 0 };

    semu_error_clear(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    devices = semu_sapporo_devices_create(scheduler, NULL, &error);
    SEMU_TEST_ASSERT(context, devices != NULL);
    mspi1 = semu_sapporo_devices_mspi1_endpoint(devices);
    SEMU_TEST_ASSERT(context, mspi1 != NULL);

    memset(&txn, 0, sizeof(txn));
    txn.tx = descriptor;
    txn.tx_size = sizeof(descriptor);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     mspi1->transfer(mspi1->context, &txn, &error));
    txn.tx_size = sizeof(descriptor) - 1u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     mspi1->transfer(mspi1->context, &txn, &error));

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
    devices = semu_sapporo_devices_create(scheduler, NULL, &error);
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
        SEMU_TEST_CASE(test_versioned_lps22_selection),
        SEMU_TEST_CASE(test_iom0_accelerometer),
        SEMU_TEST_CASE(test_iom4_observed_endpoint),
        SEMU_TEST_CASE(test_iom2_ohr2_via_mux),
        SEMU_TEST_CASE(test_mspi_flash_refuses),
        SEMU_TEST_CASE(test_mspi_flash_wired),
        SEMU_TEST_CASE(test_mspi1_completion_endpoint),
        SEMU_TEST_CASE(test_unknown_iom_refuses)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
