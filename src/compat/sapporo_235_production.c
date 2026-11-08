#include "sapporo_235_production.h"
#include "production_records.h"

#include <string.h>

#define PRODUCTION_ADDRESS UINT32_C(0x14fff000)
#define TABLE_ADDRESS UINT32_C(0x001b1644)

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

semu_status semu_sapporo_235_install_production(semu_bus *bus,
    semu_layer_state *state, semu_logger *logger, semu_error *error)
{
    uint8_t sector[SEMU_PRODUCTION_SECTOR_SIZE];
    uint8_t previous[SEMU_PRODUCTION_SECTOR_SIZE];
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
    status = semu_build_production_sector(bus, TABLE_ADDRESS, sector, error);
    if (status != SEMU_OK) return status;
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
