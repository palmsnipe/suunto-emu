#include "storage_internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STORAGE_PAGE_SIZE SEMU_STORAGE_PAGE_SIZE

static int range_valid(const semu_storage *storage, uint64_t address, size_t size)
{
    return storage != NULL && address <= storage->logical_size &&
           (uint64_t)size <= storage->logical_size - address;
}

static storage_page *find_page(semu_storage *storage, uint64_t index)
{
    storage_page *page = storage->pages;
    while (page != NULL && page->index < index) {
        page = page->next;
    }
    return page != NULL && page->index == index ? page : NULL;
}

static const uint8_t *page_source(const semu_storage *storage, uint64_t index)
{
    return storage->base + (size_t)(index * STORAGE_PAGE_SIZE);
}

static size_t page_size(const semu_storage *storage, uint64_t index)
{
    uint64_t start = index * STORAGE_PAGE_SIZE;
    uint64_t remaining = storage->logical_size - start;
    return remaining < STORAGE_PAGE_SIZE ? (size_t)remaining : STORAGE_PAGE_SIZE;
}

static storage_page *allocate_page(const semu_storage *storage, uint64_t index,
                                   semu_error *error)
{
    storage_page *page;
    size_t amount;

    page = (storage_page *)malloc(sizeof(*page));
    if (page == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate storage page");
        return NULL;
    }
    page->index = index;
    amount = page_size(storage, index);
    (void)memcpy(page->bytes, page_source(storage, index), amount);
    if (amount < STORAGE_PAGE_SIZE) {
        (void)memset(page->bytes + amount, storage->erased_value,
                     STORAGE_PAGE_SIZE - amount);
    }
    return page;
}

static storage_page *create_page(semu_storage *storage, uint64_t index,
                                 semu_error *error)
{
    storage_page **link = &storage->pages;
    storage_page *page;

    while (*link != NULL && (*link)->index < index) {
        link = &(*link)->next;
    }
    if (*link != NULL && (*link)->index == index) {
        return *link;
    }
    page = allocate_page(storage, index, error);
    if (page == NULL) return NULL;
    page->next = *link;
    *link = page;
    ++storage->page_count;
    return page;
}

static uint8_t byte_at(semu_storage *storage, uint64_t address)
{
    uint64_t index = address / STORAGE_PAGE_SIZE;
    storage_page *page = find_page(storage, index);
    if (page != NULL) {
        return page->bytes[(size_t)(address % STORAGE_PAGE_SIZE)];
    }
    return storage->base[(size_t)address];
}

static void prune_equal_pages(semu_storage *storage)
{
    storage_page **link = &storage->pages;
    while (*link != NULL) {
        storage_page *page = *link;
        size_t amount = page_size(storage, page->index);
        if (memcmp(page->bytes, page_source(storage, page->index), amount) == 0) {
            *link = page->next;
            free(page);
            --storage->page_count;
        } else {
            link = &page->next;
        }
    }
}

static int base_page_is_erased(const semu_storage *storage, uint64_t index)
{
    const uint8_t *source = page_source(storage, index);
    size_t amount = page_size(storage, index);
    size_t offset;

    for (offset = 0u; offset < amount; ++offset) {
        if (source[offset] != storage->erased_value) return 0;
    }
    return 1;
}

static semu_status erase_full_pages(semu_storage *storage, uint64_t address,
                                    size_t size, semu_error *error)
{
    storage_page **link = &storage->pages;
    uint64_t first = address / STORAGE_PAGE_SIZE;
    uint64_t count = (uint64_t)size / STORAGE_PAGE_SIZE;
    uint64_t index;

    while (*link != NULL && (*link)->index < first) {
        link = &(*link)->next;
    }
    for (index = first; index < first + count; ++index) {
        storage_page *page = *link;
        if (page != NULL && page->index == index) {
            (void)memset(page->bytes, storage->erased_value,
                         STORAGE_PAGE_SIZE);
            link = &page->next;
        } else if (!base_page_is_erased(storage, index)) {
            page = allocate_page(storage, index, error);
            if (page == NULL) return SEMU_ERR_NOMEM;
            (void)memset(page->bytes, storage->erased_value,
                         STORAGE_PAGE_SIZE);
            page->next = *link;
            *link = page;
            ++storage->page_count;
            link = &page->next;
        }
    }
    prune_equal_pages(storage);
    semu_error_clear(error);
    return SEMU_OK;
}

