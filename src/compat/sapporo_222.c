#include "sapporo_222.h"

#include <string.h>

#define FIRMWARE_BASE 0x00040000u
#define CRC_TABLE_ADDRESS 0x00199ee8u
#define PRODUCTION_ADDRESS 0x14fff000u
#define RECORD_SIZE 256u
#define SECTOR_SIZE 4096u
#define CHECKSUM_OFFSET 252u

const semu_layer_descriptor semu_sapporo_222_no_device_layer = {
    "sapporo-2.22-no-device",
    SEMU_LAYER_SYNTHETIC_STATE,
    "sapporo-2.22.60",
    "suunto-firmware/tools/build_production_data_fixture.py and "
    "docs/research/factory-calibration-records.md",
    1u
};

static uint32_t firmware_checksum(const uint8_t *data, size_t size,
                                  const uint32_t table[16])
{
    uint32_t value = 0u;
    size_t i;
    for (i = 0u; i < size; ++i) {
        uint32_t intermediate = table[(data[i] ^ value) & 0x0fu] ^ (value >> 4u);
        value = table[(intermediate ^ (data[i] >> 4u)) & 0x0fu] ^
                (intermediate >> 4u);
    }
    return value;
}

static void put_u16(uint8_t *data, size_t offset, uint16_t value)
{
    data[offset] = (uint8_t)value;
    data[offset + 1u] = (uint8_t)(value >> 8u);
}

static void put_u32(uint8_t *data, size_t offset, uint32_t value)
{
    data[offset] = (uint8_t)value;
    data[offset + 1u] = (uint8_t)(value >> 8u);
    data[offset + 2u] = (uint8_t)(value >> 16u);
    data[offset + 3u] = (uint8_t)(value >> 24u);
}

static void finish_record(uint8_t record[RECORD_SIZE],
                          const uint32_t table[16])
{
    put_u32(record, CHECKSUM_OFFSET,
            firmware_checksum(record, CHECKSUM_OFFSET, table));
}

static void build_production(uint8_t record[RECORD_SIZE],
                             const uint32_t table[16])
{
    memset(record, 0, RECORD_SIZE);
    memcpy(record, "ProductionData", 14u);
    put_u16(record, 14u, 1000u);
    memcpy(record + 20u, "EMU000", 6u);
    memcpy(record + 26u, "00000", 5u);
    record[31u] = (uint8_t)'E';
    memcpy(record + 32u, "EMU000001", 9u);
    memcpy(record + 41u, "SUUNTO-EMU-001", 14u);
    memcpy(record + 55u, "000000", 6u);
    record[61u] = 1u;
    finish_record(record, table);
}

static void build_calibration(uint8_t record[RECORD_SIZE], const char magic[4],
                              uint8_t page, uint16_t payload_size,
                              const uint8_t *payload,
                              const uint32_t table[16])
{
    memset(record, 0, RECORD_SIZE);
    memcpy(record, magic, 4u);
    put_u16(record, 4u, 1u);
    put_u16(record, 6u, payload_size);
    record[12u] = page;
    record[13u] = 1u;
    if (payload != NULL) {
        memcpy(record + 16u, payload, payload_size);
    }
    finish_record(record, table);
}

semu_status semu_sapporo_222_install_no_device(
    semu_bus *bus, semu_layer_state *state, semu_logger *logger,
    semu_error *error)
{
    uint8_t sector[SECTOR_SIZE];
    uint8_t table_bytes[64];
    uint8_t hlat[112];
    uint32_t table[16];
    size_t i;
    semu_status status;
    if (bus == NULL || state == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid Sapporo fixture target");
        return SEMU_ERR_ARGUMENT;
    }
    status = semu_bus_copy_out(bus, CRC_TABLE_ADDRESS, table_bytes,
                               sizeof(table_bytes), error);
    if (status != SEMU_OK) {
        return status;
    }
    for (i = 0u; i < 16u; ++i) {
        table[i] = (uint32_t)table_bytes[i * 4u] |
                   (uint32_t)table_bytes[i * 4u + 1u] << 8u |
                   (uint32_t)table_bytes[i * 4u + 2u] << 16u |
                   (uint32_t)table_bytes[i * 4u + 3u] << 24u;
    }
    memset(sector, 0xff, sizeof(sector));
    build_production(sector, table);
    build_calibration(sector + RECORD_SIZE, "ACCR", 1u, 36u, NULL, table);
    build_calibration(sector + RECORD_SIZE * 2u, "ACCC", 2u, 60u, NULL, table);
    build_calibration(sector + RECORD_SIZE * 3u, "MAGN", 3u, 108u, NULL, table);
    memset(hlat, 0, sizeof(hlat));
    memcpy(hlat, "EMUHLAT00001", 12u);
    build_calibration(sector + RECORD_SIZE * 6u, "HLAT", 6u, 112u,
                      hlat, table);
    status = semu_bus_load(bus, PRODUCTION_ADDRESS, sector, sizeof(sector), error);
    if (status != SEMU_OK) {
        return status;
    }
    return semu_layer_hit(state, logger,
                          "installed session-local synthetic ProductionData, "
                          "ACCR, ACCC, MAGN, and HLAT records",
                          error);
}
