#include "test.h"
#include "../../src/compat/sapporo_239.h"
#include "../../src/compat/sapporo_239_files.h"
#include <stdio.h>
#include <string.h>

#define RAM UINT32_C(0x10000000)
#define OLD_LIMIT UINT64_C(76371)
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

static void check_contents(semu_test_context *context, fixture *f,
    unsigned selected, const uint8_t *expected, size_t length)
{
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    uint32_t word;
    uint8_t present, actual[1727];
    unsigned i;
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f->files, &writer, &f->error));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    for (i = 0u; i < 4u; ++i)
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_snapshot_reader_u32(&reader, &word, &f->error));
    for (i = 0u; i < 11u; ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_snapshot_reader_u8(&reader, &present, &f->error));
        SEMU_TEST_EQ_U64(context, i == 2u || i == 9u, present);
        if (!present) continue;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_snapshot_reader_u32(&reader, &word, &f->error));
        SEMU_TEST_EQ_U64(context, i == 2u ? 1505u : 1727u, word);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_snapshot_reader_bytes(&reader, actual, word, &f->error));
        if (i == selected) {
            SEMU_TEST_EQ_U64(context, length, word);
            SEMU_TEST_ASSERT(context, memcmp(actual, expected, length) == 0);
        }
    }
    semu_snapshot_writer_destroy(&writer);
}

static void test_native_personal_save_budget(semu_test_context *context)
{
    /* E-SAP-COMPAT-PERSONAL-239-001; metadata only, payloads synthetic. */
    static const uint8_t personal[] = {
        10,2,10,16,17,17,17,19,11,10,17,20,25,21,25,21,25,24,22,23,22,
        32,32,32,32,28,28,28,28,28,28,28,28,34,34,34,34,30,30,30,30,
        30,30,30,30,34,34,34,34,30,30,30,30,30,30,30,30,29,25,25,31,
        27,27,31,27,27
    };
    static const uint8_t general[] = {
        9,11,19,19,11,17,19,15,16,21,25,16,24,24,24,22,19,20,23,17,19,13,20,15,
        15,2,16,2,16,2,16,2,13,2,18,2,16,2,14,2,20,18,28,22,18,20,20,20,20,
        20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,19,14,16,
        17,18,18,22,21,16,15,14,16,16,11,14,15,19,14,14,15,17
    };
    static const char *const paths[] = {"settings/personal", "settings/general"};
    fixture f = {0};
    FILE *log = tmpfile();
    uint8_t payload[1727];
    uint32_t handle;
    size_t i, pass, total = 0u;
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
        semu_bus_load(f.bus, RAM, (const uint8_t *)"settings/time", 14u, &f.error));
    assert_refusal(context, &f, 2u, "writable file path", log);
    assert_refusal(context, &f, 10u, "open mode", log);
    for (pass = 0u; pass < 2u; ++pass) {
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_load(f.bus, RAM,
            (const uint8_t *)paths[pass], strlen(paths[pass]) + 1u, &f.error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920b4u, RAM, 2u, 0u));
        handle = f.cpu.r[0];
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            call(&f, 0x921a8u, handle, RAM + 256u, pass ? 1505u : 1727u));
        if (pass == 1u) {
            semu_log_init(&f.logger, NULL, SEMU_LOG_ERROR);
            while (f.layer.hits < OLD_LIMIT - 1u)
                SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x9221au, handle, 0u, 0u));
        }
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920f4u, handle, 0u, 0u));
    }
    semu_log_init(&f.logger, log, SEMU_LOG_INFO);
    SEMU_TEST_EQ_U64(context, 66u, SEMU_ARRAY_LEN(personal));
    SEMU_TEST_EQ_U64(context, 90u, SEMU_ARRAY_LEN(general));
    for (pass = 0u; pass < 3u; ++pass) {
        const uint8_t *sizes = pass < 2u ? personal : general;
        size_t count = pass < 2u ? sizeof(personal) : sizeof(general);
        size_t length = pass < 2u ? 1727u : 1505u, offset = 0u;
        const char *path = paths[pass == 2u];
        for (i = 0u; i < length; ++i) payload[i] = (uint8_t)(i * 17u + pass + 3u);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_load(f.bus, RAM,
            (const uint8_t *)path, strlen(path) + 1u, &f.error));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_load(f.bus, RAM + 256u, payload, length, &f.error));
        /* First post-76,371 open must fail on the old production ceiling. */
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920b4u, RAM, 2u, 0u));
        handle = f.cpu.r[0];
        for (i = 0u; i < count; ++i) {
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                call(&f, 0x921a8u, handle, RAM + 256u + (uint32_t)offset, sizes[i]));
            SEMU_TEST_EQ_U64(context, sizes[i], f.cpu.r[0]);
            offset += sizes[i];
            if (pass == 0u && i == 32u) round_trip(context, &f);
        }
        SEMU_TEST_EQ_U64(context, length, offset);
        total += offset;
        SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920f4u, handle, 0u, 0u));
        SEMU_TEST_EQ_U64(context, 1u, f.cpu.r[0]);
        SEMU_TEST_EQ_U64(context, OLD_LIMIT + (pass < 2u ? 68u * (pass + 1u) : 228u), f.layer.hits);
        check_contents(context, &f, pass < 2u ? 9u : 2u, payload, length);
    }
    SEMU_TEST_EQ_U64(context, 4959u, total);
    SEMU_TEST_EQ_U64(context, LIMIT, f.layer.descriptor->interventions[2].max_hits);
    SEMU_TEST_EQ_U64(context, LIMIT + 3u, f.layer.descriptor->maximum_hits);
    assert_refusal(context, &f, 2u, "exceeded budget", log);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f.bus, RAM, (const uint8_t *)"settings/time", 14u, &f.error));
    assert_refusal(context, &f, 2u, "writable file path", log);
    semu_sapporo_239_files_destroy(f.files); semu_bus_destroy(f.bus); fclose(log);
}

int main(void)
{
    static const semu_test_case cases[] = { SEMU_TEST_CASE(test_native_personal_save_budget) };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
