#include "semu/hash.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static void test_crc32_vector(semu_test_context *context)
{
    static const char input[] = "123456789";
    uint32_t split;
    SEMU_TEST_EQ_U64(context, 0xcbf43926u,
                     semu_crc32(0u, input, sizeof(input) - 1u));
    split = semu_crc32(0u, input, 4u);
    SEMU_TEST_EQ_U64(context, 0xcbf43926u,
                     semu_crc32(split, input + 4u, sizeof(input) - 5u));
}

static void test_sha256_vectors(semu_test_context *context)
{
    uint8_t digest[SEMU_SHA256_SIZE];
    uint8_t parsed[SEMU_SHA256_SIZE];
    char text[65];
    static const char expected[] =
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    semu_sha256("abc", 3u, digest);
    semu_sha256_format(digest, text);
    SEMU_TEST_ASSERT(context, strcmp(text, expected) == 0);
    SEMU_TEST_ASSERT(context, semu_sha256_parse(text, parsed));
    SEMU_TEST_ASSERT(context, memcmp(digest, parsed, sizeof(digest)) == 0);
    SEMU_TEST_ASSERT(context, !semu_sha256_parse("abc", parsed));
}

static void test_sha256_file(semu_test_context *context)
{
    char path[128];
    uint8_t digest[SEMU_SHA256_SIZE];
    char text[65];
    uint64_t size = 0u;
    semu_error error;
    FILE *stream;
    SEMU_TEST_ASSERT(context, semu_test_temp_path(path, sizeof(path), "hash.bin"));
    stream = fopen(path, "wb");
    SEMU_TEST_ASSERT(context, stream != NULL);
    SEMU_TEST_EQ_U64(context, 3u, fwrite("abc", 1u, 3u, stream));
    SEMU_TEST_EQ_U64(context, 0u, fclose(stream));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sha256_file(path, digest, &size, &error));
    semu_sha256_format(digest, text);
    SEMU_TEST_EQ_U64(context, 3u, size);
    SEMU_TEST_ASSERT(context, strcmp(text,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
    (void)remove(path);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_crc32_vector),
        SEMU_TEST_CASE(test_sha256_vectors),
        SEMU_TEST_CASE(test_sha256_file)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
