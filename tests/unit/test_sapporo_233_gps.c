#include "test.h"
#include "sapporo_233_gps.h"
#include <stdio.h>
#include <string.h>

typedef struct fixture {
    semu_sapporo_233_gps_context provider;
    semu_layer_state state;
    semu_logger logger;
    FILE *log;
    semu_scheduler *scheduler;
    semu_sapporo_cxd5610 *gps;
    semu_bus *bus;
    semu_cpu_state cpu;
    uint8_t received[20];
    unsigned rx;
    uint64_t rx_time;
} fixture;

static void receive(void *context, uint8_t byte, uint64_t now)
{
    fixture *f = context;
    if (f->rx < sizeof(f->received)) f->received[f->rx] = byte;
    ++f->rx;
    f->rx_time = now;
}

static int init(fixture *f, semu_error *e)
{
    memset(f, 0, sizeof(*f));
    f->log = tmpfile();
    f->scheduler = semu_scheduler_create(e);
    f->bus = semu_bus_create(e);
    f->gps = semu_sapporo_cxd5610_create(f->scheduler, NULL, NULL,
        receive, f, NULL, NULL, e);
    if (!f->log || !f->scheduler || !f->bus || !f->gps) return 0;
    semu_log_init(&f->logger, f->log, SEMU_LOG_DEBUG);
    if (semu_layer_enable(&f->state, &semu_sapporo_233_gps_layer,
            "sapporo-2.33.16", e) != SEMU_OK ||
        semu_bus_map_ram(f->bus, "sram", 0x10000000u, 0x180000u, e) != SEMU_OK)
        return 0;
    f->provider.state = &f->state; f->provider.logger = &f->logger;
    semu_sapporo_cxd5610_set_exchange(f->gps, semu_sapporo_233_gps_exchange,
        &f->provider);
    /* E-SAP-0071 native initial-arm tuple; no firmware bytes. */
    f->cpu.r[15] = SEMU_SAPPORO_233_GPS_PC;
    f->cpu.r[0] = 2u;
    f->cpu.r[4] = 0x10039d40u;
    f->cpu.r[5] = 0x10039fb0u; f->cpu.r[6] = 0x1003a078u;
    return semu_bus_write(f->bus, 0x10039f60u, 4u, 0x10048d40u, e) == SEMU_OK &&
        semu_bus_write(f->bus, 0x10048d44u, 4u, 0x11d593u, e) == SEMU_OK &&
        semu_bus_write(f->bus, 0x10039fb2u, 2u, 0x0204u, e) == SEMU_OK;
}

static void destroy(fixture *f)
{
    semu_bus_destroy(f->bus);
    semu_sapporo_cxd5610_destroy(f->gps);
    semu_scheduler_destroy(f->scheduler);
    if (f->log) (void)fclose(f->log);
}

static semu_status startup(fixture *f, semu_error *e)
{
    return semu_sapporo_233_gps_startup(&f->provider, f->gps, f->bus,
        &f->cpu, e);
}

static semu_transaction_result send(fixture *f, const char *text, semu_error *e)
{
    semu_serial_endpoint ep = semu_sapporo_cxd5610_endpoint(f->gps);
    semu_serial_transaction tx = {0u, 0u, (const uint8_t *)text,
        strlen(text), NULL, 0u};
    return ep.transfer(ep.context, &tx, e);
}

static void test_sapporo_233_gps_activation(semu_test_context *context)
{
    const semu_layer_descriptor *d = &semu_sapporo_233_gps_layer;
    const char *hashes[3];
    semu_layer_state state;
    semu_error e;
    unsigned i;
    SEMU_TEST_EQ_U64(context, 2u, d->maximum_hits);
    SEMU_TEST_EQ_U64(context, 3u, d->component_hash_count);
    SEMU_TEST_ASSERT(context, strcmp(d->evidence, "E-SAP-0071") == 0);
    for (i = 0u; i < 3u; ++i) hashes[i] = d->component_hashes[i];
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_layer_enable_checked(
        &state, d, "sapporo-2.33.16", hashes, 3u, &e));
    for (i = 0u; i < 3u; ++i) {
        hashes[i] = "wrong";
        SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT, semu_layer_enable_checked(
            &state, d, "sapporo-2.33.16", hashes, 3u, &e));
        SEMU_TEST_ASSERT(context, !state.enabled);
        hashes[i] = d->component_hashes[i];
    }
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT, semu_layer_enable_checked(
        &state, d, "sapporo-2.35.34", hashes, 3u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, semu_layer_enable_checked(
        &state, d, "sapporo-2.33.16", hashes, 2u, &e));
    SEMU_TEST_ASSERT(context, !state.enabled);
}

