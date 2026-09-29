#include "test.h"
#include "semu/hash.h"
#include "../../src/boards/machine_internal.h"
#include "../../src/devices/sapporo_devices_internal.h"
#include "../../src/frontends/cli_snapshot.c"
#include "../../src/display/nema_backend.h"

static semu_machine *synthetic_machine_layer(char path[128], semu_logger *logger,
    const semu_layer_descriptor *awake, semu_error *e)
{
    static const uint8_t program[36] = {
        0u, 1u, 0u, 0x10u, 0x21u, 0u, 0u, 0u, [32] = 0u, 0xbfu, 0u, 0xbeu
    };
    semu_profile p = {0}; semu_firmware_manifest fw = {0};
    semu_machine_options o = {0}; semu_component *c = &fw.components[0];
    semu_machine *m; FILE *file; size_t written;
    if (!semu_test_temp_path(path, 128u, "awake-snapshot.bin")) return NULL;
    file = fopen(path, "wb"); if (!file) return NULL;
    written = fwrite(program, 1u, sizeof(program), file);
    if (fclose(file) != 0 || written != sizeof(program)) return NULL;
    p.format = fw.format = 1u;
    strcpy(p.id, "sapporo-2.39.20"); strcpy(p.board, "sapporo");
    strcpy(p.product, "Synthetic"); strcpy(fw.product, "Synthetic");
    strcpy(p.version, "test"); strcpy(fw.version, "test");
    p.required_count = fw.component_count = 1u;
    strcpy(c->id, "test"); strcpy(c->role, "application");
    (void)snprintf(c->path, sizeof(c->path), "%s", path);
    if (semu_sha256_file(path, c->sha256, &c->size, e) != SEMU_OK) return NULL;
    p.required[0] = *c; p.required[0].path[0] = '\0';
    semu_log_init(logger, NULL, SEMU_LOG_ERROR);
    o.profile = &p; o.firmware = &fw; o.logger = logger;
    m = semu_machine_create(&o, e); if (!m) return NULL;
    /* Synthetic-only binding. Production creation retains all exact hashes. */
    const semu_layer_descriptor *d[] = {&semu_sapporo_239_gps_layer,
        &semu_sapporo_239_gps_reopen_layer, awake};
    m->layer_count = 3u;
    for (unsigned i = 0u; i < 3u; ++i)
        if (semu_layer_enable(&m->layers[i], d[i], p.id, e) != SEMU_OK) goto fail;
    if (semu_sapporo_devices_bind_gps_layers(m->devices, m->layers, 3u, logger, e) != SEMU_OK)
        goto fail;
    m->layers[0].hits = m->layers[1].hits = 2u;
    m->devices->gps_239_context.interventions[0].hits = m->devices->gps_239_context.interventions[1].hits = 1u;
    m->devices->gps_reopen_context.interventions[0].hits = m->devices->gps_reopen_context.interventions[1].hits = 1u;
    return m;
fail:
    semu_machine_destroy(m); return NULL;
}

static semu_machine *synthetic_machine(char path[128], semu_logger *logger, semu_error *e)
{
    return synthetic_machine_layer(path, logger, &semu_sapporo_239_gps_awake_layer, e);
}

static void equal(semu_test_context *context, semu_snapshot *a, semu_snapshot *b)
{
    for (uint32_t id = 0u; id <= SEMU_SNAPSHOT_SECTION_MACHINE; ++id) {
        const uint8_t *x, *y; size_t nx, ny;
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_snapshot_read_section(a, id, &x, &nx));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_snapshot_read_section(b, id, &y, &ny));
        SEMU_TEST_ASSERT(context, nx == ny && !memcmp(x, y, nx));
    }
}

static void arm(semu_test_context *context, semu_machine *m)
{
    semu_error e; semu_cpu_state cpu = {0};
    cpu.r[15] = SEMU_SAPPORO_239_GPS_AWAKE_PC;
    cpu.r[0] = 1u; cpu.r[2] = 12u; cpu.r[4] = 0x100588a2u;
    cpu.r[8] = 0x100366d8u; cpu.r[5] = 0x10036944u;
    cpu.r[6] = 0x100369ecu; cpu.r[7] = 0x1003674du;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m->bus, 0x1003694au, 2u, 0x0a0cu, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m->bus, 0x100588a2u, 1u, 1u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m->bus, 0x40010200u, 4u, 0x73u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m->bus, 0x40010060u, 4u, 0x93u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m->bus, 0x400102c0u, 4u, 0x1000000u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_devices_apply_compat_hook(
        m->devices, m->bus, &cpu, &m->layers[2], m->logger, &e));
}

