#ifndef SEMU_SAPPORO_FLASH_H
#define SEMU_SAPPORO_FLASH_H

#include "semu/peripheral.h"
#include "semu/storage.h"
#include "semu/types.h"

/*
 * Sapporo 2.22 external flash device (ticket 402).
 * Evidence: E-SAP-FLASH-001 is MISSING.  The external-flash candidate
 * is dimensional inference only; chip ID, opcodes, address widths,
 * status bits, and write-enable lifecycle are unverified.  All MSPI
 * commands refuse fail-closed until evidence is verified.
 *
 * The constructor accepts an immutable semu_storage backend so that
 * verified overlay operations can be wired once evidence is available.
 */

typedef struct semu_sapporo_flash semu_sapporo_flash;

semu_sapporo_flash *semu_sapporo_flash_create(
    const semu_storage *storage, uint32_t capacity, uint32_t sector_size,
    uint32_t page_size, semu_error *error);
void semu_sapporo_flash_destroy(semu_sapporo_flash *flash);
void semu_sapporo_flash_reset(semu_sapporo_flash *flash);
semu_serial_endpoint semu_sapporo_flash_endpoint(
    semu_sapporo_flash *flash);

#endif
