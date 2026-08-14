#include "sapporo_222.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

int main(void)
{
    semu_bus *bus;
    semu_layer_state state;
    semu_logger logger;
    semu_error error;
    uint8_t table[64];
    uint8_t sector[4096];
    size_t i;
    semu_error_clear(&error);
    semu_log_init(&logger, NULL, SEMU_LOG_ERROR);
    bus = semu_bus_create(&error);
    CHECK(bus != NULL);
    CHECK(semu_bus_map_ram(bus, "firmware", 0u, 0x200000u, &error) == SEMU_OK);
    CHECK(semu_bus_map_ram(bus, "flash", 0x14000000u, 0x02000000u,
                           &error) == SEMU_OK);
    for (i = 0u; i < sizeof(table); ++i) {
        table[i] = (uint8_t)(i * 17u + 3u);
    }
    CHECK(semu_bus_load(bus, 0x00199ee8u, table, sizeof(table), &error) == SEMU_OK);
    CHECK(semu_layer_enable(&state, &semu_sapporo_222_no_device_layer,
                            "sapporo-2.22.60", &error) == SEMU_OK);
    CHECK(semu_sapporo_222_install_no_device(bus, &state, &logger, &error) ==
          SEMU_OK);
    CHECK(state.hits == 1u);
    CHECK(semu_bus_copy_out(bus, 0x14fff000u, sector, sizeof(sector), &error) ==
          SEMU_OK);
    CHECK(memcmp(sector, "ProductionData", 14u) == 0);
    CHECK(memcmp(sector + 256u, "ACCR", 4u) == 0);
    CHECK(memcmp(sector + 512u, "ACCC", 4u) == 0);
    CHECK(memcmp(sector + 768u, "MAGN", 4u) == 0);
    CHECK(memcmp(sector + 1536u, "HLAT", 4u) == 0);
    CHECK(memcmp(sector + 1552u, "EMUHLAT00001", 12u) == 0);
    CHECK(semu_sapporo_222_install_no_device(bus, &state, &logger, &error) ==
          SEMU_ERR_STATE);
    semu_bus_destroy(bus);
    return failures != 0 ? 1 : 0;
}