static void finish(semu_test_context *context, semu_machine *m)
{
    semu_error e;
    for (;;) {
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(m->scheduler,
            m->layers[2].hits * UINT64_C(101000000) - semu_scheduler_now(m->scheduler), &e));
        if (m->layers[2].hits == 4u) break;
        arm(context, m);
    }
}

static void test_sapporo_239_gps_awake_snapshot_phases(semu_test_context *context)
{
    for (unsigned phase = 0u; phase < 4u; ++phase) {
        char path[128]; semu_error e; semu_logger logger;
        semu_machine *m = synthetic_machine(path, &logger, &e);
        semu_snapshot *saved = semu_snapshot_create(&e), *a = semu_snapshot_create(&e);
        semu_snapshot *b = semu_snapshot_create(&e);
        SEMU_TEST_ASSERT(context, m && saved && a && b);
        if (phase >= 1u) arm(context, m);
        if (phase >= 2u) {
            uint32_t pending;
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(m->scheduler, 100000000u, &e));
            SEMU_TEST_EQ_U64(context, 1u, semu_apollo4_gpio_get_input(m->soc->gpio, 24u));
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(m->bus, 0x400102c4u, 4u, &pending, &e));
            SEMU_TEST_EQ_U64(context, 0x1000000u, pending);
        }
        if (phase == 3u) finish(context, m);
        {
            char other_path[128]; semu_logger other_logger;
            semu_machine *other = synthetic_machine(other_path, &other_logger, &e);
            SEMU_TEST_ASSERT(context, other != NULL);
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_reset(other, &e));
            SEMU_TEST_EQ_U64(context, phase == 3u ? 4u : phase != 0u, m->layers[2].hits);
            semu_machine_destroy(other); (void)remove(other_path);
        }
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, saved, &e));
        if (phase == 0u) arm(context, m);
        finish(context, m);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, a, &e));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_reset(m, &e));
        SEMU_TEST_EQ_U64(context, 0u, m->layers[2].hits);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_load(m, saved, &e));
        if (phase == 0u) arm(context, m);
        finish(context, m);
        SEMU_TEST_EQ_U64(context, 0u, semu_apollo4_gpio_get_input(m->soc->gpio, 24u));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, b, &e));
        equal(context, a, b);
        semu_snapshot_destroy(saved); semu_snapshot_destroy(a); semu_snapshot_destroy(b);
        semu_machine_destroy(m); (void)remove(path);
    }
}

static void put64(uint8_t *p, uint64_t value)
{
    for (unsigned i = 0u; i < 8u; ++i) p[i] = (uint8_t)(value >> (8u * i));
}

