#include "sapporo_235_gps_awake.h"

static const char *const hashes[] = {
    "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5",
    "36a14dc5bad7b9cb8a7c8164bfaaedaf68c75a9611bc3a9e6efaa47418a5a38a",
    "f281385acc8bab169976f9e506c230fd25124c44bfdbe2048393763a7d85ae22"
};
const semu_layer_descriptor semu_sapporo_235_gps_awake_layer = {
    .id = "sapporo-2.35-gps-awake",
    .kind = SEMU_LAYER_DEVICE_FIXTURE,
    .profile_id = "sapporo-2.35.34",
    .evidence = "E-SAP-0049",
    .component_hashes = hashes,
    .component_hash_count = 3u,
    .maximum_hits = 64u
};

static semu_status refuse(semu_sapporo_235_gps_awake_context *c, semu_error *error)
{
    semu_error local;
    semu_error_set(&local, SEMU_ERR_STATE,
        "2.35 GPS awake fixture disabled, exhausted or unexpected state");
    if (c != NULL && c->state != NULL && c->state->enabled) {
        if (c->refusal.code == SEMU_OK) c->refusal = local;
        local = c->refusal;
    }
    if (error != NULL) *error = local;
    return local.code;
}

static int ready(const semu_sapporo_235_gps_awake_context *c)
{
    const semu_sapporo_235_gps_reopen_context *r;
    const semu_sapporo_235_gps_context *s;
    if (c == NULL || c->state == NULL || c->logger == NULL ||
        !c->state->enabled || c->state->descriptor != &semu_sapporo_235_gps_awake_layer ||
        c->state->hits >= 64u || c->refusal.code != SEMU_OK) return 0;
    r = c->reopen;
    if (r == NULL || r->state == NULL || r->logger != c->logger ||
        !r->state->enabled || r->state->descriptor != &semu_sapporo_235_gps_reopen_layer ||
        r->state->hits != 2u || r->refusal.code != SEMU_OK) return 0;
    s = r->startup;
    return s != NULL && s->state != NULL && s->logger == c->logger &&
        s->state->enabled && s->state->descriptor == &semu_sapporo_235_gps_layer &&
        s->state->hits == 2u && s->refusal.code == SEMU_OK;
}

semu_status semu_sapporo_235_gps_awake_poll(semu_sapporo_235_gps_awake_context *c,
    semu_sapporo_cxd5610 *gps, semu_bus *bus, const semu_cpu_state *cpu,
    semu_error *error)
{
    uint8_t driver[0x348], flags[3];
    uint32_t base, config;
    semu_error failure;
    semu_status status;
    if (c != NULL && c->refusal.code != SEMU_OK) return refuse(c, error);
    if (cpu == NULL || bus == NULL || gps == NULL) return refuse(c, error);
    if (cpu->r[15] != SEMU_SAPPORO_235_GPS_AWAKE_PC) return SEMU_OK;
    if (!ready(c)) return refuse(c, error);
    base = cpu->r[8];
    if ((base & 3u) != 0u || base < UINT32_C(0x10000000) ||
        base > UINT32_C(0x10180000) - sizeof(driver) ||
        cpu->r[4] != UINT32_C(0x100589fe) || cpu->r[5] != base + 0x26cu ||
        cpu->r[6] != base + 0x314u || cpu->r[7] != base + 0x75u)
        return refuse(c, error);
    if (semu_bus_copy_out(bus, base, driver, sizeof(driver), error) != SEMU_OK ||
        semu_bus_copy_out(bus, UINT32_C(0x100589fe), flags, sizeof(flags), error) != SEMU_OK ||
        semu_bus_read(bus, UINT32_C(0x40010060), 4u, &config, error) != SEMU_OK ||
        driver[0x272] != 12u || driver[0x273] != 10u ||
        flags[0] != 1u || flags[1] != 0u || flags[2] != 0u || config != 0x93u)
        return refuse(c, error);
    /* E-SAP-0048/E-SAP-0049: finite synthetic GPIO edges through the real
     * IRQ path. No instruction/state patch, location data or recurring timer. */
    status = semu_sapporo_cxd5610_pulse_awake_after(gps, UINT64_C(100000000), &failure);
    if (status != SEMU_OK) {
        c->refusal = failure;
        if (error != NULL) *error = failure;
        return status;
    }
    return semu_layer_hit(c->state, c->logger,
        "trigger=gps-awake-poll synthetic-gpio24 delay-ns=100000000 width-ns=1000000",
        error);
}
