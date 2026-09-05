#include "test.h"
#include "../../src/compat/sapporo_239.h"
#include "../../src/compat/sapporo_239_files_internal.h"
#include <string.h>

#define RAM UINT32_C(0x10000000)

typedef struct fixture {
    semu_bus *bus;
    semu_sapporo_239_files *files;
    semu_layer_state layer;
    semu_cpu_state cpu;
    semu_logger logger;
    semu_error error;
} fixture;

static semu_status call(fixture *f, uint32_t pc, uint32_t a, uint32_t b, uint32_t c)
{
    f->cpu.r[0] = a; f->cpu.r[1] = b; f->cpu.r[2] = c;
    f->cpu.r[14] = 0x843e9u; f->cpu.r[15] = pc;
    return semu_sapporo_239_apply_file_hook(f->files, f->bus, &f->cpu,
        &f->layer, &f->logger, &f->error);
}

static void check_open(semu_test_context *context, fixture *f,
                        const char *path, semu_status expected)
{
    semu_snapshot_writer before, after;
    semu_cpu_state saved;
    uint8_t ram[64] = {0}, actual[64];
    uint64_t hits = f->layer.hits;
    memcpy(ram, path, strlen(path));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f->bus, RAM, ram, sizeof(ram), &f->error));
    f->cpu.r[0] = RAM; f->cpu.r[1] = 9u; f->cpu.r[2] = 0u;
    f->cpu.r[14] = 0x843e9u; f->cpu.r[15] = 0x920b4u; saved = f->cpu;
    semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f->files, &before, &f->error));
    SEMU_TEST_EQ_U64(context, expected, call(f, 0x920b4u, RAM, 9u, 0u));
    SEMU_TEST_ASSERT(context, memcmp(&saved, &f->cpu, sizeof(saved)) == 0);
    SEMU_TEST_EQ_U64(context, hits, f->layer.hits);
    SEMU_TEST_EQ_U64(context, hits,
        f->layer.descriptor->interventions[SEMU_SAPPORO_239_IV_LOGICAL_FILE].hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(f->bus, RAM, actual, sizeof(actual), &f->error));
    SEMU_TEST_ASSERT(context, memcmp(ram, actual, sizeof(ram)) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f->files, &after, &f->error));
    SEMU_TEST_EQ_U64(context, before.size, after.size);
    SEMU_TEST_ASSERT(context, memcmp(before.data, after.data, before.size) == 0);
    semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
}

static void test_quiet_read_routing_and_synthetic_ownership(semu_test_context *context)
{
    fixture f = {0};
    uint32_t handle, i;
    uint8_t value;
    f.bus = semu_bus_create(&f.error);
    f.files = semu_sapporo_239_files_create(&f.error);
    SEMU_TEST_ASSERT(context, f.bus != NULL && f.files != NULL);
    semu_log_init(&f.logger, NULL, SEMU_LOG_ERROR);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_map_ram(f.bus, "ram", RAM, 4096u, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_layer_enable_checked(&f.layer, &semu_sapporo_239_wbsto_layer,
            "sapporo-2.39.20", semu_sapporo_239_wbsto_layer.component_hashes,
            3u, &f.error));
    check_open(context, &f, "ui/js/config.js", SEMU_OK);
    check_open(context, &f, "UI/JS/CONFIG.JS", SEMU_OK);
    check_open(context, &f, "unlisted/native-read.bin", SEMU_OK);
    for (i = 0u; i < S239_FILE_COUNT; ++i) {
        const char *path = semu_s239_file_paths[i];
        check_open(context, &f, path, SEMU_ERR_STATE); /* Absent but table-owned. */
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920b4u, RAM, 2u, 0u));
        handle = f.cpu.r[0];
        if (semu_s239_file_capacities[i] != 0u) {
            value = (uint8_t)(i + 1u);
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_bus_load(f.bus, RAM + 256u, &value, 1u, &f.error));
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                call(&f, 0x921a8u, handle, RAM + 256u, 1u));
        }
        check_open(context, &f, path, SEMU_ERR_STATE); /* Present; no native alias. */
        check_open(context, &f, "ui/js/config.js", SEMU_OK);
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920f4u, handle, 0u, 0u));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_load(f.bus, RAM, (const uint8_t *)path, strlen(path) + 1u, &f.error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920b4u, RAM, 1u, 0u));
        SEMU_TEST_EQ_U64(context, 0x843e8u, f.cpu.r[15]);
        handle = f.cpu.r[0];
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            call(&f, 0x921dcu, handle, RAM + 512u, 1u));
        SEMU_TEST_EQ_U64(context, semu_s239_file_capacities[i] != 0u, f.cpu.r[0]);
        if (semu_s239_file_capacities[i] != 0u) {
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_bus_copy_out(f.bus, RAM + 512u, &value, 1u, &f.error));
            SEMU_TEST_EQ_U64(context, i + 1u, value);
        }
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920f4u, handle, 0u, 0u));
    }
    check_open(context, &f, "SETTINGS/GENERAL", SEMU_ERR_STATE);
    SEMU_TEST_EQ_U64(context, S239_FILE_COUNT, semu_sapporo_239_files_count(f.files));
    semu_sapporo_239_files_destroy(f.files); semu_bus_destroy(f.bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_quiet_read_routing_and_synthetic_ownership)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