static void test_sapporo_239_gps_awake_snapshot_refusal(semu_test_context *context)
{
    char path[128]; semu_error e; semu_logger logger;
    semu_machine *m = synthetic_machine(path, &logger, &e);
    semu_snapshot *before = semu_snapshot_create(&e), *bad = semu_snapshot_create(&e);
    semu_snapshot *after = semu_snapshot_create(&e);
    SEMU_TEST_ASSERT(context, m && before && bad && after);
    arm(context, m);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, before, &e));
    for (unsigned mode = 0u; mode < 10u; ++mode) {
        const uint8_t *data; uint8_t *copy; size_t size, aggregate = 29u;
        uint32_t section = mode == 9u ? SEMU_SNAPSHOT_SECTION_SCHEDULER : SEMU_SNAPSHOT_SECTION_MACHINE;
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, bad, &e));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_snapshot_read_section(bad, section, &data, &size));
        copy = malloc(size); SEMU_TEST_ASSERT(context, copy != NULL); memcpy(copy, data, size);
        for (unsigned i = 0u; i < 2u; ++i) aggregate += 33u + strlen(m->layers[i].descriptor->id);
        aggregate += strlen(m->layers[2].descriptor->id);
        if (mode == 0u) put64(copy + aggregate, 2u); /* Unattributed hit. */
        if (mode == 1u) { put64(copy + aggregate, 5u); put64(copy + aggregate + 12u, 5u); }
        if (mode == 2u) copy[aggregate - 1u] = 0u;
        if (mode == 3u) copy[aggregate - 2u] ^= 1u;
        if (mode == 4u) copy[aggregate + 8u] = 0u;
        if (mode == 5u) { /* Pending pulse with no attributed progress. */
            put64(copy + aggregate, 0u); put64(copy + aggregate + 12u, 0u);
        }
        if (mode == 6u || mode == 7u) {
            size_t dep = 29u + strlen(m->layers[0].descriptor->id);
            if (mode == 7u) dep += 33u + strlen(m->layers[1].descriptor->id);
            put64(copy + dep, 1u); put64(copy + dep + 20u, 0u);
        }
        if (mode == 8u) copy[20u] = 2u; /* No implicit layer-set migration. */
        if (mode == 9u) { SEMU_TEST_ASSERT(context, size >= 60u); copy[52u] ^= 1u; }
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_snapshot_write_section(bad, section, copy, size, &e));
        free(copy);
        SEMU_TEST_ASSERT(context, semu_machine_snapshot_load(m, bad, &e) != SEMU_OK);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, after, &e));
        equal(context, before, after);
    }
    semu_snapshot_destroy(before); semu_snapshot_destroy(bad); semu_snapshot_destroy(after);
    semu_machine_destroy(m); (void)remove(path);
}

/* Private inspector/IRQ verifier: exact component validation before execution,
 * normal renderer backend, no injected signals or native-state writes. */
