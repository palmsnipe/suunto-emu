#include "ulsan_244_production.h"
#include "production_records.h"

#include "semu/hash.h"
#include <string.h>

#define PRODUCTION_ADDRESS UINT32_C(0x1803f000)
#define TABLE_ADDRESS UINT32_C(0x001c6604)

static const char *const hashes[] = {
    "3ba7a5b312a2fd4fdb4c327270ec66888f3fc03940bf8b5184233dad221369d9",
    "64827b1c4653464e5d15de9b3efd01243193604b24891b651f28f5c6da6d34ef",
    "81f861d4541d5c9f0e6b0cb73c2b3e44197d5abc6f147e1de8005da9af87f524"
};

const semu_layer_descriptor semu_ulsan_244_production_layer = {
    .id = "ulsan-2.44-production-data",
    .kind = SEMU_LAYER_SYNTHETIC_STATE,
    .profile_id = "ulsan-2.44.52",
    .evidence = "E-ULS-0078",
    .component_hashes = hashes,
    .component_hash_count = SEMU_ARRAY_LEN(hashes),
    .maximum_hits = 1u
};

const semu_layer_descriptor semu_ulsan_244_emmc_layer = {
    .id = "ulsan-2.44-emmc",
    .kind = SEMU_LAYER_SYNTHETIC_STATE,
    .profile_id = "ulsan-2.44.52",
    .evidence = "E-ULS-0079",
    .component_hashes = hashes,
    .component_hash_count = SEMU_ARRAY_LEN(hashes),
    .maximum_hits = 1u
};

semu_status semu_ulsan_244_install_production(semu_bus *bus,
    semu_layer_state *state, semu_logger *logger, semu_error *error)
{
    static const char expected[] =
        "c08816067aed620fb8c3a074f5f0e3a8ceb398416d6f9c33d1f6c13df5619a53";
    uint8_t sector[SEMU_PRODUCTION_SECTOR_SIZE], prior[SEMU_PRODUCTION_SECTOR_SIZE];
    uint8_t digest[SEMU_SHA256_SIZE];
    char hash[65];
    semu_status status;

    if (bus == NULL || state == NULL || !state->enabled ||
        state->descriptor != &semu_ulsan_244_production_layer ||
        state->hits != 0u) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "Ulsan production data layer disabled or exhausted");
        return SEMU_ERR_STATE;
    }
    status = semu_build_production_sector(bus, TABLE_ADDRESS, sector, error);
    if (status != SEMU_OK)
        return status;
    semu_sha256(sector, sizeof(sector), digest);
    semu_sha256_format(digest, hash);
    if (strcmp(hash, expected) != 0) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "Ulsan synthetic production sector hash mismatch");
        return SEMU_ERR_CONFLICT;
    }
    status = semu_bus_copy_out(bus, PRODUCTION_ADDRESS, prior, sizeof(prior), NULL);
    if (status == SEMU_ERR_RANGE) {
        status = semu_bus_map_rom(bus, "ulsan.synthetic-production",
                                  PRODUCTION_ADDRESS, sector, sizeof(sector), error);
        if (status != SEMU_OK)
            return status;
    } else if (status != SEMU_OK || memcmp(prior, sector, sizeof(sector)) != 0) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "Ulsan production sector is already populated");
        return SEMU_ERR_CONFLICT;
    }
    return semu_layer_hit(state, logger,
        "trigger=production-data classification=synthetic install verified first-boot records",
        error);
}
