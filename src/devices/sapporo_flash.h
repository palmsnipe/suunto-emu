#ifndef SEMU_SAPPORO_FLASH_H
#define SEMU_SAPPORO_FLASH_H

#include "semu/peripheral.h"
#include "semu/bus.h"
#include "semu/storage.h"
#include "semu/types.h"

/*
 * Sapporo 2.22 external flash device (ticket 402 continuation).
 * Evidence: E-SAP-FLASH-001, E-A4-MSPI-001, and the native MSPI2 boundary
 * trace.  The endpoint implements the observed startup read contract:
 * JEDEC IDs 0x9f/0xaf, the observed one-byte zero response for 0x85, flag
 * status 0x70, octal read 0x0c, setup/write-enable, page program, and sector
 * erase commands.
 *
 * The storage backend is borrowed and remains the owner of the immutable base
 * and sparse session overlay.
 */

typedef struct semu_sapporo_flash semu_sapporo_flash;

/* Validate the minimum observed shape of a private full-device image. */
semu_status semu_sapporo_flash_validate_image(const char *path,
                                               semu_error *error);

semu_sapporo_flash *semu_sapporo_flash_create(
    const semu_storage *storage, uint32_t capacity, uint32_t sector_size,
    uint32_t page_size, semu_error *error);
void semu_sapporo_flash_destroy(semu_sapporo_flash *flash);
void semu_sapporo_flash_reset(semu_sapporo_flash *flash);
void semu_sapporo_flash_bind_overlay_bus(semu_sapporo_flash *flash,
                                          semu_bus *bus);
semu_serial_endpoint semu_sapporo_flash_endpoint(
    semu_sapporo_flash *flash);

#endif
