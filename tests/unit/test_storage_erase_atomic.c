#include "test.h"
#include <stdlib.h>
#include <string.h>

/* Exercise the production implementation with deterministic allocation failure. */
static unsigned allocation, fail_at;
static void *fault_malloc(size_t size)
{ return ++allocation == fail_at ? NULL : malloc(size); }
#define malloc fault_malloc
#define semu_storage_open fault_storage_open
#define semu_storage_destroy fault_storage_destroy
#define semu_storage_read fault_storage_read
#define semu_storage_program fault_storage_program
#define semu_storage_erase fault_storage_erase
#define semu_storage_dirty_pages fault_storage_dirty_pages
#include "../../src/core/storage.c"
#undef malloc

static void test_storage_erase_atomic_allocation(semu_test_context *context)
{
    unsigned failure;
    for (failure = 1u; failure <= 15u; ++failure) {
        semu_storage *storage = calloc(1u, sizeof(*storage));
        uint8_t before[65536], after[65536], zero = 0u;
        storage_page *original;
        semu_error error;
        SEMU_TEST_ASSERT(context, storage != NULL);
        storage->logical_size = sizeof(before); storage->erased_value = 0xffu;
        storage->base = malloc(sizeof(before));
        SEMU_TEST_ASSERT(context, storage->base != NULL);
        memset(storage->base, 0xa5, sizeof(before));
        fail_at = 0u;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            fault_storage_program(storage, 0u, &zero, 1u, &error));
        original = storage->pages;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            fault_storage_read(storage, 0u, before, sizeof(before), &error));
        allocation = 0u; fail_at = failure;
        SEMU_TEST_EQ_U64(context, SEMU_ERR_NOMEM,
            fault_storage_erase(storage, 0u, sizeof(before), &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            fault_storage_read(storage, 0u, after, sizeof(after), &error));
        SEMU_TEST_ASSERT(context, memcmp(before, after, sizeof(before)) == 0);
        SEMU_TEST_EQ_U64(context, 1u, storage->page_count);
        SEMU_TEST_ASSERT(context, storage->pages == original && original->next == NULL);
        fail_at = 0u;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            fault_storage_erase(storage, 0u, sizeof(before), &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            fault_storage_read(storage, 0u, after, sizeof(after), &error));
        memset(before, 0xff, sizeof(before));
        SEMU_TEST_ASSERT(context, memcmp(before, after, sizeof(before)) == 0);
        fault_storage_destroy(storage);
    }
}

int main(void)
{
    const semu_test_case cases[] = { SEMU_TEST_CASE(test_storage_erase_atomic_allocation) };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
