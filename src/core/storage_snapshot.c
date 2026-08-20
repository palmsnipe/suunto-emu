#include "storage_internal.h"

#include <stdlib.h>

static void free_pages(storage_page *page)
{
    while (page != NULL) {
        storage_page *next = page->next;
        free(page);
        page = next;
    }
}

semu_status semu_storage_snapshot_write(const semu_storage *storage,
                                        semu_snapshot_writer *writer,
                                        semu_error *error)
{
    const storage_page *page;
    if (storage == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "storage snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (semu_snapshot_writer_u64(writer, storage->logical_size, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, storage->erased_value, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, (uint64_t)storage->page_count,
                                  error) != SEMU_OK)
        return error->code;
    for (page = storage->pages; page != NULL; page = page->next) {
        if (semu_snapshot_writer_u64(writer, page->index, error) != SEMU_OK ||
            semu_snapshot_writer_bytes(writer, page->bytes,
                                       SEMU_STORAGE_PAGE_SIZE, error) != SEMU_OK)
            return error->code;
    }
    return SEMU_OK;
}

semu_status semu_storage_snapshot_read(semu_storage *storage,
                                       semu_snapshot_reader *reader,
                                       semu_error *error)
{
    storage_page *head = NULL;
    storage_page **tail = &head;
    uint64_t logical_size;
    uint64_t page_count;
    uint8_t erased_value;
    uint64_t index;
    uint64_t previous = 0u;
    uint64_t page_limit;

    if (storage == NULL || reader == NULL ||
        semu_snapshot_reader_u64(reader, &logical_size, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &erased_value, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &page_count, error) != SEMU_OK) {
        if (error != NULL && error->code == SEMU_OK)
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                           "storage snapshot arguments are invalid");
        return error != NULL ? error->code : SEMU_ERR_ARGUMENT;
    }
    page_limit = storage->logical_size / SEMU_STORAGE_PAGE_SIZE;
    if (storage->logical_size % SEMU_STORAGE_PAGE_SIZE != 0u) {
        ++page_limit;
    }
    if (logical_size != storage->logical_size ||
        erased_value != storage->erased_value ||
        page_count > SIZE_MAX / sizeof(storage_page) ||
        page_count > (SEMU_SNAPSHOT_MAX_SECTION_SIZE /
                      (SEMU_STORAGE_PAGE_SIZE + sizeof(uint64_t)))) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "storage snapshot geometry does not match machine");
        return SEMU_ERR_CONFLICT;
    }
    for (index = 0u; index < page_count; ++index) {
        storage_page *page = (storage_page *)calloc(1u, sizeof(*page));
        if (page == NULL) {
            free_pages(head);
            semu_error_set(error, SEMU_ERR_NOMEM,
                           "cannot allocate storage snapshot page");
            return SEMU_ERR_NOMEM;
        }
        if (semu_snapshot_reader_u64(reader, &page->index, error) != SEMU_OK ||
            semu_snapshot_reader_bytes(reader, page->bytes,
                                       SEMU_STORAGE_PAGE_SIZE, error) != SEMU_OK) {
            free(page);
            free_pages(head);
            return error->code;
        }
        if (logical_size % SEMU_STORAGE_PAGE_SIZE != 0u &&
            page->index == logical_size / SEMU_STORAGE_PAGE_SIZE) {
            size_t valid_bytes = (size_t)(logical_size %
                                          SEMU_STORAGE_PAGE_SIZE);
            size_t offset;
            for (offset = valid_bytes; offset < SEMU_STORAGE_PAGE_SIZE;
                 ++offset) {
                if (page->bytes[offset] != erased_value) {
                    free(page);
                    free_pages(head);
                    semu_error_set(error, SEMU_ERR_FORMAT,
                                   "storage snapshot final page tail is not erased");
                    return SEMU_ERR_FORMAT;
                }
            }
        }
        if (page->index >= page_limit || (index != 0u && page->index <= previous)) {
            free(page);
            free_pages(head);
            semu_error_set(error, SEMU_ERR_FORMAT,
                           "storage snapshot pages are invalid");
            return SEMU_ERR_FORMAT;
        }
        previous = page->index;
        *tail = page;
        tail = &page->next;
    }
    free_pages(storage->pages);
    storage->pages = head;
    storage->page_count = (size_t)page_count;
    return SEMU_OK;
}
