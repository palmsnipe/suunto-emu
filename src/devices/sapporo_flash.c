#include "sapporo_flash.h"

#include "semu/hash.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define FLASH_CAPACITY UINT32_C(0x02000000)
#define FLASH_SECTOR_SIZE UINT32_C(0x1000)
#define FLASH_PAGE_SIZE UINT32_C(0x100)
#define FLASH_FACTORY_PAGE UINT32_C(0x00fff000)
#define FLASH_XIP_BASE UINT32_C(0x14000000)
#define FLASH_VSF_OFFSET UINT32_C(0x00fc0000)

#define COMMAND_READ_ID UINT8_C(0x9f)
#define COMMAND_MULTI_READ_ID UINT8_C(0xaf)
#define COMMAND_READ_FLAG_STATUS UINT8_C(0x70)
#define COMMAND_OBSERVED_ZERO_READ UINT8_C(0x85)
#define COMMAND_OCTAL_READ UINT8_C(0x0c)
#define COMMAND_WRITE_ENABLE UINT8_C(0x06)
#define COMMAND_SETUP UINT8_C(0x35)
#define COMMAND_PAGE_PROGRAM UINT8_C(0x12)
#define COMMAND_SECTOR_ERASE UINT8_C(0x21)

static const uint8_t flash_id[] = { 0x20u, 0xbbu, 0x19u };
static const uint8_t flash_vsf_magic[] = { '1', 'V', 'S', 'F' };

struct semu_sapporo_flash {
    const semu_storage *storage;
    semu_bus *overlay_bus;
    uint32_t capacity;
    uint32_t sector_size;
    uint32_t page_size;
    uint8_t write_enabled;
};

semu_status semu_sapporo_flash_validate_image(const char *path,
                                               semu_error *error)
{
    semu_storage *storage;
    uint8_t magic[sizeof(flash_vsf_magic)];
    uint8_t digest[SEMU_SHA256_SIZE];
    uint64_t actual_size;
    semu_status status;

    if (path == NULL || *path == '\0') {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "full external flash image path is required");
        return SEMU_ERR_ARGUMENT;
    }
    status = semu_sha256_file(path, digest, &actual_size, error);
    if (status != SEMU_OK) {
        return status;
    }
    if (actual_size != FLASH_CAPACITY) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "full external flash image must be exactly 32 MiB");
        return SEMU_ERR_CONFLICT;
    }
    storage = semu_storage_open(path, FLASH_CAPACITY, 0xffu, error);
    if (storage == NULL) {
        return error != NULL ? error->code : SEMU_ERR_IO;
    }
    status = semu_storage_read(storage, FLASH_VSF_OFFSET, magic,
                               sizeof(magic), error);
    semu_storage_destroy(storage);
    if (status != SEMU_OK) {
        return status;
    }
    if (memcmp(magic, flash_vsf_magic, sizeof(magic)) != 0) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "full external flash image has no 1VSF footer at 0xfc0000");
        return SEMU_ERR_CONFLICT;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_transaction_result refuse(semu_error *error, semu_status status,
                                      const char *message)
{
    semu_error_set(error, status, "%s", message);
    return SEMU_TRANSACTION_REFUSE;
}

static int frame_has(const semu_serial_transaction *transaction,
                     size_t tx_size, size_t rx_size)
{
    return transaction != NULL && transaction->tx != NULL &&
           transaction->tx_size == tx_size && transaction->rx != NULL &&
           transaction->rx_size == rx_size;
}

static uint32_t frame_address(const semu_serial_transaction *transaction)
{
    return ((uint32_t)transaction->address << 24u) |
           ((uint32_t)transaction->tx[1u] << 16u) |
           ((uint32_t)transaction->tx[2u] << 8u) |
           (uint32_t)transaction->tx[3u];
}

