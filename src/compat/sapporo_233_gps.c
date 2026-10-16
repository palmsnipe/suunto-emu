#include "sapporo_233_gps.h"
#include <string.h>

static const char *const hashes[] = {
    "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5",
    "ba286a7bce55dd2f7aba81436d3c5152401c28d86bef89b2c9dbbacd04b7e9e5",
    "b06492a982faa188a14d0a458e7165f9e0b13d0917ded3c3e37077f6ef68d5c5"
};
/* The staged-window response order plus the E-SAP-0077 re-arm: the lane
 * arms three times in 16 s with the same allocation and driver state 4/2,
 * so the budget is three admissions. The reopen arm (ordinal 2) skips the
 * retry and UART-callback checks: the lane shows the retry byte counts
 * un-answered arms (guest history, not an admission key) and the UART
 * callback slot is installed after the arm instant. */
#define SEMU_SAPPORO_233_GPS_HITS 6u

const semu_layer_descriptor semu_sapporo_233_gps_layer = {
    .id = "sapporo-2.33-gps-startup",
    .kind = SEMU_LAYER_DEVICE_FIXTURE,
    .profile_id = "sapporo-2.33.16",
    .evidence = "E-SAP-0071",
    .component_hashes = hashes,
    .component_hash_count = 3u,
    .maximum_hits = SEMU_SAPPORO_233_GPS_HITS
};

static semu_status refuse(semu_sapporo_233_gps_context *c, semu_error *error)
{
    semu_error local;
    semu_error_set(&local, SEMU_ERR_STATE,
        "2.33 GPS startup fixture disabled, exhausted or unexpected state/request");
    if (c != NULL && c->state != NULL && c->state->enabled) {
        if (c->refusal.code == SEMU_OK) c->refusal = local;
        local = c->refusal;
    }
    if (error != NULL) *error = local;
    return local.code;
}

static semu_status queue_status(semu_sapporo_233_gps_context *c,
    semu_sapporo_cxd5610 *gps, unsigned ordinal, semu_error *error)
{
    /* Synthetic inert status, not physical receiver data. E-SAP-0071 proves
     * the native prefix consumer and this bounded UART lifecycle; E-SAP-0077
     * proves the re-arm carries the same shape. No power transition-to-status
     * timing law is known, so admission is explicit. */
    static const uint8_t status[] = "$PSS0000\r\n";
    semu_error failure;
    if (semu_sapporo_cxd5610_inject_rx_after(gps, status, sizeof(status)-1u,
            UINT64_C(10000000), &failure) != SEMU_OK) {
        c->refusal = failure;
        if (error != NULL) *error = failure;
        return failure.code;
    }
    /* Preflighted hit; no callback can run during single-threaded admission. */
    return semu_layer_hit(c->state, c->logger, ordinal == 0u
        ? "trigger=gps-startup-status ordinal=1 synthetic-status delay-ns=10000000"
        : ordinal == 1u
        ? "trigger=gps-version-status ordinal=2 exact-VER synthetic-status delay-ns=10000000"
        : "trigger=gps-rearm-status ordinal=3 synthetic-status delay-ns=10000000",
        error);
}

static int sram_span(uint32_t address, uint32_t size)
{
    return (address & 3u) == 0u && address >= UINT32_C(0x10000000) &&
        size <= UINT32_C(0x180000) && address <= UINT32_C(0x10180000) - size;
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8u |
        (uint32_t)p[2] << 16u | (uint32_t)p[3] << 24u;
}

semu_status semu_sapporo_233_gps_startup(semu_sapporo_233_gps_context *c,
    semu_sapporo_cxd5610 *gps, semu_bus *bus, const semu_cpu_state *cpu,
    semu_error *error)
{
    uint8_t driver[0x348], uart[16], retry;
    if (c != NULL && c->refusal.code != SEMU_OK) return refuse(c, error);
    if (cpu == NULL || bus == NULL || gps == NULL) return refuse(c, error);
    if (cpu->r[15] != SEMU_SAPPORO_233_GPS_PC) return SEMU_OK;
    if (c->state == NULL ||
        (c->state->hits % 2u != 0u) ||
        c->state->hits >= SEMU_SAPPORO_233_GPS_HITS ||
        !c->state->enabled ||
        c->state->descriptor != &semu_sapporo_233_gps_layer ||
        c->refusal.code != SEMU_OK || c->logger == NULL) {
        return refuse(c, error);
    }
    /* Only the observed initial allocation/shape is admitted. A different
     * driver or UART allocation needs evidence; no alternate lifecycle. */
    if (cpu->r[4] != UINT32_C(0x10039d40) ||
        cpu->r[5] != UINT32_C(0x10039fb0) ||
        cpu->r[6] != UINT32_C(0x1003a078)) return refuse(c, error);
    if (semu_bus_copy_out(bus, UINT32_C(0x10039d40), driver,
            sizeof(driver), error) != SEMU_OK ||
        semu_bus_copy_out(bus, UINT32_C(0x1005b8f8), &retry, 1u,
            error) != SEMU_OK) return refuse(c, error);
    if (driver[0x272] != 4u || driver[0x273] != 2u)
        return refuse(c, error);
    {
        /* The UART struct address comes from the driver's own +0x220
         * word: 0x10048d40 in the initial window (E-SAP-0071) and the
         * guest's post-exchange re-allocation 0x10049400 at the
         * E-SAP-0077 re-arm. The callback stays the E-SAP-0071 install;
         * the retry byte counts un-answered arms (guest history), so it
         * is an admission key only in the initial window (zero there). */
        uint32_t uart_base = le32(driver + 0x220u);
        if (!sram_span(uart_base, sizeof(uart))) return refuse(c, error);
        if (semu_bus_copy_out(bus, uart_base, uart, sizeof(uart),
                error) != SEMU_OK ||
            le32(uart + 4u) != UINT32_C(0x0011d593) ||
            le32(uart + 12u) != 0u) return refuse(c, error);
        if (c->state->hits == 0u && retry != 0u) return refuse(c, error);
    }
    return queue_status(c, gps, c->state->hits, error);
}

semu_transaction_result semu_sapporo_233_gps_exchange(void *context,
    const uint8_t *request, size_t count, semu_sapporo_cxd5610 *gps,
    semu_error *error)
{
    semu_sapporo_233_gps_context *c = context;
    /* Every admitted arm carries the same exact @VER exchange after its
     * status: hits 1 (initial), 3 and 5 (the E-SAP-0077 re-arms). */
    if ((c == NULL || c->state == NULL ||
         c->state->hits == 0u || c->state->hits % 2u == 0u ||
         c->state->hits >= SEMU_SAPPORO_233_GPS_HITS ||
         !c->state->enabled ||
         c->state->descriptor != &semu_sapporo_233_gps_layer ||
         c->refusal.code != SEMU_OK || c->logger == NULL) ||
        request == NULL || count != 6u ||
        memcmp(request, "@VER\r\n", 6u) != 0) {
        (void)refuse(c, error);
        return SEMU_TRANSACTION_REFUSE;
    }
    return queue_status(c, gps, c->state->hits, error) == SEMU_OK
        ? SEMU_TRANSACTION_OK : SEMU_TRANSACTION_REFUSE;
}