semu_storage *semu_storage_open(const char *path, uint64_t logical_size,
                                uint8_t erased_value, semu_error *error)
{
    semu_storage *storage;
    FILE *stream;
    size_t offset = 0u;

    if (path == NULL || logical_size == 0u || logical_size > SIZE_MAX) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid storage arguments");
        return NULL;
    }
    stream = fopen(path, "rb");
    if (stream == NULL) {
        semu_error_set(error, SEMU_ERR_IO, "cannot open %s: %s",
                       path, strerror(errno));
        return NULL;
    }
    storage = (semu_storage *)calloc(1u, sizeof(*storage));
    if (storage == NULL) {
        (void)fclose(stream);
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate storage");
        return NULL;
    }
    storage->base = (uint8_t *)malloc((size_t)logical_size);
    if (storage->base == NULL) {
        (void)fclose(stream);
        free(storage);
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate storage base");
        return NULL;
    }
    (void)memset(storage->base, erased_value, (size_t)logical_size);
    while (offset < (size_t)logical_size) {
        size_t count = fread(storage->base + offset, 1u,
                             (size_t)logical_size - offset, stream);
        offset += count;
        if (count == 0u) {
            break;
        }
    }
    if (ferror(stream)) {
        semu_error_set(error, SEMU_ERR_IO, "cannot read %s", path);
        semu_storage_destroy(storage);
        (void)fclose(stream);
        return NULL;
    }
    if (offset == (size_t)logical_size) {
        unsigned char extra;
        if (fread(&extra, 1u, 1u, stream) != 0u) {
            semu_error_set(error, SEMU_ERR_RANGE,
                           "storage image exceeds logical size");
            semu_storage_destroy(storage);
            (void)fclose(stream);
            return NULL;
        }
    }
    if (fclose(stream) != 0) {
        semu_error_set(error, SEMU_ERR_IO, "cannot close %s", path);
        semu_storage_destroy(storage);
        return NULL;
    }
    storage->logical_size = logical_size;
    storage->erased_value = erased_value;
    semu_error_clear(error);
    return storage;
}

void semu_storage_destroy(semu_storage *storage)
{
    storage_page *page;
    if (storage == NULL) {
        return;
    }
    page = storage->pages;
    while (page != NULL) {
        storage_page *next = page->next;
        free(page);
        page = next;
    }
    free(storage->base);
    free(storage);
}

semu_status semu_storage_read(semu_storage *storage, uint64_t address,
                              void *data, size_t size, semu_error *error)
{
    uint8_t *output = (uint8_t *)data;
    storage_page *page;
    uint64_t index;
    size_t offset = 0u;
    size_t in_page;
    if (!range_valid(storage, address, size) || (size != 0u && data == NULL)) {
        semu_error_set(error, SEMU_ERR_RANGE, "storage read out of range");
        return SEMU_ERR_RANGE;
    }
    index = address / STORAGE_PAGE_SIZE;
    in_page = (size_t)(address % STORAGE_PAGE_SIZE);
    page = storage->pages;
    while (page != NULL && page->index < index) {
        page = page->next;
    }
    while (offset < size) {
        size_t amount = STORAGE_PAGE_SIZE - in_page;
        const uint8_t *source;
        if (amount > size - offset) amount = size - offset;
        if (page != NULL && page->index == index) {
            source = page->bytes;
            (void)memcpy(output + offset, source + in_page, amount);
            page = page->next;
        } else {
            source = page_source(storage, index);
            (void)memcpy(output + offset, source + in_page, amount);
        }
        offset += amount;
        ++index;
        in_page = 0u;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_storage_program(semu_storage *storage, uint64_t address,
                                 const void *data, size_t size,
                                 semu_error *error)
{
    const uint8_t *input = (const uint8_t *)data;
    size_t offset;

    if (!range_valid(storage, address, size) || (size != 0u && data == NULL)) {
        semu_error_set(error, SEMU_ERR_RANGE, "storage program out of range");
        return SEMU_ERR_RANGE;
    }
    for (offset = 0u; offset < size; ++offset) {
        uint8_t old_value = byte_at(storage, address + offset);
        if ((old_value & input[offset]) != input[offset]) {
            semu_error_set(error, SEMU_ERR_STATE,
                           "program requires erase at 0x%llx",
                           (unsigned long long)(address + offset));
            return SEMU_ERR_STATE;
        }
    }
    for (offset = 0u; offset < size; ++offset) {
        uint64_t current = address + offset;
        uint64_t index = current / STORAGE_PAGE_SIZE;
        uint8_t old_value = byte_at(storage, current);
        storage_page *page;
        if (old_value == input[offset]) {
            continue;
        }
        page = create_page(storage, index, error);
        if (page == NULL) {
            return SEMU_ERR_NOMEM;
        }
        page->bytes[(size_t)(current % STORAGE_PAGE_SIZE)] = input[offset];
    }
    prune_equal_pages(storage);
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_storage_erase(semu_storage *storage, uint64_t address,
                               size_t size, semu_error *error)
{
    size_t offset;
    if (!range_valid(storage, address, size)) {
        semu_error_set(error, SEMU_ERR_RANGE, "storage erase out of range");
        return SEMU_ERR_RANGE;
    }
    if ((address % STORAGE_PAGE_SIZE) == 0u &&
        (size % STORAGE_PAGE_SIZE) == 0u) {
        return erase_full_pages(storage, address, size, error);
    }
    for (offset = 0u; offset < size; ++offset) {
        uint64_t current = address + offset;
        storage_page *page;
        if (byte_at(storage, current) == storage->erased_value) {
            continue;
        }
        page = create_page(storage, current / STORAGE_PAGE_SIZE, error);
        if (page == NULL) {
            return SEMU_ERR_NOMEM;
        }
        page->bytes[(size_t)(current % STORAGE_PAGE_SIZE)] = storage->erased_value;
    }
    prune_equal_pages(storage);
    semu_error_clear(error);
    return SEMU_OK;
}

size_t semu_storage_dirty_pages(const semu_storage *storage)
{
    return storage != NULL ? storage->page_count : 0u;
}