static void test_sapporo_233_gps_delayed_and_isolated(semu_test_context *context)
{
    fixture a, b;
    semu_cpu_state cpu;
    semu_error e;
    char log[1024];
    size_t count;
    SEMU_TEST_ASSERT(context, init(&a, &e));
    SEMU_TEST_ASSERT(context, init(&b, &e));
    cpu = a.cpu;
    SEMU_TEST_EQ_U64(context, SEMU_OK, startup(&a, &e));
    SEMU_TEST_ASSERT(context, memcmp(&cpu, &a.cpu, sizeof(cpu)) == 0);
    SEMU_TEST_EQ_U64(context, 1u, a.state.hits);
    SEMU_TEST_EQ_U64(context, 0u, b.state.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_scheduler_advance(a.scheduler, 9999999u, &e));
    SEMU_TEST_EQ_U64(context, 0u, a.rx);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_scheduler_advance(a.scheduler, 1u, &e));
    SEMU_TEST_EQ_U64(context, 10u, a.rx);
    SEMU_TEST_EQ_U64(context, 10000000u, a.rx_time);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, send(&a, "@VER\r\n", &e));
    SEMU_TEST_EQ_U64(context, 2u, a.state.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_scheduler_advance(a.scheduler, 9999999u, &e));
    SEMU_TEST_EQ_U64(context, 10u, a.rx);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_scheduler_advance(a.scheduler, 1u, &e));
    SEMU_TEST_EQ_U64(context, 20u, a.rx);
    SEMU_TEST_EQ_U64(context, 20000000u, a.rx_time);
    SEMU_TEST_ASSERT(context,
        memcmp(a.received, "$PSS0000\r\n$PSS0000\r\n", 20u) == 0);
    rewind(a.log);
    count = fread(log, 1u, sizeof(log)-1u, a.log); log[count] = '\0';
    SEMU_TEST_ASSERT(context, strstr(log, "trigger=gps-startup-status ordinal=1"));
    SEMU_TEST_ASSERT(context, strstr(log, "trigger=gps-version-status ordinal=2"));
    SEMU_TEST_ASSERT(context, strstr(log, "evidence=E-SAP-0071"));
    SEMU_TEST_EQ_U64(context, 0u, ftell(b.log));
    destroy(&a); destroy(&b);
}

