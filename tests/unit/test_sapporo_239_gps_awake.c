#include "test.h"
#include "semu/manifest.h"
#include "sapporo_devices_internal.h"
#include <stdio.h>
#include <string.h>

static void test_sapporo_239_gps_awake_activation(semu_test_context *context)
{
    semu_profile profile;
    semu_error error;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_profile_load("profiles/sapporo/2.39.20/profile.semu", &profile, &error));
    SEMU_TEST_EQ_U64(context, 5u, profile.layer_count);
    SEMU_TEST_ASSERT(context, strcmp(profile.layers[3], "sapporo-2.39-gps-awake") == 0);
    const semu_layer_descriptor *d = &semu_sapporo_239_gps_awake_layer;
    const char *hashes[3];
    semu_layer_state state;
    unsigned i;
    for (i = 0u; i < 3u; ++i) hashes[i] = d->component_hashes[i];
    SEMU_TEST_EQ_U64(context, 4u, d->maximum_hits);
    SEMU_TEST_EQ_U64(context, 1u, d->intervention_count);
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
    semu_sapporo_devices d;
    semu_layer_state layers[4];
    semu_scheduler *scheduler;
    semu_bus *bus;
    semu_cpu_state cpu;
    semu_logger logger;
    FILE *log;
    unsigned signals, highs;
} fixture;

static void signal_level(void *context, unsigned channel, int level)
{
    fixture *f = context;
    (void)channel;
    ++f->signals;
    if (level) ++f->highs;
}

static void receive(void *context, uint8_t value, uint64_t now)
{
    (void)context; (void)value; (void)now;
}

static int init_layer(fixture *f, const semu_layer_descriptor *awake, semu_error *e)
{
    const semu_layer_descriptor *descriptors[] = {&semu_sapporo_239_gps_layer,
        &semu_sapporo_239_gps_reopen_layer, awake};
    memset(f, 0, sizeof(*f));
    f->log = tmpfile(); f->scheduler = semu_scheduler_create(e);
    f->bus = semu_bus_create(e); f->d.scheduler = f->scheduler;
    f->d.gps = semu_sapporo_cxd5610_create(f->scheduler, signal_level, f,
        receive, f, NULL, NULL, e);
    f->d.ohr2_profile_239 = 1;
    if (!f->log || !f->scheduler || !f->bus || !f->d.gps) return 0;
    semu_log_init(&f->logger, f->log, SEMU_LOG_DEBUG);
    for (unsigned i = 0u; i < 3u; ++i)
        if (semu_layer_enable(&f->layers[i], descriptors[i], "sapporo-2.39.20", e) != SEMU_OK)
            return 0;
    if (semu_sapporo_devices_bind_gps_layers(&f->d, f->layers, 3u, &f->logger, e) != SEMU_OK ||
        semu_bus_map_ram(f->bus, "sram", 0x10000000u, 0x180000u, e) != SEMU_OK ||
        semu_bus_map_ram(f->bus, "synthetic GPIO config", 0x40010060u, 4u, e) != SEMU_OK)
        return 0;
    f->layers[0].hits = f->layers[1].hits = 2u;
    f->d.gps_239_context.interventions[0].hits = f->d.gps_239_context.interventions[1].hits = 1u;
    f->d.gps_reopen_context.interventions[0].hits = f->d.gps_reopen_context.interventions[1].hits = 1u;
    f->cpu.r[15] = SEMU_SAPPORO_239_GPS_AWAKE_PC;
    f->cpu.r[0] = 1u; f->cpu.r[2] = 12u; f->cpu.r[4] = 0x100588a2u;
    f->cpu.r[8] = 0x100366d8u; f->cpu.r[5] = 0x10036944u;
    f->cpu.r[6] = 0x100369ecu; f->cpu.r[7] = 0x1003674du;
    return semu_bus_write(f->bus, 0x1003694au, 2u, 0x0a0cu, e) == SEMU_OK &&
        semu_bus_write(f->bus, 0x100588a2u, 1u, 1u, e) == SEMU_OK &&
        semu_bus_write(f->bus, 0x40010060u, 4u, 0x93u, e) == SEMU_OK;
}

static int init(fixture *f, semu_error *e)
{
    return init_layer(f, &semu_sapporo_239_gps_awake_layer, e);
}

static void destroy(fixture *f)
{
    semu_sapporo_cxd5610_destroy(f->d.gps); semu_bus_destroy(f->bus);
    semu_scheduler_destroy(f->scheduler); if (f->log) (void)fclose(f->log);
}

