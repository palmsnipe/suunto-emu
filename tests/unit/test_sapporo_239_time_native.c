#include "test.h"
#include "../../src/compat/sapporo_239.h"
#include "../../src/compat/sapporo_239_files.h"
#include <stdio.h>
#include <string.h>

#define RAM UINT32_C(0x10000000)

typedef struct fixture {
    semu_bus *bus;
    semu_sapporo_239_files *files;
    semu_layer_state layer;
    semu_cpu_state cpu;
    semu_logger logger;
    semu_error error;
    FILE *log;
} fixture;

static void check_open(semu_test_context *context, fixture *f,
    const char *path, uint32_t mode, semu_status expected)
{
    semu_snapshot_writer before, after;
    semu_cpu_state cpu;
    uint8_t ram[4096], actual[4096];
    uint64_t hits = f->layer.hits;
    uint64_t file_hits = semu_sapporo_239_wbsto_layer.interventions[2].hits;
    long position = ftell(f->log);
    size_t n = strlen(path);
    SEMU_TEST_ASSERT(context, n <= 64u);
    memset(ram, 0xa5, sizeof(ram));
    memcpy(ram, path, n < 64u ? n + 1u : 64u);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f->bus, RAM, ram, sizeof(ram), &f->error));
    f->cpu.r[0] = RAM; f->cpu.r[1] = mode; f->cpu.r[2] = 0x10034da0u;
    f->cpu.r[14] = 0xacb8bu; f->cpu.r[15] = 0x920b4u; cpu = f->cpu;
    semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f->files, &before, &f->error));
    SEMU_TEST_EQ_U64(context, expected, semu_sapporo_239_apply_file_hook(
        f->files, f->bus, &f->cpu, &f->layer, &f->logger, &f->error));
    SEMU_TEST_ASSERT(context, memcmp(&cpu, &f->cpu, sizeof(cpu)) == 0);
    SEMU_TEST_EQ_U64(context, hits, f->layer.hits);
    SEMU_TEST_EQ_U64(context, file_hits, semu_sapporo_239_wbsto_layer.interventions[2].hits);
    SEMU_TEST_ASSERT(context, position >= 0 && ftell(f->log) == position);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(f->bus, RAM, actual, sizeof(actual), &f->error));
    SEMU_TEST_ASSERT(context, memcmp(ram, actual, sizeof(ram)) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f->files, &after, &f->error));
    SEMU_TEST_EQ_U64(context, before.size, after.size);
    SEMU_TEST_ASSERT(context, memcmp(before.data, after.data, before.size) == 0);
    SEMU_TEST_EQ_U64(context, SIZE_MAX,
        semu_sapporo_239_file_size(f->files, "settings/time"));
    semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
}

static void test_time_native_routing_and_refusals(semu_test_context *context)
{
    fixture f = {0};
    uint32_t handle;
    char unterminated[65];
    const char *bad_hashes[3];
    size_t i;
    f.bus = semu_bus_create(&f.error);
    f.files = semu_sapporo_239_files_create(&f.error);
    f.log = tmpfile();
    SEMU_TEST_ASSERT(context, f.bus && f.files && f.log);
    semu_log_init(&f.logger, f.log, SEMU_LOG_INFO);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_map_ram(f.bus, "ram", RAM, 4096u, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_layer_enable_checked(&f.layer, &semu_sapporo_239_wbsto_layer,
            "sapporo-2.39.20", semu_sapporo_239_wbsto_layer.component_hashes, 3u, &f.error));
    /* E-SAP-TIME-NATIVE-239-001: leave the native entry completely untouched. */
    check_open(context, &f, "settings/time", 2u, SEMU_OK);
    check_open(context, &f, "SETTINGS/TIME", 2u, SEMU_OK); /* Existing normalization. */
    check_open(context, &f, "settings/time", 10u, SEMU_ERR_STATE);
    check_open(context, &f, "settings/time", 0u, SEMU_ERR_STATE);
    check_open(context, &f, "settings/times", 2u, SEMU_ERR_STATE);
    check_open(context, &f, "settings/time/", 2u, SEMU_ERR_STATE);
    check_open(context, &f, "settings/time.bin", 2u, SEMU_ERR_STATE);
    check_open(context, &f, "settings/time?", 2u, SEMU_ERR_STATE);
    memset(unterminated, 'x', 64u); unterminated[64] = '\0';
    check_open(context, &f, unterminated, 2u, SEMU_ERR_STATE);
    f.layer.enabled = 0;
    check_open(context, &f, "settings/time", 2u, SEMU_ERR_STATE);
    f.layer.enabled = 1;
    /* A table-owned path must still create a synthetic handle and consume hits. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_load(f.bus, RAM,
        (const uint8_t *)"settings/general", 17u, &f.error));
    f.cpu.r[0] = RAM; f.cpu.r[1] = 2u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_239_apply_file_hook(
        f.files, f.bus, &f.cpu, &f.layer, &f.logger, &f.error));
    handle = f.cpu.r[0];
    SEMU_TEST_EQ_U64(context, 0xacb8au, f.cpu.r[15]);
    SEMU_TEST_EQ_U64(context, 1u, f.layer.hits);
    semu_log_init(&f.logger, NULL, SEMU_LOG_ERROR);
    while (f.layer.hits < semu_sapporo_239_wbsto_layer.interventions[2].max_hits) {
        f.cpu.r[0] = handle; f.cpu.r[15] = 0x9221au;
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_239_apply_file_hook(
            f.files, f.bus, &f.cpu, &f.layer, &f.logger, &f.error));
    }
    semu_log_init(&f.logger, f.log, SEMU_LOG_INFO);
    check_open(context, &f, "settings/time", 2u, SEMU_OK);
    check_open(context, &f, "settings/general", 2u, SEMU_ERR_STATE);
    check_open(context, &f, "settings/times", 2u, SEMU_ERR_STATE);
    for (i = 0u; i < 3u; ++i) bad_hashes[i] = semu_sapporo_239_wbsto_layer.component_hashes[i];
    bad_hashes[1] = "wrong-application-hash";
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
        semu_layer_enable_checked(&f.layer, &semu_sapporo_239_wbsto_layer,
            "sapporo-2.39.20", bad_hashes, 3u, &f.error));
    check_open(context, &f, "settings/time", 2u, SEMU_ERR_STATE);
    semu_sapporo_239_files_destroy(f.files); semu_bus_destroy(f.bus); fclose(f.log);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_time_native_routing_and_refusals)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
