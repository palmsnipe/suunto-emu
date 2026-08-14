#include "semu/storage.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static int write_image(const char *path)
{
    uint8_t block[8192];
    FILE *stream;
    (void)memset(block, 0xff, sizeof(block));
    block[10] = 0xf0u;
    stream = fopen(path, "wb");
    if (stream == NULL) return 0;
    if (fwrite(block, 1u, sizeof(block), stream) != sizeof(block)) {
        (void)fclose(stream); return 0;
    }
    return fclose(stream) == 0;
}

static void test_overlay_program_and_erase(semu_test_context *context)
{
    char path[128];
    semu_error error;
    semu_storage *storage;
    uint8_t input[2] = {0x7fu, 0x3fu};
    uint8_t output[2];
    SEMU_TEST_ASSERT(context, semu_test_temp_path(path, sizeof(path), "store.bin"));
    SEMU_TEST_ASSERT(context, write_image(path));
    storage = semu_storage_open(path, 8192u, 0xffu, &error);
    SEMU_TEST_ASSERT(context, storage != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_storage_program(storage, 4095u, input, 2u, &error));
    SEMU_TEST_EQ_U64(context, 2u, semu_storage_dirty_pages(storage));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_storage_read(storage, 4095u, output, 2u, &error));
    SEMU_TEST_ASSERT(context, memcmp(input, output, 2u) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
                     semu_storage_program(storage, 4095u, "\xff", 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_storage_erase(storage, 4095u, 2u, &error));
    SEMU_TEST_EQ_U64(context, 0u, semu_storage_dirty_pages(storage));
    semu_storage_destroy(storage);
    (void)remove(path);
}

static void test_source_is_immutable(semu_test_context *context)
{
    char path[128];
    semu_error error;
    semu_storage *storage;
    uint8_t value;
    FILE *stream;
    SEMU_TEST_ASSERT(context, semu_test_temp_path(path, sizeof(path), "immutable.bin"));
    SEMU_TEST_ASSERT(context, write_image(path));
    storage = semu_storage_open(path, 12288u, 0xffu, &error);
    SEMU_TEST_ASSERT(context, storage != NULL);
    value = 0xe0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_storage_program(storage, 10u, &value, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_storage_read(storage, 10u, &value, 1u, &error));
    SEMU_TEST_EQ_U64(context, 0xe0u, value);
    semu_storage_destroy(storage);
    stream = fopen(path, "rb");
    SEMU_TEST_ASSERT(context, stream != NULL);
    SEMU_TEST_EQ_U64(context, 0u, fseek(stream, 10L, SEEK_SET));
    SEMU_TEST_EQ_U64(context, 1u, fread(&value, 1u, 1u, stream));
    SEMU_TEST_EQ_U64(context, 0xf0u, value);
    (void)fclose(stream);
    (void)remove(path);
}

static void test_bounds_and_oversized_image(semu_test_context *context)
{
    char path[128];
    semu_error error;
    semu_storage *storage;
    uint8_t value;
    SEMU_TEST_ASSERT(context, semu_test_temp_path(path, sizeof(path), "bounds.bin"));
    SEMU_TEST_ASSERT(context, write_image(path));
    storage = semu_storage_open(path, 4096u, 0xffu, &error);
    SEMU_TEST_ASSERT(context, storage == NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, error.code);
    storage = semu_storage_open(path, 8192u, 0xffu, &error);
    SEMU_TEST_ASSERT(context, storage != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
                     semu_storage_read(storage, 8192u, &value, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
                     semu_storage_erase(storage, 8191u, 2u, &error));
    semu_storage_destroy(storage);
    (void)remove(path);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_overlay_program_and_erase),
        SEMU_TEST_CASE(test_source_is_immutable),
        SEMU_TEST_CASE(test_bounds_and_oversized_image)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
