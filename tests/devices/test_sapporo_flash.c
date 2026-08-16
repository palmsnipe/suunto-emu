#include "test.h"

#include <stdio.h>
#include <string.h>

#include "sapporo_flash.h"
#include "semu/peripheral.h"
#include "semu/storage.h"

#define FLASH_CAPACITY 0x02000000u

static int write_image(const char *path)
{
    FILE *stream;
    uint8_t image[4096];

    (void)memset(image, 0xff, sizeof(image));
    image[0x123u] = 0xa5u;
    image[0x124u] = 0x5au;
    image[0x125u] = 0x3cu;
    stream = fopen(path, "wb");
    if (stream == NULL) return 0;
    if (fwrite(image, 1u, sizeof(image), stream) != sizeof(image)) {
        (void)fclose(stream);
        return 0;
    }
    return fclose(stream) == 0;
}

static semu_storage *open_storage(semu_test_context *context, char *path,
                                  size_t path_size)
{
    semu_error error;
    semu_storage *storage;

    if (!semu_test_temp_path(path, path_size, "flash.bin")) {
        semu_test_fail(context, __FILE__, __LINE__, "temporary path");
        return NULL;
    }
    if (!write_image(path)) {
        semu_test_fail(context, __FILE__, __LINE__, "write image");
        return NULL;
    }
    semu_error_clear(&error);
    storage = semu_storage_open(path, FLASH_CAPACITY, 0xffu, &error);
    if (storage == NULL) {
        semu_test_fail(context, __FILE__, __LINE__, "open storage");
        (void)remove(path);
        return NULL;
    }
    return storage;
}

static void test_flash_id_and_read(semu_test_context *context)
{
    char path[128];
    semu_storage *storage = open_storage(context, path, sizeof(path));
    semu_sapporo_flash *flash;
    semu_serial_endpoint ep;
    semu_serial_transaction txn;
    semu_error error;
    uint8_t id[3] = { 0u, 0u, 0u };
    uint8_t readback[3] = { 0u, 0u, 0u };
    const uint8_t id_command[] = { 0x9fu };
    const uint8_t read_command[] = { 0x0cu, 0x00u, 0x01u, 0x23u };

    if (storage == NULL) return;
    semu_error_clear(&error);
    flash = semu_sapporo_flash_create(storage, FLASH_CAPACITY, 4096u, 256u,
                                      &error);
    SEMU_TEST_ASSERT(context, flash != NULL);
    ep = semu_sapporo_flash_endpoint(flash);

    memset(&txn, 0, sizeof(txn));
    txn.tx = id_command;
    txn.tx_size = sizeof(id_command);
    txn.rx = id;
    txn.rx_size = sizeof(id);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));
    SEMU_TEST_EQ_U64(context, 0x20u, id[0u]);
    SEMU_TEST_EQ_U64(context, 0xbbu, id[1u]);
    SEMU_TEST_EQ_U64(context, 0x19u, id[2u]);

    memset(&txn, 0, sizeof(txn));
    txn.tx = read_command;
    txn.tx_size = sizeof(read_command);
    txn.rx = readback;
    txn.rx_size = sizeof(readback);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));
    SEMU_TEST_EQ_U64(context, 0xa5u, readback[0u]);
    SEMU_TEST_EQ_U64(context, 0x5au, readback[1u]);
    SEMU_TEST_EQ_U64(context, 0x3cu, readback[2u]);
    SEMU_TEST_EQ_U64(context, 0u, semu_storage_dirty_pages(storage));

    semu_sapporo_flash_destroy(flash);
    semu_storage_destroy(storage);
    (void)remove(path);
}

static void test_flash_status_and_reset(semu_test_context *context)
{
    char path[128];
    semu_storage *storage = open_storage(context, path, sizeof(path));
    semu_sapporo_flash *flash;
    semu_serial_endpoint ep;
    semu_serial_transaction txn;
    semu_error error;
    uint8_t status = 0u;
    uint8_t observed = 0xffu;
    const uint8_t write_enable[] = { 0x06u };
    const uint8_t read_status[] = { 0x70u };
    const uint8_t observed_read[] = { 0x85u };

    if (storage == NULL) return;
    semu_error_clear(&error);
    flash = semu_sapporo_flash_create(storage, FLASH_CAPACITY, 4096u, 256u,
                                      &error);
    SEMU_TEST_ASSERT(context, flash != NULL);
    ep = semu_sapporo_flash_endpoint(flash);

    memset(&txn, 0, sizeof(txn));
    txn.tx = write_enable;
    txn.tx_size = sizeof(write_enable);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));
    semu_sapporo_flash_reset(flash);

    memset(&txn, 0, sizeof(txn));
    txn.tx = read_status;
    txn.tx_size = sizeof(read_status);
    txn.rx = &status;
    txn.rx_size = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));
    SEMU_TEST_EQ_U64(context, 0x80u, status);

    memset(&txn, 0, sizeof(txn));
    txn.tx = observed_read;
    txn.tx_size = sizeof(observed_read);
    txn.rx = &observed;
    txn.rx_size = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));
    SEMU_TEST_EQ_U64(context, 0u, observed);

    semu_sapporo_flash_destroy(flash);
    semu_storage_destroy(storage);
    (void)remove(path);
}

