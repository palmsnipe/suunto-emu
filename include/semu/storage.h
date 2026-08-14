#ifndef SEMU_STORAGE_H
#define SEMU_STORAGE_H

#include "semu/types.h"

typedef struct semu_storage semu_storage;

semu_storage *semu_storage_open(const char *path, uint64_t logical_size,
                                uint8_t erased_value, semu_error *error);
void semu_storage_destroy(semu_storage *storage);
semu_status semu_storage_read(semu_storage *storage, uint64_t address,
                              void *data, size_t size, semu_error *error);
semu_status semu_storage_program(semu_storage *storage, uint64_t address,
                                 const void *data, size_t size,
                                 semu_error *error);
semu_status semu_storage_erase(semu_storage *storage, uint64_t address,
                               size_t size, semu_error *error);
size_t semu_storage_dirty_pages(const semu_storage *storage);

#endif
