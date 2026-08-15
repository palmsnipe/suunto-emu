#include "test.h"

#include <stdio.h>
#include <string.h>

#include "sapporo_flash.h"
#include "semu/peripheral.h"
#include "semu/storage.h"

static int write_image(const char *path, uint32_t capacity)
{
    FILE *stream;
    static uint8_t block[4096];
    uint32_t remaining = capacity;
    (void)memset(block, 0xFF, sizeof(block));
    stream = fopen(path, "wb");
    if (stream == NULL) return 0;
    while (remaining > 0u) {
        size_t chunk = remaining < sizeof(block) ? remaining : sizeof(block);
        if (fwrite(block, 1u, chunk, stream) != chunk) {
            (void)fclose(stream);
            return 0;
        }
        remaining -= (uint32_t)chunk;
    }
    return fclose(stream) == 0;
}

static void test_flash_refuses_transfer(semu_test_context *context)
{
    char path[128];
    semu_storage *storage;
    semu_sapporo_flash *flash;
    semu_serial_endpoint ep;
    semu_serial_transaction txn;
    semu_error error;
    semu_transaction_result result;

    SEMU_TEST_ASSERT(context,
        semu_test_temp_path(path, sizeof(path), "flash.bin"));
    SEMU_TEST_ASSERT(context, write_image(path, 0x200000u));
    semu_error_clear(&error);
    storage = semu_storage_open(path, 0x200000u, 0xFFu, &error);
    SEMU_TEST_ASSERT(context, storage != NULL);
    flash = semu_sapporo_flash_create(storage, 0x200000u, 4096u, 256u,
                                      &error);
    SEMU_TEST_ASSERT(context, flash != NULL);
    ep = semu_sapporo_flash_endpoint(flash);
    memset(&txn, 0, sizeof(txn));
    result = ep.transfer(ep.context, &txn, &error);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, result);
    semu_sapporo_flash_destroy(flash);
    semu_storage_destroy(storage);
    (void)remove(path);
}

static void test_flash_requires_geometry(semu_test_context *context)
{
    char path[128];
    semu_storage *storage;
    semu_error error;
    SEMU_TEST_ASSERT(context,
        semu_test_temp_path(path, sizeof(path), "flash2.bin"));
    SEMU_TEST_ASSERT(context, write_image(path, 0x200000u));
    semu_error_clear(&error);
    storage = semu_storage_open(path, 0x200000u, 0xFFu, &error);
    SEMU_TEST_ASSERT(context, storage != NULL);
    SEMU_TEST_ASSERT(context,
        semu_sapporo_flash_create(storage, 0u, 4096u, 256u, &error) == NULL);
    SEMU_TEST_ASSERT(context,
        semu_sapporo_flash_create(NULL, 0x200000u, 4096u, 256u, &error)
        == NULL);
    semu_storage_destroy(storage);
    (void)remove(path);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_flash_refuses_transfer),
        SEMU_TEST_CASE(test_flash_requires_geometry)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
