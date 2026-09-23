#include "test.h"
#include "semu/manifest.h"
#include "sapporo_235_gps_awake.h"
#include "../../src/boards/machine_internal.h"
#include "../../src/soc/apollo4/apollo4_internal.h"
#include <stdio.h>
#include <string.h>

static void test_sapporo_235_gps_awake_activation(semu_test_context *context)
{
    semu_profile profile;
    semu_error error;
    semu_layer_state state;
    const semu_layer_descriptor *d = &semu_sapporo_235_gps_awake_layer;
    const char *hashes[3];
    unsigned i;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_profile_load("profiles/sapporo/2.35.34/profile.semu", &profile, &error));
    SEMU_TEST_EQ_U64(context, 64u, d->maximum_hits);
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
    semu_sapporo_235_gps_awake_context provider;
    semu_sapporo_235_gps_reopen_context reopen;
    semu_sapporo_235_gps_context initial;
    semu_layer_state states[3];
    semu_logger logger;
    FILE *log;
    semu_scheduler *scheduler;
    semu_sapporo_cxd5610 *gps;
    semu_bus *bus;
    semu_cpu_state cpu;
    unsigned signals, highs;
    int level;
    uint64_t signal_time;
} fixture;

static void signal_awake(void *context, unsigned channel, int level)
{
    fixture *f = context;
    (void)channel;
    ++f->signals; f->highs += level != 0; f->level = level;
    f->signal_time = semu_scheduler_now(f->scheduler);
}

static int init(fixture *f, int gpio_ram, semu_error *e)
{
    memset(f, 0, sizeof(*f));
    f->log = tmpfile();
    f->scheduler = semu_scheduler_create(e);
    f->bus = semu_bus_create(e);
    f->gps = semu_sapporo_cxd5610_create(f->scheduler, signal_awake, f,
        NULL, NULL, NULL, NULL, e);
    if (!f->log || !f->scheduler || !f->bus || !f->gps) return 0;
    semu_log_init(&f->logger, f->log, SEMU_LOG_DEBUG);
    if (semu_bus_map_ram(f->bus, "sram", 0x10000000u, 0x180000u, e) != SEMU_OK ||
        semu_layer_enable(&f->states[0], &semu_sapporo_235_gps_layer,
            "sapporo-2.35.34", e) != SEMU_OK ||
        semu_layer_enable(&f->states[1], &semu_sapporo_235_gps_reopen_layer,
            "sapporo-2.35.34", e) != SEMU_OK ||
        semu_layer_enable(&f->states[2], &semu_sapporo_235_gps_awake_layer,
            "sapporo-2.35.34", e) != SEMU_OK) return 0;
    if (gpio_ram && (semu_bus_map_ram(f->bus, "gpio-fixture", 0x40010000u, 0x400u, e) != SEMU_OK ||
        semu_bus_write(f->bus, 0x40010060u, 4u, 0x93u, e) != SEMU_OK)) return 0;
    f->states[0].hits = f->states[1].hits = 2u; /* Completed synthetic dependencies. */
    f->initial.state = &f->states[0]; f->initial.logger = &f->logger;
    f->reopen.state = &f->states[1]; f->reopen.logger = &f->logger;
    f->reopen.startup = &f->initial;
    f->provider.state = &f->states[2]; f->provider.logger = &f->logger;
    f->provider.reopen = &f->reopen;
    f->cpu.r[15] = SEMU_SAPPORO_235_GPS_AWAKE_PC;
    f->cpu.r[8] = 0x100364acu; f->cpu.r[4] = 0x100589feu;
    f->cpu.r[5] = 0x10036718u; f->cpu.r[6] = 0x100367c0u; f->cpu.r[7] = 0x10036521u;
    return semu_bus_write(f->bus, 0x1003671eu, 2u, 0x0a0cu, e) == SEMU_OK &&
        semu_bus_write(f->bus, 0x100589feu, 1u, 1u, e) == SEMU_OK;
}

static void destroy(fixture *f)
{
    semu_bus_destroy(f->bus);
    semu_sapporo_cxd5610_destroy(f->gps);
    semu_scheduler_destroy(f->scheduler);
    if (f->log) (void)fclose(f->log);
}