static semu_status poll_awake(fixture *f, semu_error *e)
{
    return semu_sapporo_239_gps_awake_poll(&f->d.gps_awake_context,
        f->d.gps, f->bus, &f->cpu, e);
}

static void test_sapporo_239_gps_awake_four_pulses(semu_test_context *context)
{
    fixture f, other;
    semu_error e;
    semu_cpu_state cpu;
    uint32_t awake;
    SEMU_TEST_ASSERT(context, init(&f, &e));
    f.cpu.r[1] = UINT32_MAX; cpu = f.cpu; /* Native queue return is not a predicate. */
    for (unsigned i = 0u; i < 4u; ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_OK, poll_awake(&f, &e));
        SEMU_TEST_EQ_U64(context, i + 1u, f.layers[2].hits);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(f.scheduler, 99999999u, &e));
        SEMU_TEST_EQ_U64(context, i, f.highs);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(f.scheduler, 1u, &e));
        SEMU_TEST_EQ_U64(context, i + 1u, f.highs);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(f.scheduler, 1000000u, &e));
        SEMU_TEST_EQ_U64(context, 0u, f.scheduler->count);
    }
    SEMU_TEST_ASSERT(context, init(&other, &e));
    SEMU_TEST_EQ_U64(context, 4u, f.d.gps_awake_context.intervention.hits);
    SEMU_TEST_EQ_U64(context, 0u, other.layers[2].hits);
    SEMU_TEST_ASSERT(context, memcmp(&cpu, &f.cpu, sizeof(cpu)) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(f.bus, 0x100588a2u, 1u, &awake, &e));
    SEMU_TEST_EQ_U64(context, 1u, awake); /* Only native firmware may clear it. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_devices_bind_gps_layers(&f.d, f.layers, 3u, &f.logger, &e));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, poll_awake(&f, &e));
    SEMU_TEST_EQ_U64(context, 4u, f.layers[2].hits);
    destroy(&f); destroy(&other);
}

static void refusals(semu_test_context *context, const semu_layer_descriptor *definition)
{
    for (unsigned mode = 0u; mode < 29u; ++mode) {
        fixture f; semu_error e; semu_snapshot_writer before, after;
        uint64_t id, seq, hits; size_t count; unsigned signals; long log_end;
        SEMU_TEST_ASSERT(context, init_layer(&f, definition, &e));
        if (mode == 0u) f.layers[2].enabled = 0;
        if (mode <= 6u && mode >= 1u) {
            static const unsigned regs[] = {0u, 2u, 4u, 5u, 6u, 7u};
            ++f.cpu.r[regs[mode - 1u]];
        }
        if (mode == 7u) ++f.cpu.r[8];
        if (mode == 8u) f.cpu.r[8] = 0x1017fd00u;
        if (mode >= 9u && mode <= 12u) {
            static const uint32_t addresses[] = {0x1003694au, 0x1003694bu, 0x100588a2u, 0x100588a4u};
            (void)semu_bus_write(f.bus, addresses[mode - 9u], 1u, mode == 12u ? 1u : 0u, &e);
        }
        if (mode == 13u) (void)semu_bus_write(f.bus, 0x40010060u, 4u, 0u, &e);
        if (mode == 14u) f.layers[0].enabled = 0;
        if (mode == 15u) f.layers[1].enabled = 0;
        if (mode == 16u) f.layers[0].hits = 1u;
        if (mode == 17u) f.layers[1].hits = 1u;
        if (mode == 18u) f.scheduler->now_ns = UINT64_MAX - 100000000u;
        if (mode == 19u) f.scheduler->next_id = UINT64_MAX;
        if (mode == 20u) f.scheduler->next_sequence = UINT64_MAX;
        if (mode == 21u) SEMU_TEST_EQ_U64(context, SEMU_OK, poll_awake(&f, &e));
        if (mode == 22u) f.d.gps_awake_context.descriptor.maximum_hits = 0u;
        if (mode == 23u) f.d.gps_awake_context.intervention.max_hits = 0u;
        if (mode == 24u) f.layers[2].hits = 1u;
        if (mode == 25u) f.d.gps_awake_context.intervention.hits = 1u;
        if (mode == 26u) f.layers[2].hits = f.d.gps_awake_context.intervention.hits = definition->maximum_hits;
        if (mode == 27u) f.d.gps_awake_context.startup = NULL;
        if (mode == 28u) f.d.gps_awake_context.reopen = NULL;
        id = f.scheduler->next_id; seq = f.scheduler->next_sequence;
        hits = f.layers[2].hits; count = f.scheduler->count;
        signals = f.signals; log_end = ftell(f.log);
        semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_cxd5610_snapshot_write(f.d.gps, &before, &e));
        SEMU_TEST_ASSERT(context, poll_awake(&f, &e) != SEMU_OK);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_cxd5610_snapshot_write(f.d.gps, &after, &e));
        SEMU_TEST_ASSERT(context, before.size == after.size && !memcmp(before.data, after.data, before.size));
        SEMU_TEST_EQ_U64(context, id, f.scheduler->next_id);
        SEMU_TEST_EQ_U64(context, seq, f.scheduler->next_sequence);
        SEMU_TEST_EQ_U64(context, count, f.scheduler->count);
        SEMU_TEST_EQ_U64(context, hits, f.layers[2].hits);
        SEMU_TEST_EQ_U64(context, signals, f.signals);
        SEMU_TEST_EQ_U64(context, log_end, ftell(f.log));
        semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after); destroy(&f);
    }
}

static void test_sapporo_239_gps_awake_refusals(semu_test_context *context)
{
    refusals(context, &semu_sapporo_239_gps_awake_layer);
}

static void test_sapporo_239_gps_awake_binding(semu_test_context *context)
{
    for (unsigned mode = 0u; mode < 10u; ++mode) {
        fixture f; semu_sapporo_devices before; semu_layer_state layers[4];
        semu_error e; size_t count = 3u;
        SEMU_TEST_ASSERT(context, init(&f, &e));
        if (mode == 0u) { f.layers[3] = f.layers[2]; count = 4u; }
        if (mode == 1u) f.d.gps_awake_context.state = &f.layers[3];
        if (mode == 2u) f.d.gps_awake_context.descriptor.interventions = NULL;
        if (mode == 3u) f.d.gps_awake_context.descriptor.intervention_count = 2u;
        if (mode == 4u) f.layers[0].descriptor = NULL;
        if (mode == 5u) f.layers[1].descriptor = NULL;
        if (mode == 6u) f.layers[2].enabled = 0;
        if (mode == 7u) f.layers[2].hits = 1u;
        if (mode == 8u) count = 2u; /* Cannot silently drop an existing owner. */
        if (mode == 9u) f.d.ohr2_profile_239 = 0;
        before = f.d; memcpy(layers, f.layers, sizeof(layers));
        SEMU_TEST_ASSERT(context, semu_sapporo_devices_bind_gps_layers(
            &f.d, f.layers, count, &f.logger, &e) != SEMU_OK);
        SEMU_TEST_ASSERT(context, !memcmp(&before, &f.d, sizeof(before)) &&
            !memcmp(layers, f.layers, sizeof(layers)));
        destroy(&f);
    }
}

