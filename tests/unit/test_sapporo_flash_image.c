#include "sapporo_flash.h"
#include "test.h"

#include <stdio.h>

#define FLASH_SIZE 0x02000000L
#define VSF_OFFSET 0x00fc0000L

static int make_image(const char *path, const char *magic)
{
    FILE *stream = fopen(path, "wb");
    if (stream == NULL) return 0;
    if (fseek(stream, FLASH_SIZE - 1L, SEEK_SET) != 0 ||
        fputc(0xff, stream) == EOF || fseek(stream, VSF_OFFSET, SEEK_SET) != 0 ||
        fwrite(magic, 1u, 4u, stream) != 4u) {
        (void)fclose(stream);
        return 0;
    }
    return fclose(stream) == 0;
}

static void test_accepts_observed_full_image(semu_test_context *context)
{
    char path[128];
    semu_error error;

    SEMU_TEST_ASSERT(context,
        semu_test_temp_path(path, sizeof(path), "sapporo-full-flash.bin"));
    SEMU_TEST_ASSERT(context, make_image(path, "1VSF"));
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_flash_validate_image(path, &error));
    (void)remove(path);
}

static void test_refuses_bad_full_image(semu_test_context *context)
{
    char path[128];
    FILE *stream;
    semu_error error;

    SEMU_TEST_ASSERT(context,
        semu_test_temp_path(path, sizeof(path), "sapporo-bad-flash.bin"));
    stream = fopen(path, "wb");
    SEMU_TEST_ASSERT(context, stream != NULL);
    if (stream != NULL) {
        (void)fputs("not a flash image", stream);
        (void)fclose(stream);
    }
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
                     semu_sapporo_flash_validate_image(path, &error));
    (void)remove(path);
}

static void test_refuses_missing_footer(semu_test_context *context)
{
    char path[128];
    semu_error error;

    SEMU_TEST_ASSERT(context,
        semu_test_temp_path(path, sizeof(path), "sapporo-no-footer.bin"));
    SEMU_TEST_ASSERT(context, make_image(path, "NOPE"));
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
                     semu_sapporo_flash_validate_image(path, &error));
    (void)remove(path);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_accepts_observed_full_image),
        SEMU_TEST_CASE(test_refuses_bad_full_image),
        SEMU_TEST_CASE(test_refuses_missing_footer)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
