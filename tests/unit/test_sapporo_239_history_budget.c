#include "test.h"
#include "../../src/compat/sapporo_239.h"
#include "../../src/compat/sapporo_239_files.h"
#include <stdio.h>
#include <string.h>

#define RAM UINT32_C(0x10000000)
#define LIMIT UINT64_C(76258)

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

static void test_history_scan_budget_and_atomic_refusal(semu_test_context *context)
{
    fixture f = {0};
    uint32_t handle, day, record;
    uint8_t before_ram[72], after_ram[72];
    semu_cpu_state expected;
    semu_snapshot_writer before, after;
    FILE *log = tmpfile();
    char line[512];
    unsigned events = 0u;
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
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f.bus, RAM, (const uint8_t *)"sleepln/sleep.bin", 17u, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920b4u, RAM, 2u, 0u));
    handle = f.cpu.r[0];
    /* Synthetic zero-filled records, never firmware-derived bytes. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        call(&f, 0x921a8u, handle, RAM + 256u, 17888u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f.bus, RAM, (const uint8_t *)"actitmln/unknown.bin", 20u, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, call(&f, 0x920b4u, RAM, 2u, 0u));
    SEMU_TEST_EQ_U64(context, 2u, f.layer.hits);
    SEMU_TEST_EQ_U64(context, 1u, semu_sapporo_239_files_count(f.files));
    for (day = 0u; day < 144u; ++day) {
        for (record = 0u; record < 248u; ++record) {
            uint32_t offset = 32u + record * 72u;
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                call(&f, 0x92182u, handle, offset, 0u));
            SEMU_TEST_EQ_U64(context, offset, f.cpu.r[0]);
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                call(&f, 0x921dcu, handle, RAM + 32768u, 72u));
            SEMU_TEST_EQ_U64(context, 72u, f.cpu.r[0]);
        }
    }
    SEMU_TEST_EQ_U64(context, 71426u, f.layer.hits);
    SEMU_TEST_EQ_U64(context, LIMIT,
        f.layer.descriptor->interventions[SEMU_SAPPORO_239_IV_LOGICAL_FILE].max_hits);
    SEMU_TEST_EQ_U64(context, LIMIT + 3u, f.layer.descriptor->maximum_hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x92182u, handle, 32u, 0u));
    while (f.layer.hits < LIMIT - 1u)
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x9221au, handle, 0u, 0u));
    semu_log_init(&f.logger, log, SEMU_LOG_INFO);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        call(&f, 0x921dcu, handle, RAM + 32768u, 72u));
    SEMU_TEST_EQ_U64(context, 72u, f.cpu.r[0]);
    SEMU_TEST_EQ_U64(context, LIMIT, f.layer.hits);
    memset(before_ram, 0x5a, sizeof(before_ram));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f.bus, RAM + 32768u, before_ram, sizeof(before_ram), &f.error));
    semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f.files, &before, &f.error));
    f.cpu.r[0] = handle; f.cpu.r[1] = RAM + 32768u; f.cpu.r[2] = 72u;
    f.cpu.r[15] = 0x921dcu; expected = f.cpu;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        call(&f, 0x921dcu, handle, RAM + 32768u, 72u));
    SEMU_TEST_ASSERT(context, strstr(f.error.text, "exceeded budget") != NULL);
    SEMU_TEST_ASSERT(context, memcmp(&expected, &f.cpu, sizeof(expected)) == 0);
    SEMU_TEST_EQ_U64(context, LIMIT, f.layer.hits);
    SEMU_TEST_EQ_U64(context, LIMIT,
        f.layer.descriptor->interventions[SEMU_SAPPORO_239_IV_LOGICAL_FILE].hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(f.bus, RAM + 32768u, after_ram, sizeof(after_ram), &f.error));
    SEMU_TEST_ASSERT(context, memcmp(before_ram, after_ram, sizeof(before_ram)) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f.files, &after, &f.error));
    SEMU_TEST_EQ_U64(context, before.size, after.size);
    SEMU_TEST_ASSERT(context, memcmp(before.data, after.data, before.size) == 0);
    rewind(log);
    while (fgets(line, sizeof(line), log) != NULL)
        if (strstr(line, "trigger=logical-file ordinal=76258") != NULL) ++events;
    SEMU_TEST_EQ_U64(context, 1u, events);
    semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
    semu_sapporo_239_files_destroy(f.files); semu_bus_destroy(f.bus);
    (void)fclose(log);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_history_scan_budget_and_atomic_refusal)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