static void test_sapporo_239_gps_awake_uart_provider(semu_test_context *context)
{
    fixture f; semu_error e;
    SEMU_TEST_ASSERT(context, init(&f, &e));
    f.layers[0].hits = 1u; f.d.gps_239_context.interventions[1].hits = 0u;
    f.layers[1].hits = 0u;
    f.d.gps_reopen_context.interventions[0].hits = f.d.gps_reopen_context.interventions[1].hits = 0u;
    semu_serial_endpoint ep = semu_sapporo_cxd5610_endpoint(f.d.gps);
    semu_serial_transaction tx = {0u, 0u, (const uint8_t *)"@VER\r\n", 6u, NULL, 0u};
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, ep.transfer(ep.context, &tx, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(f.scheduler, 10000000u, &e));
    f.layers[1].hits = f.d.gps_reopen_context.interventions[0].hits = 1u;
    tx.tx = (const uint8_t *)"@GSR\r\n";
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, ep.transfer(ep.context, &tx, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(f.scheduler, 10000000u, &e));
    SEMU_TEST_EQ_U64(context, 2u, f.layers[0].hits);
    SEMU_TEST_EQ_U64(context, 2u, f.layers[1].hits);
    SEMU_TEST_EQ_U64(context, 0u, f.layers[2].hits);
    tx.tx = (const uint8_t *)"@GSTP\r\n"; tx.tx_size = 7u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, ep.transfer(ep.context, &tx, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, poll_awake(&f, &e));
    destroy(&f);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_sapporo_239_gps_awake_activation),
        SEMU_TEST_CASE(test_sapporo_239_gps_awake_four_pulses),
        SEMU_TEST_CASE(test_sapporo_239_gps_awake_refusals),
        SEMU_TEST_CASE(test_sapporo_239_gps_awake_binding),
        SEMU_TEST_CASE(test_sapporo_239_gps_awake_uart_provider)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
