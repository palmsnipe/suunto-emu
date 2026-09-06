#include "test.h"
#include "semu/hash.h"
#include "../../src/boards/machine_internal.h"
#include "../../src/devices/sapporo_devices_internal.h"
#include "../../src/frontends/cli_snapshot.c"

static semu_machine *synthetic_machine(char path[128], semu_logger *logger, semu_error *e)
{
    static const uint8_t program[36] = {
        0u, 1u, 0u, 0x10u, 0x21u, 0u, 0u, 0u, [32] = 0u, 0xbfu, 0u, 0xbeu
    };
    semu_profile p = {0};
    semu_firmware_manifest fw = {0};
    semu_machine_options o = {0};
    semu_component *c = &fw.components[0];
    semu_machine *m;
    FILE *file; size_t written;
    if (!semu_test_temp_path(path, 128u, "gps-snapshot.bin")) return NULL;
    file = fopen(path, "wb");
    if (!file) return NULL;
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
    m = semu_machine_create(&o, e);
    if (!m) { fprintf(stderr, "synthetic machine: %s\n", e->text); return NULL; }
    /* Synthetic host fixture: production creation still requires exact hashes. */
    m->layer_count = 2u;
    if (semu_layer_enable(&m->layers[0], &semu_sapporo_239_gps_layer, p.id, e) != SEMU_OK ||
        semu_layer_enable(&m->layers[1], &semu_sapporo_239_gps_reopen_layer, p.id, e) != SEMU_OK ||
        semu_sapporo_devices_bind_gps_layers(m->devices, m->layers, 2u, m->logger, e) != SEMU_OK) {
        fprintf(stderr, "synthetic binding: %s\n", e->text);
        semu_machine_destroy(m); return NULL;
    }
    m->layers[0].hits = 2u;
    m->devices->gps_239_context.interventions[0].hits = 1u;
    m->devices->gps_239_context.interventions[1].hits = 1u;
    return m;
}

static void equal(semu_test_context *context, semu_snapshot *a, semu_snapshot *b)
{
    uint32_t id;
    for (id = 0u; id <= SEMU_SNAPSHOT_SECTION_MACHINE; ++id) {
        const uint8_t *x, *y;
        size_t nx, ny;
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_snapshot_read_section(a, id, &x, &nx));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_snapshot_read_section(b, id, &y, &ny));
        SEMU_TEST_ASSERT(context, nx == ny && memcmp(x, y, nx) == 0);
    }
}

static void arm(semu_test_context *context, semu_machine *m)
{
    semu_error e; semu_cpu_state cpu = {0};
    cpu.r[15] = SEMU_SAPPORO_239_GPS_REOPEN_PC;
    cpu.r[4] = 0x100366d8u; cpu.r[5] = 0x1003674cu; cpu.r[6] = 0x10036948u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m->bus, 0x100368f8u, 4u, 0x100472e8u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m->bus, 0x100472ecu, 4u, 0x12890fu, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m->bus, 0x1003694au, 2u, 0x0704u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m->bus, 0x1003674cu, 1u, 15u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m->bus, 0x10036757u, 1u, 2u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m->bus, 0x10036a1cu, 2u, 0x04cbu, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m->bus, 0x100588a3u, 1u, 1u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_devices_apply_compat_hook(
        m->devices, m->bus, &cpu, &m->layers[1], m->logger, &e));
}

static void reply(semu_test_context *context, semu_machine *m)
{
    semu_error e;
    semu_serial_endpoint ep = semu_sapporo_cxd5610_endpoint(m->devices->gps);
    semu_serial_transaction tx = {0u, 0u, (const uint8_t *)"@GSR\r\n", 6u, NULL, 0u};
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, ep.transfer(ep.context, &tx, &e));
}

