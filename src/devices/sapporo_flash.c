/*
 * Sapporo 2.22 external flash device (ticket 402).
 * Evidence: E-SAP-FLASH-001 is MISSING.  Chip ID, opcodes, address
 * widths, status bits, and write-enable lifecycle are unverified.
 * All MSPI commands refuse fail-closed.  The immutable storage backend
 * is held for future wiring but not mutated.
 */

#include "sapporo_flash.h"

#include <stdlib.h>

struct semu_sapporo_flash {
    const semu_storage *storage;
    uint32_t capacity;
    uint32_t sector_size;
    uint32_t page_size;
};

static semu_transaction_result flash_transfer(
    void *context, semu_serial_transaction *transaction, semu_error *error)
{
    (void)context;
    (void)transaction;
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "external flash: E-SAP-FLASH-001 evidence is missing; "
                   "chip ID, opcodes, and status bits are unverified");
    return SEMU_TRANSACTION_REFUSE;
}

static const semu_serial_endpoint flash_endpoint = {
    .name = "sapporo.flash",
    .transfer = flash_transfer,
    .context = NULL
};

semu_sapporo_flash *semu_sapporo_flash_create(
    const semu_storage *storage, uint32_t capacity, uint32_t sector_size,
    uint32_t page_size, semu_error *error)
{
    semu_sapporo_flash *flash;
    if (storage == NULL || capacity == 0u || sector_size == 0u ||
        page_size == 0u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "flash requires storage and geometry");
        return NULL;
    }
    flash = (semu_sapporo_flash *)calloc(1u, sizeof(*flash));
    if (flash == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate flash");
        return NULL;
    }
    flash->storage = storage;
    flash->capacity = capacity;
    flash->sector_size = sector_size;
    flash->page_size = page_size;
    return flash;
}

void semu_sapporo_flash_destroy(semu_sapporo_flash *flash)
{
    free(flash);
}

void semu_sapporo_flash_reset(semu_sapporo_flash *flash)
{
    (void)flash;
}

semu_serial_endpoint semu_sapporo_flash_endpoint(
    semu_sapporo_flash *flash)
{
    (void)flash;
    return flash_endpoint;
}
