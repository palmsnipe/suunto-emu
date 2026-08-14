#include "sapporo_devices.h"

#include "semu/hash.h"

#include <stdlib.h>
#include <string.h>

struct semu_sapporo_device {
    semu_sapporo_device_kind kind;
    const char *name;
    uint8_t address;
    uint8_t registers[256];
    uint8_t readable[256];
    uint8_t writable[256];
    uint16_t ohr_sequence;
    int ohr_main;
};

static void allow_range(uint8_t mask[256], unsigned first, unsigned count)
{
    unsigned i;
    for (i = 0u; i < count && first + i < 256u; ++i) {
        mask[first + i] = 1u;
    }
}

static void initialize_registers(semu_sapporo_device *device)
{
    memset(device->registers, 0, sizeof(device->registers));
    memset(device->readable, 0, sizeof(device->readable));
    memset(device->writable, 0, sizeof(device->writable));
    device->ohr_sequence = 0u;
    device->ohr_main = 0;
    switch (device->kind) {
    case SEMU_SAPPORO_PRESSURE:
        device->registers[0x00] = 0x49u;
        device->registers[0x03] = 0x11u;
        device->registers[0x1c] = 0xe0u;
        allow_range(device->readable, 0u, 256u);
        allow_range(device->writable, 0u, 256u);
        break;
    case SEMU_SAPPORO_ACCELEROMETER:
        device->registers[0x0f] = 0x6au;
        device->registers[0x3a] = 0x00u;
        device->registers[0x3b] = 0x10u;
        allow_range(device->readable, 0u, 256u);
        allow_range(device->writable, 0u, 256u);
        break;
    case SEMU_SAPPORO_WRIST_MAGNETOMETER:
        allow_range(device->readable, 0u, 23u);
        allow_range(device->writable, 0x10u, 2u);
        device->registers[6u] = 0x44u;
        break;
    case SEMU_SAPPORO_HAPTIC:
        allow_range(device->readable, 0u, 256u);
        allow_range(device->writable, 0u, 256u);
        device->registers[0x22u] = 0x02u;
        break;
    case SEMU_SAPPORO_AMBIENT_LIGHT:
        allow_range(device->readable, 0u, 4u);
        allow_range(device->writable, 2u, 2u);
        break;
    case SEMU_SAPPORO_OHR2:
        break;
    }
}

static semu_transaction_result refuse(semu_error *error, const char *name,
                                      const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "%s refuses %s", name, reason);
    return SEMU_TRANSACTION_REFUSE;
}

static semu_transaction_result register_transfer(
    semu_sapporo_device *device, semu_serial_transaction *transaction,
    semu_error *error)
{
    uint8_t reg;
    size_t i;
    int spi = device->kind == SEMU_SAPPORO_ACCELEROMETER;
    if (transaction->address != device->address || transaction->tx_size == 0u) {
        return refuse(error, device->name, "address or empty transaction");
    }
    reg = transaction->tx[0];
    if (spi) {
        reg &= 0x7fu;
    }
    if (transaction->rx_size != 0u) {
        if ((!spi && transaction->tx_size != 1u) ||
            (spi && (transaction->tx[0] & 0x80u) == 0u)) {
            return refuse(error, device->name, "read framing");
        }
        for (i = 0u; i < transaction->rx_size; ++i) {
            unsigned index = (unsigned)reg + (unsigned)i;
            if (index >= 256u || device->readable[index] == 0u) {
                return refuse(error, device->name, "unknown read register");
            }
            transaction->rx[i] = device->registers[index];
        }
        return SEMU_TRANSACTION_OK;
    }
    if (spi && (transaction->tx[0] & 0x80u) != 0u) {
        return refuse(error, device->name, "read without receive buffer");
    }
    for (i = 1u; i < transaction->tx_size; ++i) {
        unsigned index = (unsigned)reg + (unsigned)i - 1u;
        if (index >= 256u || device->writable[index] == 0u) {
            return refuse(error, device->name, "unknown write register");
        }
        device->registers[index] = transaction->tx[i];
    }
    if (device->kind == SEMU_SAPPORO_HAPTIC && reg == 0x22u &&
        transaction->tx_size == 2u && transaction->tx[1] == 1u) {
        device->registers[0x22u] = 0x02u;
    }
    return SEMU_TRANSACTION_OK;
}