static void test_sapporo_239_gps_reopen_snapshot_phases(semu_test_context *context)
{
    unsigned phase;
    for (phase = 0u; phase < 4u; ++phase) {
        char path[128];
        semu_error e;
        semu_logger logger;
        semu_machine *m = synthetic_machine(path, &logger, &e);
        semu_snapshot *saved = semu_snapshot_create(&e);
        semu_snapshot *a = semu_snapshot_create(&e), *b = semu_snapshot_create(&e);
        SEMU_TEST_ASSERT(context, m && saved && a && b);
        if (phase >= 1u) arm(context, m);
        if (phase >= 2u) {
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(m->scheduler, 10000000u, &e));
            reply(context, m);
        }
        if (phase >= 3u) SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_scheduler_advance(m->scheduler, 10000000u, &e));
        { /* Creating/resetting a second machine cannot reset this one's counters. */
            char other_path[128];
            semu_logger other_logger;
            semu_machine *other = synthetic_machine(other_path, &other_logger, &e);
            SEMU_TEST_ASSERT(context, other != NULL);
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_reset(other, &e));
            SEMU_TEST_EQ_U64(context, phase > 1u ? 2u : phase, m->layers[1].hits);
            semu_machine_destroy(other); (void)remove(other_path);
        }
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, saved, &e));
        if (phase == 0u) arm(context, m);
        if (phase <= 1u) {
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(m->scheduler, 10000000u, &e));
            reply(context, m);
        }
        if (phase <= 2u) SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_scheduler_advance(m->scheduler, 10000000u, &e));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, a, &e));
        /* Reset disconnects callbacks; load must explicitly restore binding. */
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_reset(m, &e));
        SEMU_TEST_EQ_U64(context, 0u, m->layers[0].hits);
        SEMU_TEST_EQ_U64(context, 0u, m->layers[1].hits);
        semu_sapporo_cxd5610_set_exchange(m->devices->gps, NULL, NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_load(m, saved, &e));
        if (phase == 0u) arm(context, m);
        if (phase <= 1u) {
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(m->scheduler, 10000000u, &e));
            reply(context, m);
        }
        if (phase <= 2u) SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_scheduler_advance(m->scheduler, 10000000u, &e));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, b, &e));
        equal(context, a, b);
        SEMU_TEST_EQ_U64(context, 2u, m->layers[0].hits);
        SEMU_TEST_EQ_U64(context, 2u, m->layers[1].hits);
        semu_snapshot_destroy(saved); semu_snapshot_destroy(a); semu_snapshot_destroy(b);
        semu_machine_destroy(m); (void)remove(path);
    }
}

static void test_sapporo_239_gps_reopen_uart_irq(semu_test_context *context)
{
    char path[128];
    semu_error e;
    semu_logger logger;
    semu_machine *m = synthetic_machine(path, &logger, &e);
    static const uint8_t line[] = "$PSS0000\r\n";
    unsigned i; uint32_t value;
    SEMU_TEST_ASSERT(context, m != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m->bus, 0x4001d024u, 4u, 1u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m->bus, 0x4001d028u, 4u, 0x28u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m->bus, 0x4001d02cu, 4u, 0x70u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m->bus, 0x4001d030u, 4u, 0x319u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(m->bus, 0x4001d038u, 4u, 0x51u, &e));
    arm(context, m);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(m->scheduler, 9999999u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(m->bus, 0x4001d040u, 4u, &value, &e));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(m->scheduler, 1u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(m->bus, 0x4001d040u, 4u, &value, &e));
    SEMU_TEST_EQ_U64(context, SEMU_APOLLO4_UART_INTERRUPT_RX, value);
    for (i = 0u; i < 10u; ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(m->bus, 0x4001d000u, 4u, &value, &e));
        SEMU_TEST_EQ_U64(context, line[i], value);
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(m->bus, 0x4001d040u, 4u, &value, &e));
    SEMU_TEST_EQ_U64(context, 0u, value);
    semu_machine_destroy(m); (void)remove(path);
}

static void put64(uint8_t *p, uint64_t value)
{
    unsigned i;
    for (i = 0u; i < 8u; ++i) p[i] = (uint8_t)(value >> (8u * i));
}

static void test_sapporo_239_gps_reopen_snapshot_refusal(semu_test_context *context)
{
    char path[128];
    semu_error e;
    semu_logger logger;
    semu_machine *m = synthetic_machine(path, &logger, &e);
    semu_snapshot *before = semu_snapshot_create(&e), *bad = semu_snapshot_create(&e);
    semu_snapshot *after = semu_snapshot_create(&e);
    const uint8_t *data;
    uint8_t *copy; size_t size, aggregate;
    unsigned mode;
    SEMU_TEST_ASSERT(context, m && before && bad && after);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, before, &e));
    for (mode = 0u; mode < 7u; ++mode) {
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, bad, &e));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_snapshot_read_section(bad, SEMU_SNAPSHOT_SECTION_MACHINE, &data, &size));
        copy = malloc(size);
        SEMU_TEST_ASSERT(context, copy != NULL);
        memcpy(copy, data, size);
        aggregate = 62u + strlen(semu_sapporo_239_gps_layer.id) +
            strlen(semu_sapporo_239_gps_reopen_layer.id);
        if (mode == 0u) { put64(copy + aggregate, 1u); put64(copy + aggregate + 20u, 1u); }
        if (mode == 1u) put64(copy + aggregate, 1u); /* Unattributed hit. */
        if (mode == 2u) copy[aggregate - strlen(semu_sapporo_239_gps_reopen_layer.id) - 1u] ^= 1u; /* Different layer ID. */
        if (mode == 3u) copy[aggregate - 1u] = 0u; /* Disabled fixture. */
        if (mode == 4u) { /* Reopen progress before completed dependency. */
            put64(copy + 29u + strlen(semu_sapporo_239_gps_layer.id), 1u);
            put64(copy + 49u + strlen(semu_sapporo_239_gps_layer.id), 0u);
            put64(copy + aggregate, 1u); put64(copy + aggregate + 12u, 1u);
        }
        if (mode == 5u) put64(copy + aggregate, 3u);
        if (mode == 6u) copy[aggregate + 8u] = 1u; /* Wrong intervention count. */
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_snapshot_write_section(bad, SEMU_SNAPSHOT_SECTION_MACHINE, copy, size, &e));
        free(copy);
        SEMU_TEST_ASSERT(context, semu_machine_snapshot_load(m, bad, &e) != SEMU_OK);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, after, &e));
        equal(context, before, after);
    }
    semu_snapshot_destroy(before); semu_snapshot_destroy(bad); semu_snapshot_destroy(after);
    semu_machine_destroy(m); (void)remove(path);
}

