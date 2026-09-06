#include "sapporo_239_gps_reopen.h"
#include <string.h>

static const char *const hashes[] = {
    "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5",
    "85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89",
    "49a3936f4c9d61dbceee12324f41412334c42aa98b900f6f2d3fc5633ae43aea"
};
static semu_layer_intervention interventions[] = {
    {"gps-reopen-status", "queue synthetic reopen status after 10ms",
     "E-SAP-COMPAT-GPS-REOPEN-239-001", 1u, 0u},
    {"gps-gsr-status", "reply to exact @GSR with synthetic status after 10ms",
     "E-SAP-COMPAT-GPS-REOPEN-239-001", 1u, 0u}
};
const semu_layer_descriptor semu_sapporo_239_gps_reopen_layer = {
    "sapporo-2.39-gps-reopen", SEMU_LAYER_DEVICE_FIXTURE,
    "sapporo-2.39.20", "E-SAP-COMPAT-GPS-REOPEN-239-001",
    hashes, 3u, interventions, 2u, 2u
};

int semu_sapporo_239_gps_reopen_is_layer(const semu_layer_descriptor *d)
{
    return d != NULL && d->id != NULL &&
        strcmp(d->id, semu_sapporo_239_gps_reopen_layer.id) == 0;
}

static int initial_valid(const semu_sapporo_239_gps_context *s)
{
    return s != NULL && s->state != NULL && s->state->enabled &&
        s->state->descriptor == &s->descriptor &&
        s->descriptor.interventions == s->interventions &&
        s->descriptor.intervention_count == 2u &&
        semu_sapporo_239_gps_counts_valid(s->state->hits,
            s->interventions[0].hits, s->interventions[1].hits);
}

semu_status semu_sapporo_239_gps_reopen_bind(
    semu_sapporo_239_gps_reopen_context *c, semu_layer_state *state,
    semu_sapporo_239_gps_context *startup, semu_logger *logger, semu_error *error)
{
    if (c == NULL || state == NULL || !state->enabled || logger == NULL ||
        !initial_valid(startup) || startup->logger != logger ||
        (state->descriptor != &semu_sapporo_239_gps_reopen_layer &&
         state->descriptor != &c->descriptor) ||
        state->descriptor->intervention_count != 2u ||
        (state->descriptor == &c->descriptor &&
         state->descriptor->interventions != c->interventions) ||
        (c->state != NULL && c->state != state)) {
        semu_error_set(error, SEMU_ERR_STATE, "invalid 2.39 GPS reopen binding");
        return SEMU_ERR_STATE;
    }
    if (!semu_sapporo_239_gps_counts_valid(state->hits,
            state->descriptor->interventions[0].hits,
            state->descriptor->interventions[1].hits) ||
        (state->hits != 0u && startup->state->hits != 2u)) {
        semu_error_set(error, SEMU_ERR_FORMAT, "2.39 GPS reopen dependency lifecycle invalid");
        return SEMU_ERR_FORMAT;
    }
    if (state->descriptor == &semu_sapporo_239_gps_reopen_layer) {
        c->descriptor = semu_sapporo_239_gps_reopen_layer;
        memcpy(c->interventions, interventions, sizeof(c->interventions));
        c->descriptor.interventions = c->interventions;
        state->descriptor = &c->descriptor;
    }
    c->state = state; c->startup = startup; c->logger = logger;
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status preflight(semu_sapporo_239_gps_reopen_context *c,
    unsigned index, semu_error *error)
{
    if (c == NULL || c->state == NULL || c->logger == NULL ||
        !c->state->enabled || c->state->descriptor != &c->descriptor ||
        !initial_valid(c->startup) || c->startup->state->hits != 2u ||
        c->startup->logger != c->logger ||
        c->descriptor.intervention_count != 2u ||
        c->descriptor.interventions != c->interventions ||
        !semu_sapporo_239_gps_counts_valid(c->state->hits,
            c->interventions[0].hits, c->interventions[1].hits) ||
        c->state->hits != index || c->state->hits >= c->descriptor.maximum_hits ||
        c->interventions[index].hits >= c->interventions[index].max_hits) {
        semu_error_set(error, SEMU_ERR_STATE, "2.39 GPS reopen lifecycle or hit budget refused");
        return SEMU_ERR_STATE;
    }
    return SEMU_OK;
}

static semu_status queue_status(semu_sapporo_239_gps_reopen_context *c,
    semu_sapporo_cxd5610 *gps, unsigned index, semu_error *error)
{
    static const uint8_t status[] = "$PSS0000\r\n"; /* Synthetic parser fixture. */
    semu_status result = semu_sapporo_cxd5610_inject_rx_after(gps, status,
        sizeof(status) - 1u, UINT64_C(10000000), error);
    if (result != SEMU_OK) return result;
    return semu_layer_intervention_hit(c->state, c->logger, index, error);
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

semu_status semu_sapporo_239_gps_reopen_start(
    semu_sapporo_239_gps_reopen_context *c, semu_sapporo_cxd5610 *gps,
    semu_bus *bus, const semu_cpu_state *cpu, semu_error *error)
{
    uint8_t driver[0x348], uart[16], flags[2];
    uint32_t base, address;
    if (cpu == NULL || bus == NULL || gps == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "incomplete 2.39 GPS reopen");
        return SEMU_ERR_ARGUMENT;
    }
    if (cpu->r[15] != SEMU_SAPPORO_239_GPS_REOPEN_PC) return SEMU_OK;
    if (preflight(c, 0u, error) != SEMU_OK) return SEMU_ERR_STATE;
    base = cpu->r[4];
    if (!sram_span(base, sizeof(driver)) || cpu->r[5] != base + 0x74u ||
        cpu->r[6] != base + 0x270u) goto refused;
    if (semu_bus_copy_out(bus, base, driver, sizeof(driver), error) != SEMU_OK ||
        semu_bus_copy_out(bus, UINT32_C(0x100588a3), flags, sizeof(flags), error) != SEMU_OK)
        goto refused;
    address = le32(driver + 0x220u);
    if (!sram_span(address, sizeof(uart)) ||
        semu_bus_copy_out(bus, address, uart, sizeof(uart), error) != SEMU_OK)
        goto refused;
    if (driver[0x272] != 4u || driver[0x273] != 7u || flags[0] != 1u ||
        flags[1] != 0u || driver[0x74] != 15u || driver[0x7f] != 2u ||
        driver[0x7b] != 0u || driver[0x344] != 0xcbu || driver[0x345] != 4u ||
        le32(uart + 4u) != UINT32_C(0x0012890f) || le32(uart + 12u) != 0u)
        goto refused;
    return queue_status(c, gps, 0u, error);
refused:
    semu_error_set(error, SEMU_ERR_STATE, "2.39 GPS reopen driver/UART state mismatch");
    return SEMU_ERR_STATE;
}

semu_transaction_result semu_sapporo_239_gps_reopen_exchange(void *context,
    const uint8_t *request, size_t count, semu_sapporo_cxd5610 *gps,
    semu_error *error)
{
    semu_sapporo_239_gps_reopen_context *c = context;
    if (request == NULL || count != 6u || memcmp(request, "@GSR\r\n", 6u) != 0) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "2.39 GPS command is not exact @GSR");
        return SEMU_TRANSACTION_REFUSE;
    }
    if (preflight(c, 1u, error) != SEMU_OK ||
        queue_status(c, gps, 1u, error) != SEMU_OK) return SEMU_TRANSACTION_REFUSE;
    return SEMU_TRANSACTION_OK;
}
