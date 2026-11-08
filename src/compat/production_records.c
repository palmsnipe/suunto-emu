#include "production_records.h"

#include <string.h>

#define RECORD_SIZE 256u

static void put_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8u);
}

static void finish_record(uint8_t *record, const uint32_t table[16])
{
    uint32_t crc = 0u;
    size_t i;
    for (i = 0u; i < 252u; ++i) {
        crc = table[(record[i] ^ crc) & 15u] ^ (crc >> 4u);
        crc = table[((record[i] >> 4u) ^ crc) & 15u] ^ (crc >> 4u);
    }
    for (i = 0u; i < 4u; ++i) record[252u + i] = (uint8_t)(crc >> (i * 8u));
}

static void calibration(uint8_t *sector, unsigned page, const char *magic,
                        uint16_t payload_size, const uint32_t table[16])
{
    uint8_t *record = sector + page * RECORD_SIZE;
    memset(record, 0, RECORD_SIZE);
    memcpy(record, magic, 4u);
    put_u16(record + 4u, 1u);
    put_u16(record + 6u, payload_size);
    record[12u] = (uint8_t)page;
    record[13u] = 1u;
    if (page == 6u) memcpy(record + 16u, "EMUHLAT00001", 12u);
    finish_record(record, table);
}

semu_status semu_build_production_sector(semu_bus *bus, uint32_t table_address,
    uint8_t sector[SEMU_PRODUCTION_SECTOR_SIZE], semu_error *error)
{
    uint8_t table_bytes[64];
    uint32_t table[16];
    semu_status status;
    size_t i;

    status = semu_bus_copy_out(bus, table_address, table_bytes, sizeof(table_bytes), error);
    if (status != SEMU_OK) return status;
    for (i = 0u; i < 16u; ++i) {
        const uint8_t *p = table_bytes + i * 4u;
        table[i] = (uint32_t)p[0] | (uint32_t)p[1] << 8u |
                   (uint32_t)p[2] << 16u | (uint32_t)p[3] << 24u;
    }
    /* All identity and calibration values in these records are synthetic. */
    memset(sector, 0xff, SEMU_PRODUCTION_SECTOR_SIZE);
    memset(sector, 0, RECORD_SIZE);
    memcpy(sector, "ProductionData", 14u);
    put_u16(sector + 14u, 1000u);
    memcpy(sector + 20u, "EMU000", 6u);
    memcpy(sector + 26u, "00000", 5u);
    sector[31u] = (uint8_t)'E';
    memcpy(sector + 32u, "EMU000001", 9u);
    memcpy(sector + 41u, "SUUNTO-EMU-001", 14u);
    memcpy(sector + 55u, "000000", 6u);
    sector[61u] = 1u;
    finish_record(sector, table);
    calibration(sector, 1u, "ACCR", 36u, table);
    calibration(sector, 2u, "ACCC", 60u, table);
    calibration(sector, 3u, "MAGN", 108u, table);
    calibration(sector, 6u, "HLAT", 112u, table);
    return SEMU_OK;
}