static semu_status poll(fixture *f, semu_error *e)
{
    return semu_sapporo_235_gps_awake_poll(&f->provider, f->gps, f->bus, &f->cpu, e);
}

static void test_sapporo_235_gps_awake_timing_and_budget(semu_test_context *context)
{
    fixture a, b;
    semu_error e;
    semu_cpu_state cpu;
    unsigned i;
    SEMU_TEST_ASSERT(context, init(&a, 1, &e));
    SEMU_TEST_ASSERT(context, init(&b, 1, &e));
    cpu = a.cpu;
    for (i = 0u; i < 64u; ++i) {
        uint64_t now = semu_scheduler_now(a.scheduler);
        SEMU_TEST_EQ_U64(context, SEMU_OK, poll(&a, &e));
        SEMU_TEST_EQ_U64(context, i + 1u, a.states[2].hits);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(a.scheduler, 99999999u, &e));
        SEMU_TEST_EQ_U64(context, i, a.highs);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(a.scheduler, 1u, &e));
        SEMU_TEST_EQ_U64(context, 1u, a.level);
        SEMU_TEST_EQ_U64(context, now + 100000000u, a.signal_time);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(a.scheduler, 999999u, &e));
        SEMU_TEST_EQ_U64(context, 1u, a.level);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(a.scheduler, 1u, &e));
        SEMU_TEST_EQ_U64(context, 0u, a.level);
        SEMU_TEST_EQ_U64(context, now + 101000000u, a.signal_time);
    }
    SEMU_TEST_EQ_U64(context, 64u, a.highs);
    SEMU_TEST_EQ_U64(context, 0u, b.highs);
    SEMU_TEST_EQ_U64(context, 0u, b.states[2].hits);
    SEMU_TEST_ASSERT(context, memcmp(&cpu, &a.cpu, sizeof(cpu)) == 0);
    SEMU_TEST_ASSERT(context, poll(&a, &e) != SEMU_OK);
    SEMU_TEST_EQ_U64(context, 64u, a.states[2].hits);
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(a.scheduler));
    {
        static char log[131072]; size_t count;
        rewind(a.log); count = fread(log, 1u, sizeof(log)-1u, a.log); log[count] = '\0';
        SEMU_TEST_ASSERT(context, strstr(log, "hit=64 maximum=64") != NULL);
        SEMU_TEST_ASSERT(context, strstr(log, "delay-ns=100000000 width-ns=1000000") != NULL);
        SEMU_TEST_ASSERT(context, strstr(log, "evidence=E-SAP-0049") != NULL);
        SEMU_TEST_EQ_U64(context, 0u, ftell(b.log));
    }
    destroy(&a); destroy(&b);
}

