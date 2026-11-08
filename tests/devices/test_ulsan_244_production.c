#include "semu/hash.h"
#include "test.h"
#include "../../src/compat/ulsan_244_production.h"

#include <string.h>

#define TABLE_ADDRESS 0x001c6604u
#define SECTOR_ADDRESS 0x1803f000u

static void checksum_table(uint8_t table[64])
{
    unsigned index, bit;
    for (index = 0u; index < 16u; ++index) {
        uint32_t word = index;
        for (bit = 0u; bit < 4u; ++bit)
            word = (word >> 1u) ^ ((word & 1u) ? UINT32_C(0xedb88320) : 0u);
        word ^= UINT32_C(0x4dbdf21c);
        for (bit = 0u; bit < 4u; ++bit)
            table[index * 4u + bit] = (uint8_t)(word >> (bit * 8u));
    }
}

static void test_244_production_fixture(semu_test_context *context)
{
    const semu_layer_descriptor *layer = &semu_ulsan_244_production_layer;
    semu_error error;
    semu_layer_state state = {0};
    uint8_t table[64], sector[4096], digest[SEMU_SHA256_SIZE];
    char hash[65];
    semu_bus *bus = semu_bus_create(&error);

    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
                     semu_ulsan_244_install_production(bus, &state, NULL, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        semu_bus_copy_out(bus, SECTOR_ADDRESS, sector, sizeof(sector), &error));
    checksum_table(table);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_map_ram(bus, "application", TABLE_ADDRESS, sizeof(table), &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_layer_enable_checked(&state, layer, "ulsan-2.44.52",
            layer->component_hashes, layer->component_hash_count, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
                     semu_ulsan_244_install_production(bus, &state, NULL, &error));
    SEMU_TEST_EQ_U64(context, 0u, state.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(bus, TABLE_ADDRESS, table, sizeof(table), &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_ulsan_244_install_production(bus, &state, NULL, &error));
    SEMU_TEST_EQ_U64(context, 1u, state.hits);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
                     semu_ulsan_244_install_production(bus, &state, NULL, &error));
    SEMU_TEST_ASSERT(context,
        semu_bus_write(bus, SECTOR_ADDRESS, 1u, 0u, &error) != SEMU_OK);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(bus, SECTOR_ADDRESS, sector, sizeof(sector), &error));
    semu_sha256(sector, sizeof(sector), digest);
    semu_sha256_format(digest, hash);
    SEMU_TEST_ASSERT(context, strcmp(hash,
        "c08816067aed620fb8c3a074f5f0e3a8ceb398416d6f9c33d1f6c13df5619a53") == 0);

    semu_bus_reset(bus);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(bus, TABLE_ADDRESS, table, sizeof(table), &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_layer_enable(&state, layer, "ulsan-2.44.52", &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_ulsan_244_install_production(bus, &state, NULL, &error));
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {SEMU_TEST_CASE(test_244_production_fixture)};
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
