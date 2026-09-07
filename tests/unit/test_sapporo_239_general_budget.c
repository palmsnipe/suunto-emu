#include "test.h"
#include "../../src/compat/sapporo_239.h"
#include "../../src/compat/sapporo_239_files.h"
#include <stdio.h>
#include <string.h>

#define RAM UINT32_C(0x10000000)
#define OLD_LIMIT UINT64_C(76279)
#define LIMIT UINT64_C(76371)

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

static void round_trip(semu_test_context *context, fixture *f)
{
    semu_snapshot_writer before, after;
    semu_snapshot_reader reader;
    semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f->files, &before, &f->error));
    semu_snapshot_reader_init(&reader, before.data, before.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_read(f->files, &reader, &f->error));
    SEMU_TEST_ASSERT(context, semu_snapshot_reader_done(&reader));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f->files, &after, &f->error));
    SEMU_TEST_EQ_U64(context, before.size, after.size);
    SEMU_TEST_ASSERT(context, memcmp(before.data, after.data, before.size) == 0);
    semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
}

static void assert_refusal(semu_test_context *context, fixture *f,
                            uint32_t mode, const char *message, FILE *log)
{
    semu_snapshot_writer before, after;
    semu_cpu_state cpu;
    uint8_t ram_before[2048], ram_after[2048];
    uint64_t hits = f->layer.hits;
    uint64_t file_hits = f->layer.descriptor->interventions[2].hits;
    long position = ftell(log);
    semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
    f->cpu.r[0] = RAM; f->cpu.r[1] = mode; f->cpu.r[2] = 0u;
    f->cpu.r[14] = 0x80001u; f->cpu.r[15] = 0x920b4u; cpu = f->cpu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f->files, &before, &f->error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(f->bus, RAM, ram_before, sizeof(ram_before), &f->error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, call(f, 0x920b4u, RAM, mode, 0u));
    SEMU_TEST_ASSERT(context, strstr(f->error.text, message) != NULL);
    SEMU_TEST_ASSERT(context, memcmp(&cpu, &f->cpu, sizeof(cpu)) == 0);
    SEMU_TEST_EQ_U64(context, hits, f->layer.hits);
    SEMU_TEST_EQ_U64(context, file_hits, f->layer.descriptor->interventions[2].hits);
    SEMU_TEST_ASSERT(context, position >= 0 && ftell(log) == position);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f->files, &after, &f->error));
    SEMU_TEST_EQ_U64(context, before.size, after.size);
    SEMU_TEST_ASSERT(context, memcmp(before.data, after.data, before.size) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(f->bus, RAM, ram_after, sizeof(ram_after), &f->error));
    SEMU_TEST_ASSERT(context, memcmp(ram_before, ram_after, sizeof(ram_before)) == 0);
    semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
}

static void test_native_general_save_budget(semu_test_context *context)
{
    /* E-SAP-COMPAT-GENERAL-239-001: metadata only; all payloads synthetic. */
    static const uint8_t sizes[] = {
        9,11,19,19,11,17,19,15,16,21,25,16,24,24,24,22,19,20,23,17,19,13,20,15,
        15,2,16,2,16,2,16,2,13,2,18,2,16,2,14,2,20,18,28,22,18,20,20,20,20,
        20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,19,14,16,
        17,18,18,22,21,16,15,14,16,16,11,14,15,19,14,14,15,17
    };
    fixture f = {0};
    FILE *log = tmpfile();
    uint8_t payload[1505], actual[1505], present;
    uint32_t handle, word;
    size_t i, offset = 0u;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    f.bus = semu_bus_create(&f.error);
    f.files = semu_sapporo_239_files_create(&f.error);
    SEMU_TEST_ASSERT(context, log && f.bus && f.files);
    semu_log_init(&f.logger, log, SEMU_LOG_INFO);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_map_ram(f.bus, "ram", RAM, 4096u, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_layer_enable_checked(&f.layer, &semu_sapporo_239_wbsto_layer,
            "sapporo-2.39.20", semu_sapporo_239_wbsto_layer.component_hashes,
            3u, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f.bus, RAM, (const uint8_t *)"unknown.bin", 12u, &f.error));
    assert_refusal(context, &f, 2u, "writable file path", log);
    assert_refusal(context, &f, 10u, "open mode", log);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f.bus, RAM, (const uint8_t *)"settings/general", 17u, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920b4u, RAM, 2u, 0u));
    handle = f.cpu.r[0];
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        call(&f, 0x921a8u, handle, RAM + 256u, sizeof(payload)));
    semu_log_init(&f.logger, NULL, SEMU_LOG_ERROR);
    while (f.layer.hits < OLD_LIMIT - 1u)
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x9221au, handle, 0u, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920f4u, handle, 0u, 0u));
    semu_log_init(&f.logger, log, SEMU_LOG_INFO);
    for (i = 0u; i < sizeof(payload); ++i) payload[i] = (uint8_t)(i * 17u + 3u);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f.bus, RAM + 256u, payload, sizeof(payload), &f.error));
    /* This open is the narrow failing-before regression. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920b4u, RAM, 2u, 0u));
    handle = f.cpu.r[0];
    SEMU_TEST_EQ_U64(context, 90u, SEMU_ARRAY_LEN(sizes));
    for (i = 0u; i < SEMU_ARRAY_LEN(sizes); ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            call(&f, 0x921a8u, handle, RAM + 256u + (uint32_t)offset, sizes[i]));
        SEMU_TEST_EQ_U64(context, sizes[i], f.cpu.r[0]);
        offset += sizes[i];
        if (i == 39u) round_trip(context, &f);
    }
    SEMU_TEST_EQ_U64(context, sizeof(payload), offset);
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920f4u, handle, 0u, 0u));
    SEMU_TEST_EQ_U64(context, 1u, f.cpu.r[0]);
    SEMU_TEST_EQ_U64(context, LIMIT, f.layer.hits);
    SEMU_TEST_EQ_U64(context, LIMIT, f.layer.descriptor->interventions[2].max_hits);
    SEMU_TEST_EQ_U64(context, LIMIT + 3u, f.layer.descriptor->maximum_hits);
    SEMU_TEST_EQ_U64(context, sizeof(payload),
        semu_sapporo_239_file_size(f.files, "settings/general"));
    /* Inspect the explicit file codec without consuming another file hit. */
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f.files, &writer, &f.error));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    for (i = 0u; i < 4u; ++i)
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_snapshot_reader_u32(&reader, &word, &f.error));
    for (i = 0u; i < 3u; ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_snapshot_reader_u8(&reader, &present, &f.error));
        SEMU_TEST_EQ_U64(context, i == 2u, present);
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_snapshot_reader_u32(&reader, &word, &f.error));
    SEMU_TEST_EQ_U64(context, sizeof(payload), word);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_snapshot_reader_bytes(&reader, actual, sizeof(actual), &f.error));
    SEMU_TEST_ASSERT(context, memcmp(payload, actual, sizeof(payload)) == 0);
    semu_snapshot_writer_destroy(&writer);
    assert_refusal(context, &f, 2u, "exceeded budget", log);
    semu_sapporo_239_files_destroy(f.files); semu_bus_destroy(f.bus); fclose(log);
}

int main(void)
{
    static const semu_test_case cases[] = { SEMU_TEST_CASE(test_native_general_save_budget) };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