static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | (uint16_t)data[1] << 8u);
}

static void write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
}

static semu_transaction_result ohr_transfer(
    semu_sapporo_device *device, semu_serial_transaction *transaction,
    semu_error *error)
{
    uint16_t command;
    uint16_t sequence;
    uint32_t crc;
    if (transaction->address != device->address || transaction->tx_size != 59u ||
        (transaction->rx_size != 0u && transaction->rx_size != 58u)) {
        return refuse(error, device->name, "packet shape");
    }
    crc = semu_crc32(0u, transaction->tx, 55u);
    if (transaction->tx[55] != (uint8_t)crc ||
        transaction->tx[56] != (uint8_t)(crc >> 8u) ||
        transaction->tx[57] != (uint8_t)(crc >> 16u) ||
        transaction->tx[58] != (uint8_t)(crc >> 24u)) {
        return refuse(error, device->name, "request CRC");
    }
    command = read_u16(transaction->tx + 1u);
    sequence = read_u16(transaction->tx + 3u);
    if (command == 3u) {
        device->ohr_main = 1;
        return transaction->rx_size == 0u
                   ? SEMU_TRANSACTION_OK
                   : refuse(error, device->name, "fire-and-forget response");
    }
    if (command != 0u && command != 1u && command != 6u &&
        command != 13u && command != 14u) {
        return refuse(error, device->name, "unknown command");
    }
    if (transaction->rx_size != 58u) {
        return refuse(error, device->name, "missing response buffer");
    }
    memset(transaction->rx, 0, transaction->rx_size);
    write_u16(transaction->rx, command);
    write_u16(transaction->rx + 2u, sequence);
    if (command == 0u) {
        memcpy(transaction->rx + 9u, device->ohr_main ? "MAIN" : "BSL", 3u);
    } else if (command == 6u) {
        memcpy(transaction->rx + 4u, transaction->tx + 5u, 50u);
    }
    crc = semu_crc32(0u, transaction->rx, 54u);
    transaction->rx[54] = (uint8_t)crc;
    transaction->rx[55] = (uint8_t)(crc >> 8u);
    transaction->rx[56] = (uint8_t)(crc >> 16u);
    transaction->rx[57] = (uint8_t)(crc >> 24u);
    device->ohr_sequence = sequence;
    return SEMU_TRANSACTION_OK;
}

static semu_transaction_result transfer(void *context,
                                        semu_serial_transaction *transaction,
                                        semu_error *error)
{
    semu_sapporo_device *device = (semu_sapporo_device *)context;
    if (device == NULL || transaction == NULL) {
        return refuse(error, "Sapporo device", "null transaction");
    }
    return device->kind == SEMU_SAPPORO_OHR2
               ? ohr_transfer(device, transaction, error)
               : register_transfer(device, transaction, error);
}

semu_sapporo_device *semu_sapporo_device_create(
    semu_sapporo_device_kind kind, semu_error *error)
{
    static const char *const names[] = {
        "HSPPAD143", "LSM6DSL", "TLI493D-W2BW", "haptic PMIC",
        "OPT3007", "OHR2"
    };
    static const uint8_t addresses[] = { 0x48u, 0u, 0x35u, 0x50u, 0x45u, 0x10u };
    semu_sapporo_device *device;
    if ((unsigned)kind >= SEMU_ARRAY_LEN(names)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid Sapporo device kind");
        return NULL;
    }
    device = (semu_sapporo_device *)calloc(1u, sizeof(*device));
    if (device == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate Sapporo device");
        return NULL;
    }
    device->kind = kind;
    device->name = names[kind];
    device->address = addresses[kind];
    initialize_registers(device);
    return device;
}

void semu_sapporo_device_destroy(semu_sapporo_device *device)
{
    free(device);
}

semu_serial_endpoint semu_sapporo_device_endpoint(semu_sapporo_device *device)
{
    semu_serial_endpoint endpoint;
    endpoint.name = device != NULL ? device->name : "invalid";
    endpoint.transfer = transfer;
    endpoint.context = device;
    return endpoint;
}

void semu_sapporo_device_reset(semu_sapporo_device *device)
{
    if (device != NULL) {
        initialize_registers(device);
    }
}
