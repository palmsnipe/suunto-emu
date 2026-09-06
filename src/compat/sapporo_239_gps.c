#include "sapporo_239_gps.h"

#include <string.h>

static const char *const hashes[] = {
    "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5",
    "85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89",
    "49a3936f4c9d61dbceee12324f41412334c42aa98b900f6f2d3fc5633ae43aea"
};
static semu_layer_intervention interventions[] = {
    {"gps-startup-status", "queue synthetic startup status after 10ms",
     "E-SAP-COMPAT-GPS-STARTUP-239-001", 1u, 0u},
    {"gps-version-status", "reply to exact @VER with synthetic status after 10ms",
     "E-SAP-COMPAT-GPS-STARTUP-239-001", 1u, 0u}
};
const semu_layer_descriptor semu_sapporo_239_gps_layer = {
    "sapporo-2.39-gps-startup", SEMU_LAYER_DEVICE_FIXTURE,
    "sapporo-2.39.20", "E-SAP-COMPAT-GPS-STARTUP-239-001",
    hashes, 3u, interventions, 2u, 2u
};

int semu_sapporo_239_gps_is_layer(const semu_layer_descriptor *d)
{
    return d != NULL && d->id != NULL &&
        strcmp(d->id, semu_sapporo_239_gps_layer.id) == 0;
}

int semu_sapporo_239_gps_counts_valid(uint64_t total, uint64_t startup,
                                      uint64_t reply)
{
    return startup <= 1u && reply <= startup && total == startup + reply;
}

semu_status semu_sapporo_239_gps_bind(semu_sapporo_239_gps_context *c,
    semu_layer_state *state, semu_logger *logger, semu_error *error)
{
    if (c == NULL || state == NULL || !state->enabled || logger == NULL ||
        (state->descriptor != &semu_sapporo_239_gps_layer &&
         state->descriptor != &c->descriptor) ||
        (c->state != NULL && c->state != state)) {
        semu_error_set(error, SEMU_ERR_STATE, "invalid 2.39 GPS fixture binding");
        return SEMU_ERR_STATE;
    }
    if (state->descriptor == &semu_sapporo_239_gps_layer) {
        c->descriptor = semu_sapporo_239_gps_layer;
        memcpy(c->interventions, interventions, sizeof(c->interventions));
        c->descriptor.interventions = c->interventions;
        state->descriptor = &c->descriptor;
    }
    c->state = state;
    c->logger = logger;
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status preflight(semu_sapporo_239_gps_context *c,
                              unsigned index, semu_error *error)
{
    if (c == NULL || c->state == NULL || c->logger == NULL ||
        !c->state->enabled || c->state->descriptor != &c->descriptor ||
        c->descriptor.intervention_count != 2u ||
        c->descriptor.interventions != c->interventions ||
        !semu_sapporo_239_gps_counts_valid(c->state->hits,
            c->interventions[0].hits, c->interventions[1].hits) ||
        c->state->hits != index ||
        c->state->hits >= c->descriptor.maximum_hits ||
        c->interventions[index].hits >= c->interventions[index].max_hits) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "2.39 GPS startup lifecycle or hit budget refused");
        return SEMU_ERR_STATE;
    }
    return SEMU_OK;
}

static semu_status queue_status(semu_sapporo_239_gps_context *c,
    semu_sapporo_cxd5610 *gps, unsigned index, semu_error *error)
{
    /* Synthetic parser fixture, not captured physical status semantics. */
    static const uint8_t status[] = "$PSS0000\r\n";
    semu_status result = semu_sapporo_cxd5610_inject_rx_after(gps, status,
        sizeof(status) - 1u, UINT64_C(10000000), error);
    if (result != SEMU_OK) return result;
    /* All hit conditions were preflighted; scheduler insertion is atomic
     * and cannot invoke a callback before this commit. */
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
        size <= UINT32_C(0x180000) &&
        address <= UINT32_C(0x10180000) - size;
}

semu_status semu_sapporo_239_gps_startup(semu_sapporo_239_gps_context *c,
    semu_sapporo_cxd5610 *gps, semu_bus *bus, const semu_cpu_state *cpu,
    semu_error *error)
{
    uint8_t driver[0x348], uart[16], retry;
    uint32_t base, uart_address;
    if (cpu == NULL || bus == NULL || gps == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "incomplete 2.39 GPS startup");
        return SEMU_ERR_ARGUMENT;
    }
    if (cpu->r[15] != SEMU_SAPPORO_239_GPS_PC) return SEMU_OK;
    if (preflight(c, 0u, error) != SEMU_OK) return SEMU_ERR_STATE;
    base = cpu->r[4];
    if (!sram_span(base, sizeof(driver)) || cpu->r[5] != base + 0x270u ||
        cpu->r[6] != base + 0x338u) goto refused;
    if (semu_bus_copy_out(bus, base, driver, sizeof(driver), error) != SEMU_OK ||
        semu_bus_copy_out(bus, UINT32_C(0x100588a4), &retry, 1u, error) != SEMU_OK)
        goto refused;
    uart_address = le32(driver + 0x220u);
    if (!sram_span(uart_address, sizeof(uart)) ||
        semu_bus_copy_out(bus, uart_address, uart, sizeof(uart), error) != SEMU_OK)
        goto refused;
    if (driver[0x272] != 4u || driver[0x273] != 2u || retry != 0u ||
        le32(uart + 4u) != UINT32_C(0x0012890f) || le32(uart + 12u) != 0u)
        goto refused;
    return queue_status(c, gps, 0u, error);
refused:
    semu_error_set(error, SEMU_ERR_STATE, "2.39 GPS initial driver/UART state mismatch");
    return SEMU_ERR_STATE;
}

semu_transaction_result semu_sapporo_239_gps_exchange(void *context,
    const uint8_t *request, size_t count, semu_sapporo_cxd5610 *gps,
    semu_error *error)
{
    semu_sapporo_239_gps_context *c = context;
    if (request == NULL || count != 6u || memcmp(request, "@VER\r\n", 6u) != 0) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "2.39 GPS command is not exact @VER");
        return SEMU_TRANSACTION_REFUSE;
    }
    if (preflight(c, 1u, error) != SEMU_OK ||
        queue_status(c, gps, 1u, error) != SEMU_OK) return SEMU_TRANSACTION_REFUSE;
    return SEMU_TRANSACTION_OK;
}
