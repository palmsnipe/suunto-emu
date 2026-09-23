#include "test.h"
#include "semu/manifest.h"
#include "sapporo_235_gps_reopen.h"
#include "../../src/boards/machine_internal.h"
#include <stdio.h>
#include <string.h>

static void test_sapporo_235_gps_reopen_activation(semu_test_context *context)
{
    semu_profile profile;
    semu_error error;
    semu_layer_state state;
    const semu_layer_descriptor *d = &semu_sapporo_235_gps_reopen_layer;
    const char *hashes[3];
    unsigned i;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_profile_load("profiles/sapporo/2.35.34/profile.semu", &profile, &error));
    SEMU_TEST_EQ_U64(context, 2u, d->maximum_hits);
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
    semu_sapporo_235_gps_reopen_context provider;
    semu_sapporo_235_gps_context initial;
    semu_layer_state initial_state;
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
    if (semu_layer_enable(&f->state, &semu_sapporo_235_gps_reopen_layer,
            "sapporo-2.35.34", e) != SEMU_OK ||
        semu_bus_map_ram(f->bus, "sram", 0x10000000u, 0x180000u, e) != SEMU_OK)
        return 0;
    if (semu_layer_enable(&f->initial_state, &semu_sapporo_235_gps_layer,
            "sapporo-2.35.34", e) != SEMU_OK) return 0;
    f->initial_state.hits = 2u; /* Synthetic fixture: initial exchange completed. */
    f->initial.state = &f->initial_state; f->initial.logger = &f->logger;
    f->provider.startup = &f->initial;
    f->provider.state = &f->state; f->provider.logger = &f->logger;
    semu_sapporo_cxd5610_set_exchange(f->gps, semu_sapporo_235_gps_reopen_exchange, &f->provider);
    f->cpu.r[15] = SEMU_SAPPORO_235_GPS_REOPEN_PC;
    f->cpu.r[4] = 0x100364acu; f->cpu.r[5] = 0x10036520u; f->cpu.r[6] = 0x1003671cu;
    return semu_bus_write(f->bus, 0x100366ccu, 4u, 0x100456a0u, e) == SEMU_OK &&
        semu_bus_write(f->bus, 0x100456a4u, 4u, 0x1250e7u, e) == SEMU_OK &&
        semu_bus_write(f->bus, 0x1003671eu, 2u, 0x0704u, e) == SEMU_OK &&
        semu_bus_write(f->bus, 0x10036520u, 1u, 15u, e) == SEMU_OK &&
        semu_bus_write(f->bus, 0x1003652bu, 1u, 2u, e) == SEMU_OK &&
        semu_bus_write(f->bus, 0x100589ffu, 1u, 1u, e) == SEMU_OK;
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
    return semu_sapporo_235_gps_reopen_start(&f->provider, f->gps, f->bus, &f->cpu, e);
}

static semu_transaction_result send(fixture *f, const char *text, semu_error *e)
{
    semu_serial_endpoint ep = semu_sapporo_cxd5610_endpoint(f->gps);
    semu_serial_transaction tx = {0u, 0u, (const uint8_t *)text, strlen(text), NULL, 0u};
    return ep.transfer(ep.context, &tx, e);
}

