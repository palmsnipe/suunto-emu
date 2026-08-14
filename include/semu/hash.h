#ifndef SEMU_HASH_H
#define SEMU_HASH_H

#include "semu/types.h"

uint32_t semu_crc32(uint32_t seed, const void *data, size_t size);
void semu_sha256(const void *data, size_t size, uint8_t digest[SEMU_SHA256_SIZE]);
semu_status semu_sha256_file(const char *path,
                             uint8_t digest[SEMU_SHA256_SIZE],
                             uint64_t *size, semu_error *error);
int semu_sha256_parse(const char *text, uint8_t digest[SEMU_SHA256_SIZE]);
void semu_sha256_format(const uint8_t digest[SEMU_SHA256_SIZE], char output[65]);

#endif
