#include "semu/hash.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

typedef struct sha256_context {
    uint32_t state[8];
    uint64_t byte_count;
    uint8_t block[64];
    size_t block_size;
} sha256_context;

static uint32_t rotate_right(uint32_t value, unsigned amount)
{
    return (value >> amount) | (value << (32u - amount));
}

static uint32_t load_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24u) | ((uint32_t)data[1] << 16u) |
           ((uint32_t)data[2] << 8u) | data[3];
}

static void store_be32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24u);
    data[1] = (uint8_t)(value >> 16u);
    data[2] = (uint8_t)(value >> 8u);
    data[3] = (uint8_t)value;
}

static void sha256_transform(sha256_context *context, const uint8_t block[64])
{
    static const uint32_t constants[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
        0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
        0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
        0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };
    uint32_t words[64];
    uint32_t a, b, c, d, e, f, g, h;
    unsigned index;

    for (index = 0u; index < 16u; ++index) {
        words[index] = load_be32(block + index * 4u);
    }
    for (index = 16u; index < 64u; ++index) {
        uint32_t x = words[index - 15u];
        uint32_t y = words[index - 2u];
        uint32_t s0 = rotate_right(x, 7u) ^ rotate_right(x, 18u) ^ (x >> 3u);
        uint32_t s1 = rotate_right(y, 17u) ^ rotate_right(y, 19u) ^ (y >> 10u);
        words[index] = words[index - 16u] + s0 + words[index - 7u] + s1;
    }
    a = context->state[0]; b = context->state[1];
    c = context->state[2]; d = context->state[3];
    e = context->state[4]; f = context->state[5];
    g = context->state[6]; h = context->state[7];
    for (index = 0u; index < 64u; ++index) {
        uint32_t s1 = rotate_right(e, 6u) ^ rotate_right(e, 11u) ^
                      rotate_right(e, 25u);
        uint32_t choice = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + s1 + choice + constants[index] + words[index];
        uint32_t s0 = rotate_right(a, 2u) ^ rotate_right(a, 13u) ^
                      rotate_right(a, 22u);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + majority;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    context->state[0] += a; context->state[1] += b;
    context->state[2] += c; context->state[3] += d;
    context->state[4] += e; context->state[5] += f;
    context->state[6] += g; context->state[7] += h;
}

static void sha256_init(sha256_context *context)
{
    static const uint32_t initial[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };
    (void)memset(context, 0, sizeof(*context));
    (void)memcpy(context->state, initial, sizeof(initial));
}

static void sha256_update(sha256_context *context, const uint8_t *data,
                          size_t size)
{
    while (size != 0u) {
        size_t available = sizeof(context->block) - context->block_size;
        size_t amount = size < available ? size : available;
        (void)memcpy(context->block + context->block_size, data, amount);
        context->block_size += amount;
        context->byte_count += amount;
        data += amount;
        size -= amount;
        if (context->block_size == sizeof(context->block)) {
            sha256_transform(context, context->block);
            context->block_size = 0u;
        }
    }
}

static void sha256_final(sha256_context *context, uint8_t digest[32])
{
    uint64_t bit_count = context->byte_count * 8u;
    unsigned index;

    context->block[context->block_size++] = 0x80u;
    if (context->block_size > 56u) {
        (void)memset(context->block + context->block_size, 0,
                     sizeof(context->block) - context->block_size);
        sha256_transform(context, context->block);
        context->block_size = 0u;
    }
    (void)memset(context->block + context->block_size, 0,
                 56u - context->block_size);
    for (index = 0u; index < 8u; ++index) {
        context->block[63u - index] = (uint8_t)(bit_count >> (index * 8u));
    }
    sha256_transform(context, context->block);
    for (index = 0u; index < 8u; ++index) {
        store_be32(digest + index * 4u, context->state[index]);
    }
}

uint32_t semu_crc32(uint32_t seed, const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = seed ^ 0xffffffffu;
    size_t index;
    unsigned bit;

    if (bytes == NULL && size != 0u) {
        return seed;
    }
    for (index = 0u; index < size; ++index) {
        crc ^= bytes[index];
        for (bit = 0u; bit < 8u; ++bit) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1u) ^ (0xedb88320u & mask);
        }
    }
    return crc ^ 0xffffffffu;
}

void semu_sha256(const void *data, size_t size, uint8_t digest[32])
{
    sha256_context context;
    sha256_init(&context);
    if (data != NULL && size != 0u) {
        sha256_update(&context, (const uint8_t *)data, size);
    }
    sha256_final(&context, digest);
}

semu_status semu_sha256_file(const char *path, uint8_t digest[32],
                             uint64_t *size, semu_error *error)
{
    uint8_t buffer[8192];
    sha256_context context;
    uint64_t total = 0u;
    FILE *stream;
    size_t count;

    if (path == NULL || digest == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "path and digest required");
        return SEMU_ERR_ARGUMENT;
    }
    stream = fopen(path, "rb");
    if (stream == NULL) {
        semu_error_set(error, SEMU_ERR_IO, "cannot open %s: %s",
                       path, strerror(errno));
        return SEMU_ERR_IO;
    }
    sha256_init(&context);
    while ((count = fread(buffer, 1u, sizeof(buffer), stream)) != 0u) {
        if (UINT64_MAX - total < count) {
            (void)fclose(stream);
            semu_error_set(error, SEMU_ERR_RANGE, "file %s is too large", path);
            return SEMU_ERR_RANGE;
        }
        sha256_update(&context, buffer, count);
        total += count;
    }
    if (ferror(stream)) {
        (void)fclose(stream);
        semu_error_set(error, SEMU_ERR_IO, "cannot read %s", path);
        return SEMU_ERR_IO;
    }
    if (fclose(stream) != 0) {
        semu_error_set(error, SEMU_ERR_IO, "cannot close %s", path);
        return SEMU_ERR_IO;
    }
    sha256_final(&context, digest);
    if (size != NULL) {
        *size = total;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

static int hex_digit(char character)
{
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

int semu_sha256_parse(const char *text, uint8_t digest[32])
{
    size_t index;
    if (text == NULL || digest == NULL || strlen(text) != 64u) return 0;
    for (index = 0u; index < 32u; ++index) {
        int high = hex_digit(text[index * 2u]);
        int low = hex_digit(text[index * 2u + 1u]);
        if (high < 0 || low < 0) return 0;
        digest[index] = (uint8_t)((unsigned)high << 4u | (unsigned)low);
    }
    return 1;
}

void semu_sha256_format(const uint8_t digest[32], char output[65])
{
    static const char digits[] = "0123456789abcdef";
    size_t index;
    for (index = 0u; index < 32u; ++index) {
        output[index * 2u] = digits[digest[index] >> 4u];
        output[index * 2u + 1u] = digits[digest[index] & 15u];
    }
    output[64] = '\0';
}