static semu_transaction_result copy_read(
    semu_sapporo_flash *flash, semu_serial_transaction *transaction,
    uint32_t address, semu_error *error)
{
    uint8_t *data;
    semu_status status;

    if (address > flash->capacity ||
        transaction->rx_size > (size_t)(flash->capacity - address)) {
        return refuse(error, SEMU_ERR_RANGE,
                      "external flash read exceeds device capacity");
    }
    data = (uint8_t *)malloc(transaction->rx_size != 0u ?
                             transaction->rx_size : 1u);
    if (data == NULL) {
        return refuse(error, SEMU_ERR_NOMEM,
                      "cannot allocate external flash response");
    }
    status = semu_storage_read((semu_storage *)flash->storage, address, data,
                               transaction->rx_size, error);
    if (status == SEMU_OK) {
        uint64_t read_end = (uint64_t)address + transaction->rx_size;
        uint64_t overlay_start = address > FLASH_FACTORY_PAGE ?
                                     address : FLASH_FACTORY_PAGE;
        uint64_t overlay_end = read_end < (uint64_t)FLASH_FACTORY_PAGE +
                                           FLASH_SECTOR_SIZE ?
                                   read_end :
                                   (uint64_t)FLASH_FACTORY_PAGE +
                                       FLASH_SECTOR_SIZE;
        if (flash->overlay_bus != NULL && overlay_start < overlay_end) {
            size_t offset = (size_t)(overlay_start - address);
            size_t overlay_size = (size_t)(overlay_end - overlay_start);
            uint64_t xip_address = (uint64_t)FLASH_XIP_BASE + overlay_start;
            if (xip_address > UINT32_MAX ||
                semu_bus_copy_out(flash->overlay_bus,
                                  (uint32_t)xip_address, data + offset,
                                  overlay_size, error) != SEMU_OK) {
                free(data);
                return SEMU_TRANSACTION_REFUSE;
            }
        }
        (void)memcpy(transaction->rx, data, transaction->rx_size);
    }
    free(data);
    if (status != SEMU_OK) {
        return SEMU_TRANSACTION_REFUSE;
    }
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

static semu_transaction_result program_page(
    semu_sapporo_flash *flash, semu_serial_transaction *transaction,
    semu_error *error)
{
    uint32_t address;
    size_t data_size;
    semu_status status;

    if (transaction->tx_size <= 4u || transaction->rx_size != 0u) {
        return refuse(error, SEMU_ERR_UNSUPPORTED,
                      "external flash page-program shape is unsupported");
    }
    data_size = transaction->tx_size - 4u;
    if (data_size > flash->page_size) {
        return refuse(error, SEMU_ERR_RANGE,
                      "external flash page-program payload is too large");
    }
    address = frame_address(transaction);
    if (address >= flash->capacity ||
        data_size > (size_t)(flash->capacity - address) ||
        address / flash->page_size !=
            (address + (uint32_t)data_size - 1u) / flash->page_size) {
        return refuse(error, SEMU_ERR_RANGE,
                      "external flash page program crosses a boundary");
    }
    if (flash->write_enabled == 0u) {
        return refuse(error, SEMU_ERR_STATE,
                      "external flash page program requires write-enable");
    }
    status = semu_storage_program((semu_storage *)flash->storage, address,
                                  transaction->tx + 4u, data_size, error);
    flash->write_enabled = 0u;
    if (status != SEMU_OK) return SEMU_TRANSACTION_REFUSE;
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

static semu_transaction_result erase_sector(
    semu_sapporo_flash *flash, semu_serial_transaction *transaction,
    semu_error *error)
{
    uint32_t address;
    semu_status status;

    if (transaction->tx_size != 4u || transaction->rx_size != 0u) {
        return refuse(error, SEMU_ERR_UNSUPPORTED,
                      "external flash sector-erase shape is unsupported");
    }
    address = frame_address(transaction);
    address &= ~(flash->sector_size - 1u);
    if (address > flash->capacity - flash->sector_size) {
        return refuse(error, SEMU_ERR_RANGE,
                      "external flash sector erase exceeds capacity");
    }
    if (flash->write_enabled == 0u) {
        return refuse(error, SEMU_ERR_STATE,
                      "external flash sector erase requires write-enable");
    }
    status = semu_storage_erase((semu_storage *)flash->storage, address,
                               flash->sector_size, error);
    flash->write_enabled = 0u;
    if (status != SEMU_OK) return SEMU_TRANSACTION_REFUSE;
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

static semu_transaction_result flash_transfer(
    void *context, semu_serial_transaction *transaction, semu_error *error)
{
    semu_sapporo_flash *flash = (semu_sapporo_flash *)context;
    uint8_t command;
    uint32_t address;

    if (flash == NULL || transaction == NULL || transaction->tx == NULL ||
        transaction->tx_size == 0u) {
        return refuse(error, SEMU_ERR_ARGUMENT,
                      "external flash transaction is incomplete");
    }
    command = transaction->tx[0u];
    switch (command) {
    case COMMAND_READ_ID:
    case COMMAND_MULTI_READ_ID:
        if (!frame_has(transaction, 1u, sizeof(flash_id))) {
            return refuse(error, SEMU_ERR_UNSUPPORTED,
                          "external flash ID transaction shape is unsupported");
        }
        (void)memcpy(transaction->rx, flash_id, sizeof(flash_id));
        semu_error_clear(error);
        return SEMU_TRANSACTION_OK;
    case COMMAND_READ_FLAG_STATUS:
        if (!frame_has(transaction, 1u, 1u)) {
            return refuse(error, SEMU_ERR_UNSUPPORTED,
                          "external flash status transaction shape is unsupported");
        }
        transaction->rx[0u] = 0x80u;
        semu_error_clear(error);
        return SEMU_TRANSACTION_OK;
    case COMMAND_OBSERVED_ZERO_READ:
        if (!frame_has(transaction, 1u, 1u)) {
            return refuse(error, SEMU_ERR_UNSUPPORTED,
                          "external flash observed read shape is unsupported");
        }
        transaction->rx[0u] = 0u;
        semu_error_clear(error);
        return SEMU_TRANSACTION_OK;
    case COMMAND_OCTAL_READ:
        if (transaction->tx_size != 4u || transaction->tx == NULL ||
            transaction->rx == NULL || transaction->rx_size == 0u) {
            return refuse(error, SEMU_ERR_UNSUPPORTED,
                          "external flash read transaction shape is unsupported");
        }
        address = frame_address(transaction);
        return copy_read(flash, transaction, address, error);
    case COMMAND_WRITE_ENABLE:
        if (transaction->tx_size != 1u || transaction->rx_size != 0u) {
            return refuse(error, SEMU_ERR_UNSUPPORTED,
                          "external flash write-enable transaction shape is unsupported");
        }
        flash->write_enabled = 1u;
        semu_error_clear(error);
        return SEMU_TRANSACTION_OK;
    case COMMAND_SETUP:
        if (transaction->tx_size != 1u || transaction->rx_size != 0u) {
            return refuse(error, SEMU_ERR_UNSUPPORTED,
                          "external flash setup transaction shape is unsupported");
        }
        semu_error_clear(error);
        return SEMU_TRANSACTION_OK;
    case COMMAND_PAGE_PROGRAM:
        if (transaction->tx_size < 4u) {
            return refuse(error, SEMU_ERR_UNSUPPORTED,
                          "external flash page-program frame is incomplete");
        }
        return program_page(flash, transaction, error);
    case COMMAND_SECTOR_ERASE:
        return erase_sector(flash, transaction, error);
    default:
        return refuse(error, SEMU_ERR_UNSUPPORTED,
                      "external flash opcode is unsupported");
    }
}

semu_sapporo_flash *semu_sapporo_flash_create(
    const semu_storage *storage, uint32_t capacity, uint32_t sector_size,
    uint32_t page_size, semu_error *error)
{
    semu_sapporo_flash *flash;
    if (storage == NULL || capacity == 0u || sector_size == 0u ||
        page_size == 0u || capacity != FLASH_CAPACITY ||
        sector_size != FLASH_SECTOR_SIZE || page_size != FLASH_PAGE_SIZE ||
        capacity % sector_size != 0u || sector_size % page_size != 0u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "external flash geometry is not evidenced");
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
    if (flash != NULL) {
        flash->write_enabled = 0u;
    }
}

void semu_sapporo_flash_bind_overlay_bus(semu_sapporo_flash *flash,
                                          semu_bus *bus)
{
    if (flash != NULL) {
        flash->overlay_bus = bus;
    }
}

semu_serial_endpoint semu_sapporo_flash_endpoint(
    semu_sapporo_flash *flash)
{
    return (semu_serial_endpoint){
        "sapporo.flash", flash_transfer, flash
    };
}
