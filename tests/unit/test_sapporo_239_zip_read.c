#include "test.h"
#include "../../src/compat/sapporo_239.h"
#include "../../src/compat/sapporo_239_files.h"
#include <stdio.h>
#include <string.h>

#define RAM UINT32_C(0x10000000)

static void test_zip_read_exact_passthrough_and_refusals(semu_test_context *context)
{
    static const struct {
        const char *path;
        uint32_t mode;
        semu_status status;
    } cases[] = {
        {"zapp/zwspee01.zip", 9u, SEMU_OK},
        {"ZAPP/ZWSPEE01.ZIP", 9u, SEMU_OK},
        {"zapp/zwspee01.zip", 10u, SEMU_ERR_STATE},
        {"zapp/zwspee01.zip", 11u, SEMU_ERR_STATE},
        {"zapp/zwspee01.zip", 25u, SEMU_ERR_STATE},
        {"zapp/zwspee01.zip", 0u, SEMU_ERR_STATE},
        {"zapp/zwspee01.zip", 4u, SEMU_ERR_STATE},
        {"zapp/zwspee01.zip", 2u, SEMU_ERR_STATE},
        /* Ticket 751: native reads are no longer ZIP-filename-specific. */
        {"zapp/zwwatc01.wfa", 9u, SEMU_OK},
        {"ui/js/config.js", 9u, SEMU_OK},
        {"actitmln/ongoing.bin", 9u, SEMU_ERR_STATE},
        {"zapp/zwspee01.zip/extra", 9u, SEMU_OK},
        {"zapp/../zapp/zwspee01.zip", 9u, SEMU_OK},
        {"zapp/zwspee01.zip!", 9u, SEMU_ERR_STATE}
    };
    semu_error error = {0};
    semu_bus *bus = semu_bus_create(&error);
    semu_sapporo_239_files *files = semu_sapporo_239_files_create(&error);
    semu_layer_state layer;
    semu_cpu_state cpu, before_cpu;
    semu_logger logger;
    semu_snapshot_writer before, after;
    uint8_t before_ram[64], after_ram[64];
    FILE *log = tmpfile();
    size_t i;
    uint64_t limit;
    SEMU_TEST_ASSERT(context, bus != NULL && files != NULL && log != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_map_ram(bus, "ram", RAM, 4096u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_layer_enable_checked(&layer, &semu_sapporo_239_wbsto_layer,
            "sapporo-2.39.20", semu_sapporo_239_wbsto_layer.component_hashes,
            3u, &error));
    semu_log_init(&logger, log, SEMU_LOG_INFO);
    limit = layer.descriptor->interventions[SEMU_SAPPORO_239_IV_LOGICAL_FILE].max_hits;
    ((semu_layer_intervention *)&layer.descriptor->interventions[
        SEMU_SAPPORO_239_IV_LOGICAL_FILE])->hits = limit;
    layer.hits = limit; /* Native execution consumes no synthetic budget. */
    semu_snapshot_writer_init(&before);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(files, &before, &error));
    for (i = 0u; i < SEMU_ARRAY_LEN(cases) + 3u; ++i) {
        const char *path = i < SEMU_ARRAY_LEN(cases) ? cases[i].path : cases[0].path;
        uint32_t mode = i < SEMU_ARRAY_LEN(cases) ? cases[i].mode : 9u;
        semu_status expected = i < SEMU_ARRAY_LEN(cases) ? cases[i].status : SEMU_ERR_STATE;
        memset(before_ram, 0, sizeof(before_ram));
        memcpy(before_ram, path, strlen(path));
        if (i == SEMU_ARRAY_LEN(cases) + 1u) memset(before_ram, 'a', sizeof(before_ram));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_load(bus, RAM, before_ram, sizeof(before_ram), &error));
        memset(&cpu, 0x5a, sizeof(cpu));
        cpu.r[0] = RAM; cpu.r[1] = mode;
        cpu.r[14] = 0x843e9u; cpu.r[15] = 0x920b4u;
        if (i == SEMU_ARRAY_LEN(cases)) layer.enabled = 0;
        if (i == SEMU_ARRAY_LEN(cases) + 2u) cpu.r[0] = UINT32_MAX - 10u;
        before_cpu = cpu;
        SEMU_TEST_EQ_U64(context, expected,
            semu_sapporo_239_apply_file_hook(files, bus, &cpu, &layer, &logger, &error));
        layer.enabled = 1;
        SEMU_TEST_ASSERT(context, memcmp(&before_cpu, &cpu, sizeof(cpu)) == 0);
        SEMU_TEST_EQ_U64(context, limit, layer.hits);
        SEMU_TEST_EQ_U64(context, limit,
            layer.descriptor->interventions[SEMU_SAPPORO_239_IV_LOGICAL_FILE].hits);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_copy_out(bus, RAM, after_ram, sizeof(after_ram), &error));
        SEMU_TEST_ASSERT(context, memcmp(before_ram, after_ram, sizeof(before_ram)) == 0);
        semu_snapshot_writer_init(&after);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_239_files_snapshot_write(files, &after, &error));
        SEMU_TEST_EQ_U64(context, before.size, after.size);
        SEMU_TEST_ASSERT(context, memcmp(before.data, after.data, before.size) == 0);
        semu_snapshot_writer_destroy(&after);
    }
    SEMU_TEST_EQ_U64(context, 0u, semu_sapporo_239_files_count(files));
    rewind(log);
    SEMU_TEST_ASSERT(context, fgetc(log) == EOF);
    semu_snapshot_writer_destroy(&before);
    semu_sapporo_239_files_destroy(files); semu_bus_destroy(bus);
    (void)fclose(log);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_zip_read_exact_passthrough_and_refusals)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