static void test_sapporo_233_gps_atomic_refusals(semu_test_context *context)
{
    unsigned mode;
    for (mode = 0u; mode < 37u; ++mode) {
        fixture f;
        semu_error e;
        semu_snapshot_writer before, after;
        semu_cpu_state cpu;
        uint64_t hits, id, sequence, now;
        size_t events;
        long log_end;
        SEMU_TEST_ASSERT(context, init(&f, &e));
        if (mode == 0u) f.state.enabled = 0;
        if (mode == 1u) f.state.descriptor = NULL;
        if (mode == 2u) f.provider.logger = NULL;
        if (mode == 3u) ++f.cpu.r[4];
        if (mode == 4u) f.cpu.r[4] = 0x1017fffcu;
        if (mode == 5u) ++f.cpu.r[5];
        if (mode == 6u) ++f.cpu.r[6];
        if (mode >= 7u && mode <= 9u) SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_write(f.bus, 0x10039f60u, 4u,
                mode == 7u ? 0u : (mode == 8u ? 0x4001d000u : 0x10048d44u), &e));
        if (mode == 10u) SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_write(f.bus, 0x10048d44u, 4u, 0x1250e7u, &e));
        if (mode == 11u) SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_write(f.bus, 0x10048d4cu, 4u, 1u, &e));
        if (mode == 12u || mode == 13u) SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_write(f.bus, 0x10039fb2u, 2u,
                mode == 12u ? 0x0200u : 0x0704u, &e));
        if (mode == 14u) SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_write(f.bus, 0x1005b8f8u, 1u, 1u, &e));
        if (mode == 15u || mode == 16u) f.state.hits = mode - 14u;
        if (mode == 17u) f.scheduler->now_ns = UINT64_MAX;
        if (mode == 18u) f.scheduler->next_id = UINT64_MAX;
        if (mode == 19u) f.scheduler->next_sequence = UINT64_MAX;
        if (mode == 20u) SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_inject_rx_after(f.gps,
                (const uint8_t *)"x", 1u, 1u, &e));
        if (mode == 21u || mode == 36u || (mode >= 23u && mode <= 31u)) {
            SEMU_TEST_EQ_U64(context, SEMU_OK, startup(&f, &e));
            if (mode != 21u && mode != 36u) SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_scheduler_advance(f.scheduler, 10000000u, &e));
            if (mode == 25u) SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                send(&f, "@VER\r\n", &e));
            if (mode == 26u) f.scheduler->now_ns = UINT64_MAX;
            if (mode == 27u) f.scheduler->next_id = UINT64_MAX;
            if (mode == 28u) f.scheduler->next_sequence = UINT64_MAX;
            if (mode == 29u) f.state.enabled = 0;
            if (mode == 30u) f.provider.logger = NULL;
        }
        if (mode == 35u) {
            semu_bus_destroy(f.bus); f.bus = semu_bus_create(&e);
            SEMU_TEST_ASSERT(context, f.bus != NULL);
        }
        hits = f.state.hits; events = f.scheduler->count;
        id = f.scheduler->next_id; sequence = f.scheduler->next_sequence;
        now = f.scheduler->now_ns; log_end = ftell(f.log); cpu = f.cpu;
        semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_snapshot_write(f.gps, &before, &e));
        if (mode == 22u || mode == 25u || mode == 36u ||
            (mode >= 26u && mode <= 30u))
            SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, send(&f, "@VER\r\n", &e));
        else if (mode == 23u) SEMU_TEST_EQ_U64(context,
            SEMU_TRANSACTION_REFUSE, send(&f, "@GSR\r\n", &e));
        else if (mode == 24u || mode == 31u) SEMU_TEST_EQ_U64(context,
            SEMU_TRANSACTION_REFUSE, semu_sapporo_233_gps_exchange(&f.provider,
                mode == 24u ? (const uint8_t *)"@VER\r\n" : NULL, 5u, f.gps, &e));
        else if (mode >= 32u && mode <= 34u) SEMU_TEST_ASSERT(context,
            semu_sapporo_233_gps_startup(&f.provider,
                mode == 32u ? NULL : f.gps, mode == 33u ? NULL : f.bus,
                mode == 34u ? NULL : &f.cpu, &e) != SEMU_OK);
        else SEMU_TEST_ASSERT(context, startup(&f, &e) != SEMU_OK);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_snapshot_write(f.gps, &after, &e));
        SEMU_TEST_ASSERT(context, before.size == after.size &&
            memcmp(before.data, after.data, before.size) == 0);
        SEMU_TEST_ASSERT(context, memcmp(&cpu, &f.cpu, sizeof(cpu)) == 0);
        SEMU_TEST_EQ_U64(context, hits, f.state.hits);
        SEMU_TEST_EQ_U64(context, events, f.scheduler->count);
        SEMU_TEST_EQ_U64(context, id, f.scheduler->next_id);
        SEMU_TEST_EQ_U64(context, sequence, f.scheduler->next_sequence);
        SEMU_TEST_EQ_U64(context, now, f.scheduler->now_ns);
        SEMU_TEST_EQ_U64(context, log_end, ftell(f.log));
        if (f.state.enabled) {
            semu_error latched = f.provider.refusal;
            SEMU_TEST_ASSERT(context, latched.code != SEMU_OK);
            f.cpu.r[15] = 0x40000u;
            SEMU_TEST_EQ_U64(context, latched.code, startup(&f, &e));
            SEMU_TEST_ASSERT(context, strcmp(latched.text, e.text) == 0);
        }
        semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
        destroy(&f);
    }
}

static void test_sapporo_233_gps_reset_and_nontrigger(semu_test_context *context)
{
    fixture f;
    semu_error e;
    SEMU_TEST_ASSERT(context, init(&f, &e));
    f.cpu.r[15] = 0x40000u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, startup(&f, &e));
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(f.scheduler));
    SEMU_TEST_EQ_U64(context, 0u, f.state.hits);
    f.cpu.r[15] = SEMU_SAPPORO_233_GPS_PC;
    SEMU_TEST_EQ_U64(context, SEMU_OK, startup(&f, &e));
    semu_sapporo_cxd5610_reset(f.gps);
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(f.scheduler));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_scheduler_advance(f.scheduler, 10000000u, &e));
    SEMU_TEST_EQ_U64(context, 0u, f.rx);
    f.provider.refusal.code = SEMU_OK;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_layer_enable(&f.state,
        &semu_sapporo_233_gps_layer, "sapporo-2.33.16", &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, startup(&f, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_scheduler_advance(f.scheduler, 10000000u, &e));
    SEMU_TEST_EQ_U64(context, 10u, f.rx);
    destroy(&f);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_sapporo_233_gps_activation),
        SEMU_TEST_CASE(test_sapporo_233_gps_delayed_and_isolated),
        SEMU_TEST_CASE(test_sapporo_233_gps_atomic_refusals),
        SEMU_TEST_CASE(test_sapporo_233_gps_reset_and_nontrigger)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