static void test_sapporo_235_gps_reopen_delayed_and_isolated(semu_test_context *context)
{
    fixture a, b;
    semu_error e;
    semu_cpu_state cpu;
    SEMU_TEST_ASSERT(context, init(&a, &e));
    cpu = a.cpu; /* Native registers must remain unchanged. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, startup(&a, &e));
    SEMU_TEST_ASSERT(context, memcmp(&cpu, &a.cpu, sizeof(cpu)) == 0);
    SEMU_TEST_ASSERT(context, init(&b, &e));
    SEMU_TEST_EQ_U64(context, 1u, a.state.hits);
    SEMU_TEST_EQ_U64(context, 0u, b.state.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(a.scheduler, 9999999u, &e));
    SEMU_TEST_EQ_U64(context, 0u, a.rx);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(a.scheduler, 1u, &e));
    SEMU_TEST_EQ_U64(context, 10u, a.rx);
    SEMU_TEST_EQ_U64(context, 10000000u, a.rx_time);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, send(&a, "@GSR\r\n", &e));
    SEMU_TEST_EQ_U64(context, 2u, a.state.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(a.scheduler, 9999999u, &e));
    SEMU_TEST_EQ_U64(context, 10u, a.rx);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(a.scheduler, 1u, &e));
    SEMU_TEST_EQ_U64(context, 20u, a.rx);
    SEMU_TEST_EQ_U64(context, 20000000u, a.rx_time);
    SEMU_TEST_ASSERT(context, memcmp(a.received, "$PSS0000\r\n$PSS0000\r\n", 20u) == 0);
    {
        char log[1024];
        size_t count;
        rewind(a.log);
        count = fread(log, 1u, sizeof(log)-1u, a.log); log[count] = '\0';
        SEMU_TEST_ASSERT(context, strstr(log, "trigger=gps-reopen-status ordinal=1") != NULL);
        SEMU_TEST_ASSERT(context, strstr(log, "trigger=gps-gsr-status ordinal=2") != NULL);
        SEMU_TEST_ASSERT(context, strstr(log, "evidence=E-SAP-0047") != NULL);
        SEMU_TEST_EQ_U64(context, 0u, ftell(b.log));
    }
    destroy(&a); destroy(&b);
}

static void test_sapporo_235_gps_reopen_atomic_refusals(semu_test_context *context)
{
    unsigned mode;
    for (mode = 0u; mode < 43u; ++mode) {
        fixture f;
        semu_error e;
        semu_snapshot_writer before, after;
        uint64_t hits, next_id, sequence, now;
        size_t events;
        long log_end;
        SEMU_TEST_ASSERT(context, init(&f, &e));
        if (mode == 0u) f.state.enabled = 0;
        if (mode == 1u) f.cpu.r[4] = 0x100364adu;
        if (mode == 2u) f.cpu.r[4] = 0x1017ff00u;
        if (mode == 3u) ++f.cpu.r[5];
        if (mode == 4u) ++f.cpu.r[6];
        if (mode == 5u) (void)semu_bus_write(f.bus, 0x100366ccu, 4u, 0x4001d000u, &e);
        if (mode == 6u) (void)semu_bus_write(f.bus, 0x100366ccu, 4u, 0x1017fffcu, &e);
        if (mode == 7u) (void)semu_bus_write(f.bus, 0x100456a4u, 4u, 0u, &e);
        if (mode == 8u) (void)semu_bus_write(f.bus, 0x1003671eu, 2u, 0x0204u, &e);
        if (mode == 9u) (void)semu_bus_write(f.bus, 0x10058a00u, 1u, 1u, &e);
        if (mode == 10u) f.state.hits = 2u;
        if (mode == 11u) f.state.descriptor = NULL;
        if (mode >= 12u && mode <= 16u) {
            SEMU_TEST_EQ_U64(context, SEMU_OK, startup(&f, &e));
            if (mode >= 13u) SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_scheduler_advance(f.scheduler, 10000000u, &e));
            if (mode == 16u) SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                send(&f, "@GSR\r\n", &e));
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
            if (mode == 25u) f.state.hits = 0u;
            if (mode == 26u) f.state.hits = 2u;
            if (mode == 27u) f.state.enabled = 0;
        }
        if (mode == 28u) (void)semu_bus_write(f.bus, 0x100366ccu, 4u, 0x100456a1u, &e);
        if (mode == 29u) (void)semu_bus_write(f.bus, 0x100456acu, 4u, 1u, &e);
        if (mode == 30u) (void)semu_bus_write(f.bus, 0x1003671eu, 1u, 0u, &e);
        if (mode == 31u) f.initial_state.hits = 1u;
        if (mode == 32u) f.initial_state.enabled = 0;
        if (mode == 33u) f.initial_state.descriptor = NULL;
        if (mode == 34u) f.initial.logger = NULL;
        if (mode == 35u) f.initial.refusal.code = SEMU_ERR_STATE;
        if (mode == 36u) f.provider.startup = NULL;
        if (mode == 37u) (void)semu_bus_write(f.bus, 0x10036520u, 1u, 0u, &e);
        if (mode == 38u) (void)semu_bus_write(f.bus, 0x10036527u, 1u, 1u, &e);
        if (mode == 39u) (void)semu_bus_write(f.bus, 0x1003652bu, 1u, 0u, &e);
        if (mode == 40u) (void)semu_bus_write(f.bus, 0x100589ffu, 1u, 0u, &e);
        if (mode == 41u || mode == 42u) {
            SEMU_TEST_EQ_U64(context, SEMU_OK, startup(&f, &e));
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(f.scheduler, 10000000u, &e));
            if (mode == 41u) f.initial_state.hits = 1u;
        }
        hits = f.state.hits;
        events = f.scheduler->count; next_id = f.scheduler->next_id;
        sequence = f.scheduler->next_sequence; now = f.scheduler->now_ns;
        log_end = ftell(f.log);
        semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_snapshot_write(f.gps, &before, &e));
        if (mode == 12u || mode == 16u || (mode >= 21u && mode <= 27u) || mode == 41u)
            SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, send(&f, "@GSR\r\n", &e));
        else if (mode == 14u || mode == 15u || mode == 42u)
            SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                send(&f, mode == 14u ? "@SLP\r\n" : mode == 42u ? "@GSTP\r\n" : "@GSRx\r\n", &e));
        else SEMU_TEST_ASSERT(context, startup(&f, &e) != SEMU_OK);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_snapshot_write(f.gps, &after, &e));
        SEMU_TEST_ASSERT(context, before.size == after.size &&
            memcmp(before.data, after.data, before.size) == 0);
        SEMU_TEST_EQ_U64(context, hits, f.state.hits);
        SEMU_TEST_EQ_U64(context, events, f.scheduler->count);
        SEMU_TEST_EQ_U64(context, next_id, f.scheduler->next_id);
        SEMU_TEST_EQ_U64(context, sequence, f.scheduler->next_sequence);
        SEMU_TEST_EQ_U64(context, now, f.scheduler->now_ns);
        SEMU_TEST_EQ_U64(context, log_end, ftell(f.log));
        if (f.state.enabled) {
            SEMU_TEST_ASSERT(context, f.provider.refusal.code != SEMU_OK);
            f.cpu.r[15] = 0x40000u;
            SEMU_TEST_ASSERT(context, startup(&f, &e) != SEMU_OK);
        }
        semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
        destroy(&f);
    }
}

static void test_sapporo_235_gps_reopen_binding_and_reset(semu_test_context *context)
{
    unsigned reverse;
    for (reverse = 0u; reverse < 2u; ++reverse) {
        fixture f;
        semu_machine m;
        semu_error e;
        semu_run_limits limits = {1u, 1u};
        const semu_apollo4_uart_endpoint *ep;
        semu_cpu_state *cpu;
        unsigned i, first = reverse, second = 1u-reverse;
        const char request[] = "@GSR\r\n", unknown[] = "@GSTP\r\n";
        SEMU_TEST_ASSERT(context, init(&f, &e));
        memset(&m, 0, sizeof(m));
        m.bus = f.bus; m.scheduler = f.scheduler; m.logger = &f.logger;
        m.layers[first] = f.initial_state; m.layers[first].hits = 0u;
        m.layers[second] = f.state; m.layer_count = 2u;
        m.cpu = semu_cpu_create(m.bus, m.scheduler, &e);
        m.soc = semu_apollo4_create(m.bus, &e);
        m.devices = semu_sapporo_devices_create(m.scheduler, NULL, &e);
        SEMU_TEST_ASSERT(context, m.cpu && m.soc && m.devices);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_apollo4_init(m.soc, m.scheduler, NULL, NULL, &e));
        SEMU_TEST_ASSERT(context, semu_sapporo_devices_bind_gps_layers(
            m.devices, m.layers, 2u, m.logger, &e) != SEMU_OK);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_devices_select_profile(
            m.devices, "sapporo-2.35.34", &e));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_devices_attach(m.devices, m.soc, &e));
        /* All invalid sets must leave the fresh owner available for binding. */
        SEMU_TEST_ASSERT(context, semu_sapporo_devices_bind_gps_layers(
            m.devices, &m.layers[second], 1u, m.logger, &e) != SEMU_OK);
        m.layers[2] = m.layers[second];
        SEMU_TEST_ASSERT(context, semu_sapporo_devices_bind_gps_layers(
            m.devices, m.layers, 3u, m.logger, &e) != SEMU_OK);
        m.layers[second].enabled = 0;
        SEMU_TEST_ASSERT(context, semu_sapporo_devices_bind_gps_layers(
            m.devices, m.layers, 2u, m.logger, &e) != SEMU_OK);
        m.layers[second].enabled = 1; m.layers[second].hits = 1u;
        SEMU_TEST_ASSERT(context, semu_sapporo_devices_bind_gps_layers(
            m.devices, m.layers, 2u, m.logger, &e) != SEMU_OK);
        m.layers[second].hits = 0u;
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_devices_bind_gps_layers(
            m.devices, m.layers, 2u, m.logger, &e));
        SEMU_TEST_ASSERT(context, semu_sapporo_devices_bind_gps_layers(
            m.devices, m.layers, 2u, m.logger, &e) != SEMU_OK);
        m.layers[first].hits = 2u; /* Prepared native initial-completion fixture. */
        cpu = semu_cpu_get_state_mutable(m.cpu); *cpu = f.cpu;
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_devices_apply_compat_hook(
            m.devices, m.bus, cpu, &m.layers[second], m.logger, &e));
        SEMU_TEST_EQ_U64(context, 1u, m.layers[second].hits);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_scheduler_advance(m.scheduler, 10000000u, &e));
        ep = semu_sapporo_devices_uart_endpoint(m.devices);
        SEMU_TEST_ASSERT(context, ep != NULL);
        for (i = 0u; i < sizeof(request)-1u; ++i)
            SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                ep->transmit(ep->context, (uint8_t)request[i], &e));
        SEMU_TEST_EQ_U64(context, 2u, m.layers[second].hits);
        SEMU_TEST_EQ_U64(context, 2u, m.layers[first].hits);
        for (i = 0u; i < sizeof(unknown)-2u; ++i)
            SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                ep->transmit(ep->context, (uint8_t)unknown[i], &e));
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
            ep->transmit(ep->context, '\n', &e));
        cpu->r[15] = 0x40000u;
        SEMU_TEST_EQ_U64(context, SEMU_STOP_COMPAT_REFUSED,
            semu_machine_run(&m, &limits, &e));
        SEMU_TEST_EQ_U64(context, 0u, cpu->instructions);
        SEMU_TEST_EQ_U64(context, 0u, m.reset_request_count);
        semu_sapporo_devices_reset(m.devices);
        SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(m.scheduler));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_layer_enable(&m.layers[first],
            &semu_sapporo_235_gps_layer, "sapporo-2.35.34", &e));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_layer_enable(&m.layers[second],
            &semu_sapporo_235_gps_reopen_layer, "sapporo-2.35.34", &e));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_devices_bind_gps_layers(
            m.devices, m.layers, 2u, m.logger, &e));
        m.layers[first].hits = 2u; *cpu = f.cpu;
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_devices_apply_compat_hook(
            m.devices, m.bus, cpu, &m.layers[second], m.logger, &e));
        SEMU_TEST_EQ_U64(context, 1u, m.layers[second].hits);
        semu_sapporo_devices_destroy(m.devices);
        semu_apollo4_destroy(m.soc);
        semu_cpu_destroy(m.cpu);
        destroy(&f);
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_sapporo_235_gps_reopen_activation),
        SEMU_TEST_CASE(test_sapporo_235_gps_reopen_delayed_and_isolated),
        SEMU_TEST_CASE(test_sapporo_235_gps_reopen_atomic_refusals),
        SEMU_TEST_CASE(test_sapporo_235_gps_reopen_binding_and_reset)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
