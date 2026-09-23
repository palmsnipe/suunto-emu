#include "sapporo_235_gps.h"
#include <string.h>

static const char *const hashes[] = {
    "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5",
    "36a14dc5bad7b9cb8a7c8164bfaaedaf68c75a9611bc3a9e6efaa47418a5a38a",
    "f281385acc8bab169976f9e506c230fd25124c44bfdbe2048393763a7d85ae22"
};
const semu_layer_descriptor semu_sapporo_235_gps_layer = {
    .id = "sapporo-2.35-gps-startup",
    .kind = SEMU_LAYER_DEVICE_FIXTURE,
    .profile_id = "sapporo-2.35.34",
    .evidence = "E-SAP-0046",
    .component_hashes = hashes,
    .component_hash_count = 3u,
    .maximum_hits = 2u
};

static semu_status refuse(semu_sapporo_235_gps_context *c, semu_error *error)
{
    semu_error local;
    semu_error_set(&local, SEMU_ERR_STATE,
        "2.35 GPS startup fixture disabled, exhausted or unexpected state/request");
    if (c != NULL && c->state != NULL && c->state->enabled) {
        if (c->refusal.code == SEMU_OK) c->refusal = local;
        local = c->refusal;
    }
    if (error != NULL) *error = local;
    return local.code;
}

static int ready(const semu_sapporo_235_gps_context *c, unsigned ordinal)
{
    return c != NULL && c->state != NULL && c->logger != NULL &&
        c->state->enabled && c->state->descriptor == &semu_sapporo_235_gps_layer &&
        c->refusal.code == SEMU_OK && c->state->hits == ordinal && ordinal < 2u;
}

static semu_status queue_status(semu_sapporo_235_gps_context *c,
    semu_sapporo_cxd5610 *gps, unsigned ordinal, semu_error *error)
{
    /* E-SAP-0046: inert synthetic parser status; no physical fix/time data. */
    static const uint8_t status[] = "$PSS0000\r\n";
    semu_error failure;
    if (semu_sapporo_cxd5610_inject_rx_after(gps, status, sizeof(status)-1u,
            UINT64_C(10000000), &failure) != SEMU_OK) {
        c->refusal = failure;
        if (error != NULL) *error = failure;
        return failure.code;
    }
    /* Single-threaded scheduling cannot dispatch before this preflighted hit. */
    return semu_layer_hit(c->state, c->logger, ordinal == 0u
        ? "trigger=gps-startup-status ordinal=1 synthetic-status delay-ns=10000000"
        : "trigger=gps-version-status ordinal=2 exact-VER synthetic-status delay-ns=10000000",
        error);
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8u |
        (uint32_t)p[2] << 16u | (uint32_t)p[3] << 24u;
}

static int sram_span(uint32_t address, uint32_t size)
{
    return (address & 3u) == 0u && address >= UINT32_C(0x10000000) &&
        size <= UINT32_C(0x180000) && address <= UINT32_C(0x10180000) - size;
}

semu_status semu_sapporo_235_gps_startup(semu_sapporo_235_gps_context *c,
    semu_sapporo_cxd5610 *gps, semu_bus *bus, const semu_cpu_state *cpu,
    semu_error *error)
{
    uint8_t driver[0x348], uart[16], retry;
    uint32_t base, address;
    if (c != NULL && c->refusal.code != SEMU_OK) return refuse(c, error);
    if (cpu == NULL || bus == NULL || gps == NULL) return refuse(c, error);
    if (cpu->r[15] != SEMU_SAPPORO_235_GPS_PC) return SEMU_OK;
    if (!ready(c, 0u)) return refuse(c, error);
    base = cpu->r[4];
    if (!sram_span(base, sizeof(driver)) || cpu->r[5] != base + 0x270u ||
        cpu->r[6] != base + 0x338u) return refuse(c, error);
    if (semu_bus_copy_out(bus, base, driver, sizeof(driver), error) != SEMU_OK ||
        semu_bus_copy_out(bus, UINT32_C(0x10058a00), &retry, 1u, error) != SEMU_OK)
        return refuse(c, error);
    address = le32(driver + 0x220u);
    if (!sram_span(address, sizeof(uart)) ||
        semu_bus_copy_out(bus, address, uart, sizeof(uart), error) != SEMU_OK)
        return refuse(c, error);
    if (driver[0x272] != 4u || driver[0x273] != 2u || retry != 0u ||
        le32(uart + 4u) != UINT32_C(0x001250e7) || le32(uart + 12u) != 0u)
        return refuse(c, error);
    return queue_status(c, gps, 0u, error);
}

semu_transaction_result semu_sapporo_235_gps_exchange(void *context,
    const uint8_t *request, size_t count, semu_sapporo_cxd5610 *gps,
    semu_error *error)
{
    semu_sapporo_235_gps_context *c = context;
    if (!ready(c, 1u) || request == NULL || count != 6u ||
        memcmp(request, "@VER\r\n", 6u) != 0) {
        (void)refuse(c, error);
        return SEMU_TRANSACTION_REFUSE;
    }
    return queue_status(c, gps, 1u, error) == SEMU_OK
        ? SEMU_TRANSACTION_OK : SEMU_TRANSACTION_REFUSE;
}
