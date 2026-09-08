#include "test.h"
#include "../../src/compat/sapporo_239.h"
#include "../../src/compat/sapporo_239_files.h"
#include <stdio.h>
#include <string.h>

#define RAM UINT32_C(0x10000000)
#define OLD_LIMIT UINT64_C(76258)
#define LIMIT UINT64_C(76599)

typedef struct fixture {
    semu_bus *bus;
    semu_sapporo_239_files *files;
    semu_layer_state layer;
    semu_cpu_state cpu;
    semu_logger logger;
    semu_error error;
} fixture;

static semu_status call(fixture *f, uint32_t pc, uint32_t a,
                         uint32_t b, uint32_t c)
{
    f->cpu.r[0] = a; f->cpu.r[1] = b; f->cpu.r[2] = c;
    f->cpu.r[14] = 0x80001u; f->cpu.r[15] = pc;
    return semu_sapporo_239_apply_file_hook(f->files, f->bus, &f->cpu,
                                           &f->layer, &f->logger, &f->error);
}

static void test_post_logo_activity_budget(semu_test_context *context)
{
    static const char *const paths[] = {
        "actitmln/247.bin", "actitmln/ongoing.bin"
    };
    static const uint32_t sizes[] = {46112u, 152u};
    fixture f = {0};
    uint32_t handle = 0u, i, record;
    uint8_t bytes[40], before_ram[512], after_ram[512];
    semu_cpu_state expected;
    semu_snapshot_writer before, after;
    FILE *log = tmpfile();
    long log_position;
    f.bus = semu_bus_create(&f.error);
    f.files = semu_sapporo_239_files_create(&f.error);
    SEMU_TEST_ASSERT(context, f.bus != NULL && f.files != NULL && log != NULL);
    semu_log_init(&f.logger, NULL, SEMU_LOG_ERROR);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_map_ram(f.bus, "ram", RAM, 65536u, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_layer_enable_checked(&f.layer, &semu_sapporo_239_wbsto_layer,
            "sapporo-2.39.20", semu_sapporo_239_wbsto_layer.component_hashes,
            3u, &f.error));
    /* Synthetic fixture only: two zero files and recognizable record bytes. */
    memset(bytes, 0x5a, sizeof(bytes));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f.bus, RAM + 256u, bytes, sizeof(bytes), &f.error));
    for (i = 0u; i < 2u; ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_load(f.bus, RAM, (const uint8_t *)paths[i],
                           strlen(paths[i]) + 1u, &f.error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920b4u, RAM, 2u, 0u));
        handle = f.cpu.r[0];
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            call(&f, 0x921a8u, handle, RAM + 4096u, sizes[i]));
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920f4u, handle, 0u, 0u));
    }
    /* The next independent mode/path must still refuse below the budget. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f.bus, RAM, (const uint8_t *)"wui_dump.bin", 13u, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, call(&f, 0x920b4u, RAM, 10u, 0u));
    SEMU_TEST_ASSERT(context, strstr(f.error.text, "open mode") != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, call(&f, 0x920b4u, RAM, 2u, 0u));
    SEMU_TEST_ASSERT(context, strstr(f.error.text, "writable file path") != NULL);
    SEMU_TEST_EQ_U64(context, 6u, f.layer.hits);
    SEMU_TEST_EQ_U64(context, 2u, semu_sapporo_239_files_count(f.files));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f.bus, RAM, (const uint8_t *)paths[0],
                       strlen(paths[0]) + 1u, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920b4u, RAM, 1u, 0u));
    handle = f.cpu.r[0];
    while (f.layer.hits < OLD_LIMIT - 1u)
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x9221au, handle, 0u, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920f4u, handle, 0u, 0u));
    semu_log_init(&f.logger, log, SEMU_LOG_INFO);
    /* E-SAP-COMPAT-ACTIVITY-239-001: nine plus twelve operations. */
    for (i = 0u; i < 2u; ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_load(f.bus, RAM, (const uint8_t *)paths[i],
                           strlen(paths[i]) + 1u, &f.error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920b4u, RAM, 3u, 0u));
        handle = f.cpu.r[0];
        for (record = 0u; record <= i; ++record) {
            uint32_t offset = 32u + record * 40u;
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                call(&f, 0x92182u, handle, offset, 0u));
            SEMU_TEST_EQ_U64(context, offset, f.cpu.r[0]);
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                call(&f, 0x921a8u, handle, RAM + 256u, 40u));
            SEMU_TEST_EQ_U64(context, 40u, f.cpu.r[0]);
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                call(&f, 0x921a8u, handle, RAM + 296u, 40u));
            SEMU_TEST_EQ_U64(context, 40u, f.cpu.r[0]);
        }
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x92182u, handle, 0u, 0u));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            call(&f, 0x921dcu, handle, RAM + 128u, 24u));
        SEMU_TEST_EQ_U64(context, 24u, f.cpu.r[0]);
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x92182u, handle, 0u, 0u));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            call(&f, 0x921a8u, handle, RAM + 128u, 24u));
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920f4u, handle, 0u, 0u));
        SEMU_TEST_EQ_U64(context, 1u, f.cpu.r[0]);
        SEMU_TEST_EQ_U64(context, sizes[i], semu_sapporo_239_file_size(f.files, paths[i]));
        SEMU_TEST_EQ_U64(context, OLD_LIMIT + (i == 0u ? 9u : 21u), f.layer.hits);
    }
    SEMU_TEST_EQ_U64(context, LIMIT,
        f.layer.descriptor->interventions[SEMU_SAPPORO_239_IV_LOGICAL_FILE].max_hits);
    SEMU_TEST_EQ_U64(context, LIMIT + 3u, f.layer.descriptor->maximum_hits);
    /* Preserve the 21-hit activity suffix above; saturate the later ceiling
     * separately before checking the same excess-open refusal. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920b4u, RAM, 3u, 0u));
    handle = f.cpu.r[0];
    while (f.layer.hits < LIMIT - 1u)
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x9221au, handle, 0u, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920f4u, handle, 0u, 0u));
    semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f.files, &before, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(f.bus, RAM, before_ram, sizeof(before_ram), &f.error));
    f.cpu.r[0] = RAM; f.cpu.r[1] = 3u; f.cpu.r[2] = 0u;
    f.cpu.r[15] = 0x920b4u; expected = f.cpu;
    log_position = ftell(log);
    SEMU_TEST_ASSERT(context, log_position >= 0);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, call(&f, 0x920b4u, RAM, 3u, 0u));
    SEMU_TEST_ASSERT(context, strstr(f.error.text, "exceeded budget") != NULL);
    SEMU_TEST_ASSERT(context, memcmp(&expected, &f.cpu, sizeof(expected)) == 0);
    SEMU_TEST_EQ_U64(context, LIMIT, f.layer.hits);
    SEMU_TEST_EQ_U64(context, LIMIT,
        f.layer.descriptor->interventions[SEMU_SAPPORO_239_IV_LOGICAL_FILE].hits);
    SEMU_TEST_ASSERT(context, ftell(log) == log_position);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(f.bus, RAM, after_ram, sizeof(after_ram), &f.error));
    SEMU_TEST_ASSERT(context, memcmp(before_ram, after_ram, sizeof(before_ram)) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f.files, &after, &f.error));
    SEMU_TEST_EQ_U64(context, before.size, after.size);
    SEMU_TEST_ASSERT(context, memcmp(before.data, after.data, before.size) == 0);
    semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
    semu_sapporo_239_files_destroy(f.files); semu_bus_destroy(f.bus);
    (void)fclose(log);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_post_logo_activity_budget)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
