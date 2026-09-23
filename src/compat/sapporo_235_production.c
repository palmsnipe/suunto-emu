#include "sapporo_235_production.h"

#include <string.h>

#define PRODUCTION_ADDRESS UINT32_C(0x14fff000)
#define TABLE_ADDRESS UINT32_C(0x001b1644)
#define RECORD_SIZE 256u
#define SECTOR_SIZE 4096u

static const char *const hashes[] = {
    "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5",
    "36a14dc5bad7b9cb8a7c8164bfaaedaf68c75a9611bc3a9e6efaa47418a5a38a",
    "f281385acc8bab169976f9e506c230fd25124c44bfdbe2048393763a7d85ae22"
};

/* A single trigger uses state->hits; there are no shared mutable counters. */
const semu_layer_descriptor semu_sapporo_235_production_layer = {
    .id = "sapporo-2.35-production-data",
    .kind = SEMU_LAYER_SYNTHETIC_STATE,
    .profile_id = "sapporo-2.35.34",
    .evidence = "E-SAP-0038",
    .component_hashes = hashes,
    .component_hash_count = 3u,
    .maximum_hits = 1u
};

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

semu_status semu_sapporo_235_install_production(semu_bus *bus,
    semu_layer_state *state, semu_logger *logger, semu_error *error)
{
    uint8_t sector[SECTOR_SIZE], previous[SECTOR_SIZE], table_bytes[64];
    uint32_t table[16];
    semu_status status;
    size_t i;
    int empty;
    if (bus == NULL || state == NULL || logger == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "incomplete 2.35 production target");
        return SEMU_ERR_ARGUMENT;
    }
    if (!state->enabled || state->descriptor != &semu_sapporo_235_production_layer ||
        state->hits != 0u) {
        semu_error_set(error, SEMU_ERR_STATE, "2.35 production layer disabled or exhausted");
        return SEMU_ERR_STATE;
    }
    status = semu_bus_copy_out(bus, PRODUCTION_ADDRESS, previous, sizeof(previous), error);
    if (status != SEMU_OK) return status;
    empty = previous[0] == 0u || previous[0] == 0xffu;
    for (i = 1u; i < sizeof(previous); ++i)
        if (previous[i] != previous[0]) empty = 0;
    status = semu_bus_copy_out(bus, TABLE_ADDRESS, table_bytes, sizeof(table_bytes), error);
    if (status != SEMU_OK) return status;
    for (i = 0u; i < 16u; ++i) {
        const uint8_t *p = table_bytes + i * 4u;
        table[i] = (uint32_t)p[0] | (uint32_t)p[1] << 8u |
                   (uint32_t)p[2] << 16u | (uint32_t)p[3] << 24u;
    }
    /* E-SAP-0038: recovered container; all identity/calibration is synthetic. */
    memset(sector, 0xff, sizeof(sector));
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
    /* A firmware reset preserves XIP memory. Only our exact prior fixture
     * may survive that reset; unrelated populated manufacturing data refuses. */
    if (!empty && memcmp(previous, sector, sizeof(sector)) != 0) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "2.35 production sector is populated");
        return SEMU_ERR_CONFLICT;
    }
    status = semu_bus_load(bus, PRODUCTION_ADDRESS, sector, sizeof(sector), error);
    if (status != SEMU_OK) return status;
    return semu_layer_hit(state, logger,
        "trigger=production-data ordinal=1 install synthetic ProductionData/ACCR/ACCC/MAGN/HLAT records",
        error);
}
