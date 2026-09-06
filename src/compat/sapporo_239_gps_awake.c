#include "sapporo_239_gps_awake.h"
#include <string.h>

static const char *const hashes[] = {
    "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5",
    "85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89",
    "49a3936f4c9d61dbceee12324f41412334c42aa98b900f6f2d3fc5633ae43aea"
};
static semu_layer_intervention intervention = {
    "gps-awake-pulse", "queue synthetic GPIO24 pulse after 100ms, high for 1ms",
    "E-SAP-COMPAT-GPS-AWAKE-239-001", 4u, 0u
};
/* No physical cadence is known: this exact native poll is a bounded fixture
 * trigger, not an instruction replacement or a periodic receiver model. */
const semu_layer_descriptor semu_sapporo_239_gps_awake_layer = {
    "sapporo-2.39-gps-awake", SEMU_LAYER_DEVICE_FIXTURE,
    "sapporo-2.39.20", "E-SAP-COMPAT-GPS-AWAKE-239-001",
    hashes, 3u, &intervention, 1u, 4u
};

int semu_sapporo_239_gps_awake_is_layer(const semu_layer_descriptor *d)
{
    return d != NULL && d->id != NULL &&
        strcmp(d->id, semu_sapporo_239_gps_awake_layer.id) == 0;
}

static int dependency_valid(const semu_layer_state *s)
{
    return s != NULL && s->enabled && s->descriptor != NULL &&
        s->descriptor->intervention_count == 2u &&
        s->descriptor->interventions != NULL &&
        semu_sapporo_239_gps_counts_valid(s->hits,
            s->descriptor->interventions[0].hits,
            s->descriptor->interventions[1].hits);
}

semu_status semu_sapporo_239_gps_awake_validate(const semu_layer_state *state,
    const semu_layer_state *startup, const semu_layer_state *reopen,
    semu_error *error)
{
    if (state == NULL || !state->enabled ||
        !semu_sapporo_239_gps_awake_is_layer(state->descriptor) ||
        state->descriptor->intervention_count != 1u ||
        state->descriptor->interventions == NULL ||
        state->descriptor->maximum_hits != 4u ||
        state->descriptor->interventions[0].max_hits != 4u ||
        state->hits > 4u || state->hits != state->descriptor->interventions[0].hits ||
        !dependency_valid(startup) || !dependency_valid(reopen) ||
        !semu_sapporo_239_gps_is_layer(startup->descriptor) ||
        !semu_sapporo_239_gps_reopen_is_layer(reopen->descriptor) ||
        (reopen->hits != 0u && startup->hits != 2u) ||
        (state->hits != 0u && (startup->hits != 2u || reopen->hits != 2u))) {
        semu_error_set(error, SEMU_ERR_FORMAT, "2.39 GPS awake dependency lifecycle invalid");
        return SEMU_ERR_FORMAT;
    }
    return SEMU_OK;
}

semu_status semu_sapporo_239_gps_awake_bind(
    semu_sapporo_239_gps_awake_context *c, semu_layer_state *state,
    const semu_layer_state *startup, const semu_layer_state *reopen,
    semu_logger *logger, semu_error *error)
{
    if (c == NULL || state == NULL || logger == NULL ||
        (state->descriptor != &semu_sapporo_239_gps_awake_layer &&
         state->descriptor != &c->descriptor) ||
        (state->descriptor == &c->descriptor &&
         c->descriptor.interventions != &c->intervention) ||
        (c->state != NULL && c->state != state)) {
        semu_error_set(error, SEMU_ERR_STATE, "invalid 2.39 GPS awake binding");
        return SEMU_ERR_STATE;
    }
    semu_status status = semu_sapporo_239_gps_awake_validate(state, startup, reopen, error);
    if (status != SEMU_OK) return status;
    if (state->descriptor == &semu_sapporo_239_gps_awake_layer) {
        c->descriptor = semu_sapporo_239_gps_awake_layer;
        c->intervention = intervention;
        c->descriptor.interventions = &c->intervention;
        state->descriptor = &c->descriptor;
    }
    c->state = state; c->startup = startup; c->reopen = reopen; c->logger = logger;
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_sapporo_239_gps_awake_poll(
    semu_sapporo_239_gps_awake_context *c, semu_sapporo_cxd5610 *gps,
    semu_bus *bus, const semu_cpu_state *cpu, semu_error *error)
{
    uint8_t driver[0x348], flags[3];
    uint32_t base, config;
    semu_status status;
    if (cpu == NULL || bus == NULL || gps == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "incomplete 2.39 GPS awake poll");
        return SEMU_ERR_ARGUMENT;
    }
    if (cpu->r[15] != SEMU_SAPPORO_239_GPS_AWAKE_PC) return SEMU_OK;
    if (c == NULL || c->state == NULL || c->logger == NULL ||
        c->state->descriptor != &c->descriptor ||
        c->descriptor.interventions != &c->intervention ||
        semu_sapporo_239_gps_awake_validate(c->state, c->startup, c->reopen, error) != SEMU_OK ||
        c->startup->hits != 2u || c->reopen->hits != 2u || c->state->hits >= 4u) {
        semu_error_set(error, SEMU_ERR_STATE, "2.39 GPS awake lifecycle or hit budget refused");
        return SEMU_ERR_STATE;
    }
    base = cpu->r[8];
    if ((base & 3u) != 0u || base < UINT32_C(0x10000000) ||
        base > UINT32_C(0x10180000) - sizeof(driver) ||
        cpu->r[0] != 1u || cpu->r[2] != 12u || cpu->r[4] != UINT32_C(0x100588a2) ||
        cpu->r[5] != base + 0x26cu || cpu->r[6] != base + 0x314u ||
        cpu->r[7] != base + 0x75u) goto refused;
    if (semu_bus_copy_out(bus, base, driver, sizeof(driver), error) != SEMU_OK ||
        semu_bus_copy_out(bus, UINT32_C(0x100588a2), flags, sizeof(flags), error) != SEMU_OK ||
        semu_bus_read(bus, UINT32_C(0x40010060), 4u, &config, error) != SEMU_OK ||
        driver[0x272] != 12u || driver[0x273] != 10u || flags[0] != 1u ||
        flags[2] != 0u || config != 0x93u) goto refused;
    status = semu_sapporo_cxd5610_pulse_awake_after(gps, UINT64_C(100000000), error);
    if (status != SEMU_OK) return status;
    return semu_layer_intervention_hit(c->state, c->logger, 0u, error);
refused:
    semu_error_set(error, SEMU_ERR_STATE, "2.39 GPS awake driver/GPIO state mismatch");
    return SEMU_ERR_STATE;
}