static void test_sapporo_235_gps_awake_atomic_refusals(semu_test_context *context)
{
    unsigned mode;
    for (mode = 0u; mode < 36u; ++mode) {
        fixture f;
        semu_error e;
        semu_snapshot_writer before, after;
        uint64_t hits, next_id, sequence, now;
        size_t events;
        long log_end;
        unsigned signals;
        SEMU_TEST_ASSERT(context, init(&f, 1, &e));
        if (mode == 0u) f.states[2].enabled = 0;
        if (mode == 1u) f.states[2].descriptor = NULL;
        if (mode == 2u) f.states[2].hits = 64u;
        if (mode == 3u) f.cpu.r[8]++;
        if (mode == 4u) f.cpu.r[8] = 0x1017ff00u;
        if (mode == 5u) f.cpu.r[8] = 0x40010000u;
        if (mode >= 6u && mode <= 9u) ++f.cpu.r[mode - 2u];
        if (mode == 10u) (void)semu_bus_write(f.bus, 0x1003671eu, 1u, 4u, &e);
        if (mode == 11u) (void)semu_bus_write(f.bus, 0x1003671fu, 1u, 7u, &e);
        if (mode == 12u) (void)semu_bus_write(f.bus, 0x100589feu, 1u, 0u, &e);
        if (mode == 13u) (void)semu_bus_write(f.bus, 0x100589ffu, 1u, 1u, &e);
        if (mode == 14u) (void)semu_bus_write(f.bus, 0x10058a00u, 1u, 1u, &e);
        if (mode == 15u) (void)semu_bus_write(f.bus, 0x40010060u, 4u, 0u, &e);
        if (mode == 16u) f.provider.reopen = NULL;
        if (mode == 17u) f.reopen.startup = NULL;
        if (mode == 18u) f.states[0].hits = 1u;
        if (mode == 19u) f.states[1].hits = 1u;
        if (mode == 20u) f.states[0].enabled = 0;
        if (mode == 21u) f.states[1].enabled = 0;
        if (mode == 22u) f.states[0].descriptor = NULL;
        if (mode == 23u) f.states[1].descriptor = NULL;
        if (mode == 24u) f.initial.logger = NULL;
        if (mode == 25u) f.reopen.logger = NULL;
        if (mode == 26u) f.initial.refusal.code = SEMU_ERR_STATE;
        if (mode == 27u) f.reopen.refusal.code = SEMU_ERR_STATE;
        if (mode == 28u) f.scheduler->now_ns = UINT64_MAX - 100000000u;
        if (mode == 29u) f.scheduler->next_id = UINT64_MAX;
        if (mode == 30u) f.scheduler->next_sequence = UINT64_MAX;
        if (mode == 31u) SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_pulse_awake_after(f.gps, 1u, &e));
        if (mode == 32u) f.provider.logger = NULL;
        if (mode == 33u) {
            f.cpu.r[8] = 0x10190000u; f.cpu.r[5] = f.cpu.r[8]+0x26cu;
            f.cpu.r[6] = f.cpu.r[8]+0x314u; f.cpu.r[7] = f.cpu.r[8]+0x75u;
        }
        if (mode == 34u) f.initial.state = NULL;
        if (mode == 35u) f.reopen.state = NULL;
        hits = f.states[2].hits; events = f.scheduler->count;
        next_id = f.scheduler->next_id; sequence = f.scheduler->next_sequence;
        now = f.scheduler->now_ns; log_end = ftell(f.log); signals = f.signals;
        semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_snapshot_write(f.gps, &before, &e));
        SEMU_TEST_ASSERT(context, poll(&f, &e) != SEMU_OK);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_cxd5610_snapshot_write(f.gps, &after, &e));
        SEMU_TEST_ASSERT(context, before.size == after.size &&
            memcmp(before.data, after.data, before.size) == 0);
        SEMU_TEST_EQ_U64(context, hits, f.states[2].hits);
        SEMU_TEST_EQ_U64(context, events, f.scheduler->count);
        SEMU_TEST_EQ_U64(context, next_id, f.scheduler->next_id);
        SEMU_TEST_EQ_U64(context, sequence, f.scheduler->next_sequence);
        SEMU_TEST_EQ_U64(context, now, f.scheduler->now_ns);
        SEMU_TEST_EQ_U64(context, log_end, ftell(f.log));
        SEMU_TEST_EQ_U64(context, signals, f.signals);
        if (f.states[2].enabled) {
            semu_status code = f.provider.refusal.code;
            SEMU_TEST_ASSERT(context, code != SEMU_OK);
            f.cpu.r[15] = 0x40000u;
            SEMU_TEST_EQ_U64(context, code, poll(&f, &e));
        }
        semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
        destroy(&f);
    }
}