/* Read-only private snapshot inspector. All firmware is validated before
 * creation; no guest code execution, callback replacement or guest writes. */
static int inspect(const char *manifest, const char *flash, const char *path)
{
    const char *layers[] = {"sapporo-2.39-synthetic-wbsto", "sapporo-2.39-gps-startup",
        "sapporo-2.39-gps-reopen"};
    semu_profile p;
    semu_firmware_manifest fw;
    semu_machine_options o = {0};
    semu_machine *m = NULL;
    semu_snapshot *s = NULL;
    semu_error e;
    uint8_t digest[SEMU_SHA256_SIZE];
    semu_logger logger;
    char hash[65]; uint64_t size;
    uint32_t callback, pending, retry;
    const semu_cpu_state *cpu;
    size_t i, rx = 0u;
    int result = 1;
    if (semu_profile_load("profiles/sapporo/2.39.20/profile.semu", &p, &e) != SEMU_OK ||
        semu_manifest_load(manifest, &fw, &e) != SEMU_OK ||
        semu_manifest_validate(&p, &fw, &e) != SEMU_OK ||
        semu_sha256_file(flash, digest, &size, &e) != SEMU_OK) goto done;
    semu_sha256_format(digest, hash);
    if (strcmp(hash, "37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb")) return 2;
    o.profile = &p; o.firmware = &fw; o.external_flash_path = flash;
    semu_log_init(&logger, NULL, SEMU_LOG_ERROR); o.logger = &logger;
    o.layers = layers; o.layer_count = 3u;
    m = semu_machine_create(&o, &e); s = semu_snapshot_create(&e);
    if (!m || !s || semu_cli_snapshot_load_file(path, s, &e) != SEMU_OK ||
        semu_machine_snapshot_load(m, s, &e) != SEMU_OK) goto done;
    if (semu_bus_read(m->bus, 0x1003694au, 1u, &callback, &e) != SEMU_OK ||
        semu_bus_read(m->bus, 0x1003694bu, 1u, &pending, &e) != SEMU_OK ||
        semu_bus_read(m->bus, 0x100588a4u, 1u, &retry, &e) != SEMU_OK) goto done;
    for (i = 0u; i < m->scheduler->count; ++i)
        if (m->scheduler->events[i].state.kind == SEMU_SCHED_EVENT_CXD_RX) ++rx;
    cpu = semu_cpu_get_state(m->cpu);
    if (cpu->r[15] == 0x128c34u) {
        uint8_t command[7];
        if (semu_bus_copy_out(m->bus, cpu->r[1], command, 7u, &e) != SEMU_OK ||
            memcmp(command, "@GSTP\r\n", 7u) != 0) goto done;
        puts("tx=GSTP");
    }
    if (cpu->r[15] == 0x1c0db4u) {
        uint32_t address, pc;
        if (!semu_cpu_fault_address(m->cpu, &address) ||
            semu_bus_read(m->bus, cpu->psp + 24u, 4u, &pc, &e) != SEMU_OK) goto done;
        printf("fault_address=%08x stacked_pc=%08x\n", address, pc);
    }
    printf("pc=%08x instructions=%llu time=%llu callback=%u pending=%u retry=%u r2=%u hits=%llu startup=%llu reply=%llu reopen=%llu gsr=%llu rx_events=%zu\n",
        cpu->r[15], (unsigned long long)semu_machine_instructions(m),
        (unsigned long long)semu_machine_virtual_time(m), callback, pending, retry, cpu->r[2],
        (unsigned long long)m->layers[1].hits,
        (unsigned long long)m->layers[1].descriptor->interventions[0].hits,
        (unsigned long long)m->layers[1].descriptor->interventions[1].hits,
        (unsigned long long)m->layers[2].descriptor->interventions[0].hits,
        (unsigned long long)m->layers[2].descriptor->interventions[1].hits, rx);
    result = 0;
done:
    if (result) fprintf(stderr, "%s\n", e.text);
    semu_snapshot_destroy(s); semu_machine_destroy(m);
    return result;
}

int main(int argc, char **argv)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_sapporo_239_gps_reopen_snapshot_phases),
        SEMU_TEST_CASE(test_sapporo_239_gps_reopen_uart_irq),
        SEMU_TEST_CASE(test_sapporo_239_gps_reopen_snapshot_refusal)
    };
    if (argc == 5 && strcmp(argv[1], "--inspect") == 0)
        return inspect(argv[2], argv[3], argv[4]);
    if (argc != 1) return 2;
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
