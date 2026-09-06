#include "test.h"
#include "semu/manifest.h"
#include "sapporo_239_gps.h"
#include <stdio.h>
#include <string.h>

static void test_sapporo_239_gps_activation(semu_test_context *context)
{
    semu_profile profile;
    semu_error error;
    semu_layer_state state;
    const semu_layer_descriptor *d = &semu_sapporo_239_gps_layer;
    const char *hashes[3];
    unsigned i;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_profile_load("profiles/sapporo/2.39.20/profile.semu", &profile, &error));
    SEMU_TEST_EQ_U64(context, 4u, profile.layer_count);
    SEMU_TEST_ASSERT(context, strcmp(profile.layers[1], d->id) == 0);
    SEMU_TEST_EQ_U64(context, 2u, d->maximum_hits);
    SEMU_TEST_EQ_U64(context, 2u, d->intervention_count);
    for (i = 0u; i < 3u; ++i) hashes[i] = d->component_hashes[i];
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_layer_enable_checked(&state, d, profile.id, hashes, 3u, &error));
    for (i = 0u; i < 3u; ++i) {
        hashes[i] = "wrong";
        SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
            semu_layer_enable_checked(&state, d, profile.id, hashes, 3u, &error));
        SEMU_TEST_ASSERT(context, !state.enabled);
        hashes[i] = d->component_hashes[i];
    }
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
        semu_layer_enable_checked(&state, d, "sapporo-2.22.60", hashes, 3u, &error));
}

typedef struct fixture {
    semu_sapporo_239_gps_context provider;
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
    if (semu_layer_enable(&f->state, &semu_sapporo_239_gps_layer,
            "sapporo-2.39.20", e) != SEMU_OK ||
        semu_sapporo_239_gps_bind(&f->provider, &f->state, &f->logger, e) != SEMU_OK ||
        semu_bus_map_ram(f->bus, "sram", 0x10000000u, 0x180000u, e) != SEMU_OK)
        return 0;
    semu_sapporo_cxd5610_set_exchange(f->gps, semu_sapporo_239_gps_exchange, &f->provider);
    f->cpu.r[15] = SEMU_SAPPORO_239_GPS_PC;
    f->cpu.r[4] = 0x100366d8u; f->cpu.r[5] = 0x10036948u; f->cpu.r[6] = 0x10036a10u;
    return semu_bus_write(f->bus, 0x100368f8u, 4u, 0x100472e8u, e) == SEMU_OK &&
        semu_bus_write(f->bus, 0x100472ecu, 4u, 0x12890fu, e) == SEMU_OK &&
        semu_bus_write(f->bus, 0x1003694au, 2u, 0x0204u, e) == SEMU_OK;
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
    return semu_sapporo_239_gps_startup(&f->provider, f->gps, f->bus, &f->cpu, e);
}

static semu_transaction_result send(fixture *f, const char *text, semu_error *e)
{
    semu_serial_endpoint ep = semu_sapporo_cxd5610_endpoint(f->gps);
    semu_serial_transaction tx = {0u, 0u, (const uint8_t *)text, strlen(text), NULL, 0u};
    return ep.transfer(ep.context, &tx, e);
}

static void test_sapporo_239_gps_delayed_and_isolated(semu_test_context *context)
{
    fixture a, b;
    semu_error e;
    semu_cpu_state cpu;
    SEMU_TEST_ASSERT(context, init(&a, &e));
    cpu = a.cpu; /* In particular R0=0 at the pre-MOVS boundary. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, startup(&a, &e));
    SEMU_TEST_ASSERT(context, memcmp(&cpu, &a.cpu, sizeof(cpu)) == 0);
    SEMU_TEST_ASSERT(context, init(&b, &e));
    SEMU_TEST_EQ_U64(context, 1u, a.state.hits);
    SEMU_TEST_EQ_U64(context, 1u, a.provider.interventions[0].hits);
    SEMU_TEST_EQ_U64(context, 0u, b.state.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(a.scheduler, 9999999u, &e));
    SEMU_TEST_EQ_U64(context, 0u, a.rx);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(a.scheduler, 1u, &e));
    SEMU_TEST_EQ_U64(context, 10u, a.rx);
    SEMU_TEST_EQ_U64(context, 10000000u, a.rx_time);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, send(&a, "@VER\r\n", &e));
    SEMU_TEST_EQ_U64(context, 2u, a.state.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(a.scheduler, 9999999u, &e));
    SEMU_TEST_EQ_U64(context, 10u, a.rx);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(a.scheduler, 1u, &e));
    SEMU_TEST_EQ_U64(context, 20u, a.rx);
    SEMU_TEST_EQ_U64(context, 20000000u, a.rx_time);
    SEMU_TEST_ASSERT(context, memcmp(a.received, "$PSS0000\r\n$PSS0000\r\n", 20u) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_gps_bind(&a.provider, &a.state, &a.logger, &e));
    SEMU_TEST_EQ_U64(context, 2u, a.state.hits); /* Rebinding never rearms. */
    destroy(&a); destroy(&b);
}