static void test_sapporo_235_gps_awake_binding_and_reset(semu_test_context *context)
{
    static const unsigned orders[6][3] = {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    unsigned order;
    for (order = 0u; order < 6u; ++order) {
        fixture f;
        semu_machine m;
        semu_cpu_state *cpu;
        semu_error e;
        semu_run_limits limits = {1u, 1000u};
        unsigned j, ai = 0u;
        SEMU_TEST_ASSERT(context, init(&f, 0, &e));
        memset(&m, 0, sizeof(m));
        m.bus=f.bus; m.scheduler=f.scheduler; m.logger=&f.logger; m.layer_count=3u;
        for (j=0u; j<3u; ++j) {
            m.layers[j]=f.states[orders[order][j]]; m.layers[j].hits=0u;
            if (orders[order][j]==2u) ai=j;
        }
        m.cpu=semu_cpu_create(m.bus,m.scheduler,&e);
        m.soc=semu_apollo4_create(m.bus,&e);
        m.devices=semu_sapporo_devices_create(m.scheduler,NULL,&e);
        SEMU_TEST_ASSERT(context, m.cpu && m.soc && m.devices);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_init(m.soc,m.scheduler,NULL,NULL,&e));
        SEMU_TEST_ASSERT(context, semu_sapporo_devices_bind_gps_layers(
            m.devices,m.layers,3u,m.logger,&e)!=SEMU_OK);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_devices_select_profile(
            m.devices,"sapporo-2.35.34",&e));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_devices_attach(m.devices,m.soc,&e));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m.bus,0x40010200u,4u,0x73u,&e));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m.bus,0x40010060u,4u,0x93u,&e));
        SEMU_TEST_ASSERT(context, semu_sapporo_devices_bind_gps_layers(
            m.devices,&m.layers[ai],1u,m.logger,&e)!=SEMU_OK);
        m.layers[3]=m.layers[ai];
        SEMU_TEST_ASSERT(context, semu_sapporo_devices_bind_gps_layers(
            m.devices,m.layers,4u,m.logger,&e)!=SEMU_OK);
        m.layers[ai].enabled=0;
        SEMU_TEST_ASSERT(context, semu_sapporo_devices_bind_gps_layers(
            m.devices,m.layers,3u,m.logger,&e)!=SEMU_OK);
        m.layers[ai].enabled=1; m.layers[ai].hits=1u;
        SEMU_TEST_ASSERT(context, semu_sapporo_devices_bind_gps_layers(
            m.devices,m.layers,3u,m.logger,&e)!=SEMU_OK);
        m.layers[ai].hits=0u;
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_devices_bind_gps_layers(
            m.devices,m.layers,3u,m.logger,&e));
        for (j=0u;j<3u;++j) if(j!=ai) m.layers[j].hits=2u;
        cpu=semu_cpu_get_state_mutable(m.cpu); *cpu=f.cpu;
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_devices_apply_compat_hook(
            m.devices,m.bus,cpu,&m.layers[ai],m.logger,&e));
        SEMU_TEST_EQ_U64(context, 1u, m.layers[ai].hits);
        semu_sapporo_devices_reset(m.devices);
        SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(m.scheduler));
        for (j=0u;j<3u;++j) m.layers[j].hits=0u;
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_devices_bind_gps_layers(
            m.devices,m.layers,3u,m.logger,&e));
        for (j=0u;j<3u;++j) if(j!=ai) m.layers[j].hits=2u;
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_devices_apply_compat_hook(
            m.devices,m.bus,cpu,&m.layers[ai],m.logger,&e));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(m.scheduler,100000000u,&e));
        SEMU_TEST_EQ_U64(context, 1u, semu_apollo4_gpio_get_input(m.soc->gpio,24u));
        /* A second admission while high must stop before the CPU executes. */
        SEMU_TEST_ASSERT(context, semu_sapporo_devices_apply_compat_hook(
            m.devices,m.bus,cpu,&m.layers[ai],m.logger,&e)!=SEMU_OK);
        cpu->r[15]=0x40000u;
        SEMU_TEST_EQ_U64(context, SEMU_STOP_COMPAT_REFUSED, semu_machine_run(&m,&limits,&e));
        SEMU_TEST_EQ_U64(context, 0u, cpu->instructions);
        semu_sapporo_devices_reset(m.devices);
        SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(m.scheduler));
        SEMU_TEST_EQ_U64(context, 0u, semu_apollo4_gpio_get_input(m.soc->gpio,24u));
        semu_sapporo_devices_destroy(m.devices);
        semu_apollo4_destroy(m.soc); semu_cpu_destroy(m.cpu);
        destroy(&f);
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_sapporo_235_gps_awake_activation),
        SEMU_TEST_CASE(test_sapporo_235_gps_awake_timing_and_budget),
        SEMU_TEST_CASE(test_sapporo_235_gps_awake_atomic_refusals),
        SEMU_TEST_CASE(test_sapporo_235_gps_awake_binding_and_reset)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
