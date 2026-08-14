#include "sapporo_devices.h"

#include "semu/hash.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

static void test_pressure(void)
{
    semu_error error;
    semu_sapporo_device *device;
    semu_serial_endpoint endpoint;
    uint8_t tx[] = { 0x00u };
    uint8_t rx[2];
    semu_serial_transaction transaction = { 0x48u, 0u, tx, 1u, rx, 2u };
    semu_error_clear(&error);
    device = semu_sapporo_device_create(SEMU_SAPPORO_PRESSURE, &error);
    CHECK(device != NULL);
    endpoint = semu_sapporo_device_endpoint(device);
    CHECK(endpoint.transfer(endpoint.context, &transaction, &error) ==
          SEMU_TRANSACTION_OK);
    CHECK(rx[0] == 0x49u);
    transaction.address = 0x49u;
    CHECK(endpoint.transfer(endpoint.context, &transaction, &error) ==
          SEMU_TRANSACTION_REFUSE);
    semu_sapporo_device_destroy(device);
}

static void test_accelerometer(void)
{
    semu_error error;
    semu_sapporo_device *device;
    semu_serial_endpoint endpoint;
    uint8_t tx[] = { 0x8fu };
    uint8_t rx[1];
    semu_serial_transaction transaction = { 0u, 0u, tx, 1u, rx, 1u };
    semu_error_clear(&error);
    device = semu_sapporo_device_create(SEMU_SAPPORO_ACCELEROMETER, &error);
    endpoint = semu_sapporo_device_endpoint(device);
    CHECK(endpoint.transfer(endpoint.context, &transaction, &error) ==
          SEMU_TRANSACTION_OK);
    CHECK(rx[0] == 0x6au);
    semu_sapporo_device_destroy(device);
}

static void test_ohr_refusal(void)
{
    semu_error error;
    semu_sapporo_device *device;
    semu_serial_endpoint endpoint;
    uint8_t tx[59];
    uint8_t rx[58];
    uint32_t crc;
    semu_serial_transaction transaction = { 0x10u, 0u, tx, 59u, rx, 58u };
    memset(tx, 0, sizeof(tx));
    tx[1] = 0u;
    tx[3] = 1u;
    crc = semu_crc32(0u, tx, 55u);
    tx[55] = (uint8_t)crc;
    tx[56] = (uint8_t)(crc >> 8u);
    tx[57] = (uint8_t)(crc >> 16u);
    tx[58] = (uint8_t)(crc >> 24u);
    semu_error_clear(&error);
    device = semu_sapporo_device_create(SEMU_SAPPORO_OHR2, &error);
    endpoint = semu_sapporo_device_endpoint(device);
    CHECK(endpoint.transfer(endpoint.context, &transaction, &error) ==
          SEMU_TRANSACTION_OK);
    CHECK(memcmp(rx + 9u, "BSL", 3u) == 0);
    tx[1] = 99u;
    crc = semu_crc32(0u, tx, 55u);
    tx[55] = (uint8_t)crc;
    tx[56] = (uint8_t)(crc >> 8u);
    tx[57] = (uint8_t)(crc >> 16u);
    tx[58] = (uint8_t)(crc >> 24u);
    CHECK(endpoint.transfer(endpoint.context, &transaction, &error) ==
          SEMU_TRANSACTION_REFUSE);
    semu_sapporo_device_destroy(device);
}

int main(void)
{
    test_pressure();
    test_accelerometer();
    test_ohr_refusal();
    if (failures != 0) {
        fprintf(stderr, "%d device test(s) failed\n", failures);
        return 1;
    }
    puts("sapporo device tests passed");
    return 0;
}
