#ifndef SEMU_STORAGE_INTERNAL_H
#define SEMU_STORAGE_INTERNAL_H

#include "semu/storage.h"
#include "snapshot_io.h"

#define SEMU_STORAGE_PAGE_SIZE 4096u

typedef struct storage_page {
    uint64_t index;
    uint8_t bytes[SEMU_STORAGE_PAGE_SIZE];
    struct storage_page *next;
} storage_page;

struct semu_storage {
    uint8_t *base;
    uint64_t logical_size;
    uint8_t erased_value;
    storage_page *pages;
    size_t page_count;
};

semu_status semu_storage_snapshot_write(const semu_storage *storage,
                                        semu_snapshot_writer *writer,
                                        semu_error *error);
semu_status semu_storage_snapshot_read(semu_storage *storage,
                                       semu_snapshot_reader *reader,
                                       semu_error *error);

#endif