static int inspect(const char *manifest, const char *flash, const char *path,
    const char *expected_final)
{
    const char *layers[] = {"sapporo-2.39-synthetic-wbsto", "sapporo-2.39-gps-startup",
        "sapporo-2.39-gps-reopen", "sapporo-2.39-gps-awake"};
    semu_profile p; semu_firmware_manifest fw; semu_machine_options o = {0};
    semu_machine *m = NULL; semu_snapshot *s = NULL, *expected = NULL;
    semu_nema_backend *backend = NULL; semu_error e; semu_logger logger;
    uint8_t digest[32]; char hash[65]; uint64_t size;
    uint32_t callback, pending, retry, awake; size_t pulses = 0u; unsigned stage = 0u;
    int result = 1;
    if (semu_profile_load("profiles/sapporo/2.39.20/profile.semu", &p, &e) != SEMU_OK ||
        semu_manifest_load(manifest, &fw, &e) != SEMU_OK ||
        semu_manifest_validate(&p, &fw, &e) != SEMU_OK ||
        semu_sha256_file(flash, digest, &size, &e) != SEMU_OK) goto done;
    semu_sha256_format(digest, hash);
    if (strcmp(hash, "37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb")) return 2;
    backend = semu_nema_backend_create(&e); if (!backend) goto done;
    o.profile = &p; o.firmware = &fw; o.external_flash_path = flash;
    semu_log_init(&logger, NULL, SEMU_LOG_ERROR); o.logger = &logger;
    o.layers = layers; o.layer_count = 4u;
    o.display_backend = &semu_nema_backend_ops; o.display_backend_context = backend;
    /* Ticket 791 added the display section to machine snapshots; this
     * production machine must register the same codec as the CLI so the
     * section round-trips instead of refusing (ticket 777 classification). */
    o.display_snapshot = &semu_nema_backend_snapshot_ops;
    m = semu_machine_create(&o, &e); s = semu_snapshot_create(&e);
    if (!m || !s || semu_cli_snapshot_load_file(path, s, &e) != SEMU_OK ||
        semu_machine_snapshot_load(m, s, &e) != SEMU_OK) goto done;
#define VERIFY(condition) do { if (!(condition)) { \
    semu_error_set(&e, SEMU_ERR_STATE, "private awake verification failed at line %u", \
        (unsigned)__LINE__); goto done; } } while (0)
    if (expected_final != NULL) {
        static const uint64_t entries[] = {827008206u, 941844404u, 1052945626u, 1163113462u};
        VERIFY(semu_machine_instructions(m) == 846889602u && m->layers[3].hits == 0u);
        for (unsigned i = 0u; i < 4u; ++i) {
            semu_run_limits limits = {entries[i] - semu_machine_instructions(m),
                UINT64_C(35000000000) - semu_machine_virtual_time(m)};
            VERIFY(semu_machine_run(m, &limits, &e) == SEMU_STOP_BUDGET);
            VERIFY(semu_machine_program_counter(m) == 0x128926u && m->layers[3].hits == i + 1u);
            VERIFY(semu_bus_read(m->bus, 0x100588a2u, 1u, &awake, &e) == SEMU_OK && awake == 0u);
            limits.max_instructions = 3u;
            VERIFY(semu_machine_run(m, &limits, &e) == SEMU_STOP_BUDGET);
            VERIFY(semu_machine_program_counter(m) == 0x12892eu);
            VERIFY(semu_bus_read(m->bus, 0x100588a2u, 1u, &awake, &e) == SEMU_OK && awake == 1u);
            printf("native-awake-irq=%u instructions=%llu\n", i + 1u,
                (unsigned long long)semu_machine_instructions(m));
        }
        semu_run_limits limits = {UINT64_C(1300000000) - semu_machine_instructions(m),
            UINT64_C(35000000000) - semu_machine_virtual_time(m)};
        VERIFY(semu_machine_run(m, &limits, &e) == SEMU_STOP_COMPAT_REFUSED);
        expected = semu_snapshot_create(&e);
        VERIFY(expected && semu_cli_snapshot_load_file(expected_final, expected, &e) == SEMU_OK);
        VERIFY(semu_machine_snapshot_save(m, s, &e) == SEMU_OK);
        semu_test_context comparison = {"private awake IRQ final snapshot", 0u};
        equal(&comparison, expected, s); VERIFY(comparison.failures == 0u);
    }
    if (semu_bus_read(m->bus, 0x1003694au, 1u, &callback, &e) != SEMU_OK ||
        semu_bus_read(m->bus, 0x1003694bu, 1u, &pending, &e) != SEMU_OK ||
        semu_bus_read(m->bus, 0x100588a4u, 1u, &retry, &e) != SEMU_OK ||
        semu_bus_read(m->bus, 0x100588a2u, 1u, &awake, &e) != SEMU_OK) goto done;
    for (size_t i = 0u; i < semu_scheduler_event_count(m->scheduler); ++i) {
        const semu_scheduled_event_state *event = semu_scheduler_event_get(m->scheduler, i);
        if (event->kind == SEMU_SCHED_EVENT_CXD_AWAKE) { ++pulses; stage = event->subject; }
    }
    printf("pc=%08x instructions=%llu time=%llu callback=%u pending=%u retry=%u awake=%u hits=%llu,%llu,%llu pulse_events=%zu stage=%u gpio24=%d\n",
        semu_machine_program_counter(m), (unsigned long long)semu_machine_instructions(m),
        (unsigned long long)semu_machine_virtual_time(m), callback, pending, retry, awake,
        (unsigned long long)m->layers[1].hits, (unsigned long long)m->layers[2].hits,
        (unsigned long long)m->layers[3].hits, pulses, stage,
        semu_apollo4_gpio_get_input(m->soc->gpio, 24u));
    result = 0;
done:
    if (result) fprintf(stderr, "%s\n", e.text);
    semu_snapshot_destroy(s); semu_snapshot_destroy(expected);
    semu_machine_destroy(m); semu_nema_backend_destroy(backend); return result;
#undef VERIFY
}

int main(int argc, char **argv)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_sapporo_239_gps_awake_snapshot_phases),
        SEMU_TEST_CASE(test_sapporo_239_gps_awake_snapshot_refusal)
    };
    if (argc == 5 && !strcmp(argv[1], "--inspect"))
        return inspect(argv[2], argv[3], argv[4], NULL);
    if (argc == 6 && !strcmp(argv[1], "--verify-irqs"))
        return inspect(argv[2], argv[3], argv[4], argv[5]);
    if (argc != 1) return 2;
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