static void test_flash_program_and_erase(semu_test_context *context)
{
    char path[128];
    semu_storage *storage = open_storage(context, path, sizeof(path));
    semu_sapporo_flash *flash;
    semu_serial_endpoint ep;
    semu_serial_transaction txn;
    semu_error error;
    uint8_t readback[2] = { 0u, 0u };
    const uint8_t write_enable[] = { 0x06u };
    const uint8_t program[] = { 0x12u, 0x00u, 0x01u, 0x20u, 0x0fu, 0xf0u };
    const uint8_t invalid_program[] = {
        0x12u, 0x00u, 0x01u, 0x20u, 0xffu, 0xffu
    };
    const uint8_t crossing_program[] = {
        0x12u, 0x00u, 0x01u, 0xffu, 0x00u, 0x00u
    };
    const uint8_t erase[] = { 0x21u, 0x00u, 0x01u, 0x23u };
    const uint8_t read_command[] = { 0x0cu, 0x00u, 0x01u, 0x20u };

    if (storage == NULL) return;
    semu_error_clear(&error);
    flash = semu_sapporo_flash_create(storage, FLASH_CAPACITY, 4096u, 256u,
                                      &error);
    SEMU_TEST_ASSERT(context, flash != NULL);
    ep = semu_sapporo_flash_endpoint(flash);

    memset(&txn, 0, sizeof(txn));
    txn.tx = write_enable;
    txn.tx_size = sizeof(write_enable);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));
    memset(&txn, 0, sizeof(txn));
    txn.tx = program;
    txn.tx_size = sizeof(program);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));

    memset(&txn, 0, sizeof(txn));
    txn.tx = read_command;
    txn.tx_size = sizeof(read_command);
    txn.rx = readback;
    txn.rx_size = sizeof(readback);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));
    SEMU_TEST_EQ_U64(context, 0x0fu, readback[0u]);
    SEMU_TEST_EQ_U64(context, 0xf0u, readback[1u]);

    memset(&txn, 0, sizeof(txn));
    txn.tx = invalid_program;
    txn.tx_size = sizeof(invalid_program);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     ep.transfer(ep.context, &txn, &error));
    memset(&txn, 0, sizeof(txn));
    txn.tx = crossing_program;
    txn.tx_size = sizeof(crossing_program);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     ep.transfer(ep.context, &txn, &error));

    memset(&txn, 0, sizeof(txn));
    txn.tx = write_enable;
    txn.tx_size = sizeof(write_enable);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));
    memset(&txn, 0, sizeof(txn));
    txn.tx = erase;
    txn.tx_size = sizeof(erase);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));

    memset(readback, 0u, sizeof(readback));
    memset(&txn, 0, sizeof(txn));
    txn.tx = read_command;
    txn.tx_size = sizeof(read_command);
    txn.rx = readback;
    txn.rx_size = sizeof(readback);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));
    SEMU_TEST_EQ_U64(context, 0xffu, readback[0u]);
    SEMU_TEST_EQ_U64(context, 0xffu, readback[1u]);

    memset(&txn, 0, sizeof(txn));
    txn.tx = erase;
    txn.tx_size = sizeof(erase);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     ep.transfer(ep.context, &txn, &error));

    semu_sapporo_flash_destroy(flash);
    semu_storage_destroy(storage);
    (void)remove(path);
}

static void test_flash_refuses_invalid_transfers(semu_test_context *context)
{
    char path[128];
    semu_storage *storage = open_storage(context, path, sizeof(path));
    semu_sapporo_flash *flash;
    semu_serial_endpoint ep;
    semu_serial_transaction txn;
    semu_error error;
    uint8_t response[3] = { 0x11u, 0x22u, 0x33u };
    const uint8_t unknown[] = { 0x03u };
    const uint8_t invalid_read[] = { 0x0cu, 0x00u, 0x01u, 0x23u };

    if (storage == NULL) return;
    semu_error_clear(&error);
    flash = semu_sapporo_flash_create(storage, FLASH_CAPACITY, 4096u, 256u,
                                      &error);
    SEMU_TEST_ASSERT(context, flash != NULL);
    ep = semu_sapporo_flash_endpoint(flash);

    memset(&txn, 0, sizeof(txn));
    txn.tx = unknown;
    txn.tx_size = sizeof(unknown);
    txn.rx = response;
    txn.rx_size = sizeof(response);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     ep.transfer(ep.context, &txn, &error));
    SEMU_TEST_EQ_U64(context, 0x11u, response[0u]);
    SEMU_TEST_EQ_U64(context, 0x22u, response[1u]);
    SEMU_TEST_EQ_U64(context, 0x33u, response[2u]);

    memset(&txn, 0, sizeof(txn));
    txn.tx = invalid_read;
    txn.tx_size = sizeof(invalid_read);
    txn.rx = NULL;
    txn.rx_size = sizeof(response);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     ep.transfer(ep.context, &txn, &error));
    SEMU_TEST_EQ_U64(context, 0x11u, response[0u]);
    SEMU_TEST_EQ_U64(context, 0u, semu_storage_dirty_pages(storage));

    semu_sapporo_flash_destroy(flash);
    semu_storage_destroy(storage);
    (void)remove(path);
}