static void test_sapporo_239_gps_atomic_refusals(semu_test_context *context)
{
    unsigned mode;
    for (mode = 0u; mode < 31u; ++mode) {
        fixture f;
        semu_error e;
        semu_snapshot_writer before, after;
        uint64_t hits, first, second, next_id, sequence, now;
        size_t events;
        long log_end;
        SEMU_TEST_ASSERT(context, init(&f, &e));
        if (mode == 0u) f.state.enabled = 0;
        if (mode == 1u) f.cpu.r[4] = 0x100366d9u;
        if (mode == 2u) f.cpu.r[4] = 0x1017ff00u;
        if (mode == 3u) ++f.cpu.r[5];
        if (mode == 4u) ++f.cpu.r[6];
        if (mode == 5u) (void)semu_bus_write(f.bus, 0x100368f8u, 4u, 0x4001d000u, &e);
        if (mode == 6u) (void)semu_bus_write(f.bus, 0x100368f8u, 4u, 0x1017fffcu, &e);
        if (mode == 7u) (void)semu_bus_write(f.bus, 0x100472ecu, 4u, 0u, &e);
        if (mode == 8u) (void)semu_bus_write(f.bus, 0x1003694au, 2u, 0x0704u, &e);
        if (mode == 9u) (void)semu_bus_write(f.bus, 0x100588a4u, 1u, 1u, &e);
        if (mode == 10u) f.provider.descriptor.maximum_hits = 0u;
        if (mode == 11u) f.provider.interventions[0].max_hits = 0u;
        if (mode >= 12u && mode <= 16u) {
            SEMU_TEST_EQ_U64(context, SEMU_OK, startup(&f, &e));
            if (mode >= 13u) SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_scheduler_advance(f.scheduler, 10000000u, &e));
            if (mode == 16u) SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                send(&f, "@VER\r\n", &e));
        }
        if (mode == 17u) f.scheduler->now_ns = UINT64_MAX;
        if (mode == 18u) f.scheduler->next_id = UINT64_MAX;
        if (mode == 19u) f.scheduler->next_sequence = UINT64_MAX;
        if (mode == 20u) SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_inject_rx_after(f.gps, (const uint8_t *)"x", 1u, 1u, &e));
        if (mode >= 22u && mode <= 27u) {
            SEMU_TEST_EQ_U64(context, SEMU_OK, startup(&f, &e));
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(f.scheduler, 10000000u, &e));
            if (mode == 22u) f.scheduler->now_ns = UINT64_MAX;
            if (mode == 23u) f.scheduler->next_id = UINT64_MAX;
            if (mode == 24u) f.scheduler->next_sequence = UINT64_MAX;
            if (mode == 25u) f.provider.interventions[1].max_hits = 0u;
            if (mode == 26u) f.provider.descriptor.maximum_hits = 1u;
            if (mode == 27u) f.state.enabled = 0;
        }
        if (mode == 28u) (void)semu_bus_write(f.bus, 0x100368f8u, 4u, 0x100472e9u, &e);
        if (mode == 29u) (void)semu_bus_write(f.bus, 0x100472f4u, 4u, 1u, &e);
        if (mode == 30u) (void)semu_bus_write(f.bus, 0x1003694au, 1u, 0u, &e);
        hits = f.state.hits; first = f.provider.interventions[0].hits;
        second = f.provider.interventions[1].hits;
        events = f.scheduler->count; next_id = f.scheduler->next_id;
        sequence = f.scheduler->next_sequence; now = f.scheduler->now_ns;
        log_end = ftell(f.log);
        semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_snapshot_write(f.gps, &before, &e));
        if (mode == 12u || mode == 16u || (mode >= 21u && mode <= 27u))
            SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, send(&f, "@VER\r\n", &e));
        else if (mode == 14u || mode == 15u)
            SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                send(&f, mode == 14u ? "@SLP\r\n" : "@VERx\r\n", &e));
        else SEMU_TEST_ASSERT(context, startup(&f, &e) != SEMU_OK);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_snapshot_write(f.gps, &after, &e));
        SEMU_TEST_ASSERT(context, before.size == after.size &&
            memcmp(before.data, after.data, before.size) == 0);
        SEMU_TEST_EQ_U64(context, hits, f.state.hits);
        SEMU_TEST_EQ_U64(context, first, f.provider.interventions[0].hits);
        SEMU_TEST_EQ_U64(context, second, f.provider.interventions[1].hits);
        SEMU_TEST_EQ_U64(context, events, f.scheduler->count);
        SEMU_TEST_EQ_U64(context, next_id, f.scheduler->next_id);
        SEMU_TEST_EQ_U64(context, sequence, f.scheduler->next_sequence);
        SEMU_TEST_EQ_U64(context, now, f.scheduler->now_ns);
        SEMU_TEST_EQ_U64(context, log_end, ftell(f.log));
        semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
        destroy(&f);
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_sapporo_239_gps_activation),
        SEMU_TEST_CASE(test_sapporo_239_gps_delayed_and_isolated),
        SEMU_TEST_CASE(test_sapporo_239_gps_atomic_refusals)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