static void test_flash_upper_half_address(semu_test_context *context)
{
    char path[128];
    semu_storage *storage = open_storage(context, path, sizeof(path));
    semu_sapporo_flash *flash;
    semu_serial_endpoint ep;
    semu_serial_transaction txn;
    semu_error error;
    uint8_t readback[2] = { 0u, 0u };
    const uint8_t write_enable[] = { 0x06u };
    const uint8_t program[] = {
        0x12u, 0x00u, 0x00u, 0x20u, 0x0fu, 0xf0u
    };
    const uint8_t erase[] = { 0x21u, 0x00u, 0x00u, 0x23u };
    const uint8_t read_command[] = { 0x0cu, 0x00u, 0x00u, 0x20u };

    if (storage == NULL) return;
    semu_error_clear(&error);
    flash = semu_sapporo_flash_create(storage, FLASH_CAPACITY, 4096u, 256u,
                                      &error);
    SEMU_TEST_ASSERT(context, flash != NULL);
    ep = semu_sapporo_flash_endpoint(flash);

    memset(&txn, 0, sizeof(txn));
    txn.address = 1u;
    txn.tx = read_command;
    txn.tx_size = sizeof(read_command);
    txn.rx = readback;
    txn.rx_size = sizeof(readback);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));
    SEMU_TEST_EQ_U64(context, 0xffu, readback[0u]);
    SEMU_TEST_EQ_U64(context, 0xffu, readback[1u]);

    memset(&txn, 0, sizeof(txn));
    txn.address = 1u;
    txn.tx = write_enable;
    txn.tx_size = sizeof(write_enable);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));
    memset(&txn, 0, sizeof(txn));
    txn.address = 1u;
    txn.tx = program;
    txn.tx_size = sizeof(program);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));

    memset(readback, 0u, sizeof(readback));
    memset(&txn, 0, sizeof(txn));
    txn.address = 1u;
    txn.tx = read_command;
    txn.tx_size = sizeof(read_command);
    txn.rx = readback;
    txn.rx_size = sizeof(readback);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));
    SEMU_TEST_EQ_U64(context, 0x0fu, readback[0u]);
    SEMU_TEST_EQ_U64(context, 0xf0u, readback[1u]);

    memset(&txn, 0, sizeof(txn));
    txn.address = 1u;
    txn.tx = write_enable;
    txn.tx_size = sizeof(write_enable);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));
    memset(&txn, 0, sizeof(txn));
    txn.address = 1u;
    txn.tx = erase;
    txn.tx_size = sizeof(erase);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));

    memset(readback, 0u, sizeof(readback));
    memset(&txn, 0, sizeof(txn));
    txn.address = 1u;
    txn.tx = read_command;
    txn.tx_size = sizeof(read_command);
    txn.rx = readback;
    txn.rx_size = sizeof(readback);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     ep.transfer(ep.context, &txn, &error));
    SEMU_TEST_EQ_U64(context, 0xffu, readback[0u]);
    SEMU_TEST_EQ_U64(context, 0xffu, readback[1u]);

    semu_sapporo_flash_destroy(flash);
    semu_storage_destroy(storage);
    (void)remove(path);
}

static void test_flash_requires_geometry(semu_test_context *context)
{
    char path[128];
    semu_storage *storage = open_storage(context, path, sizeof(path));
    semu_error error;

    if (storage == NULL) return;
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
        semu_sapporo_flash_create(storage, 0x00200000u, 4096u, 256u,
                                  &error) == NULL);
    SEMU_TEST_ASSERT(context,
        semu_sapporo_flash_create(NULL, FLASH_CAPACITY, 4096u, 256u,
                                  &error) == NULL);
    semu_storage_destroy(storage);
    (void)remove(path);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_flash_id_and_read),
        SEMU_TEST_CASE(test_flash_status_and_reset),
        SEMU_TEST_CASE(test_flash_program_and_erase),
        SEMU_TEST_CASE(test_flash_refuses_invalid_transfers),
        SEMU_TEST_CASE(test_flash_upper_half_address),
        SEMU_TEST_CASE(test_flash_requires_geometry)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
