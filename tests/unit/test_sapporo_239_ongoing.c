#include "test.h"
#include "../../src/compat/sapporo_239.h"
#include "../../src/compat/sapporo_239_files.h"
#include <string.h>

#define RAM UINT32_C(0x10000000)
#define DATA (RAM + 256u)

typedef struct fixture {
    semu_bus *bus;
    semu_sapporo_239_files *files;
    semu_layer_state layer;
    semu_cpu_state cpu;
    semu_logger logger;
    semu_error error;
} fixture;

static int init(fixture *f)
{
    memset(f, 0, sizeof(*f));
    f->bus = semu_bus_create(&f->error);
    f->files = semu_sapporo_239_files_create(&f->error);
    semu_log_init(&f->logger, NULL, SEMU_LOG_ERROR);
    return f->bus != NULL && f->files != NULL &&
        semu_bus_map_ram(f->bus, "ram", RAM, 4096u, &f->error) == SEMU_OK &&
        semu_layer_enable_checked(&f->layer, &semu_sapporo_239_wbsto_layer,
            "sapporo-2.39.20", semu_sapporo_239_wbsto_layer.component_hashes,
            3u, &f->error) == SEMU_OK;
}

static void destroy(fixture *f)
{
    semu_sapporo_239_files_destroy(f->files);
    semu_bus_destroy(f->bus);
}

static semu_status call(fixture *f, uint32_t pc, uint32_t a,
                         uint32_t b, uint32_t c)
{
    f->cpu.r[0] = a; f->cpu.r[1] = b; f->cpu.r[2] = c;
    f->cpu.r[14] = 0x80001u; f->cpu.r[15] = pc;
    return semu_sapporo_239_apply_file_hook(f->files, f->bus, &f->cpu,
        &f->layer, &f->logger, &f->error);
}

static semu_status open_path(fixture *f, const char *path, uint32_t mode)
{
    if (semu_bus_load(f->bus, RAM, (const uint8_t *)path, strlen(path) + 1u,
            &f->error) != SEMU_OK) return f->error.code;
    return call(f, 0x920b4u, RAM, mode, 0u);
}

static void test_ongoing_capacity_and_lifecycle(semu_test_context *context)
{
    fixture f;
    uint32_t handle, i;
    uint8_t payload[152], actual[152];
    semu_snapshot_writer before, after;
    semu_cpu_state expected;
    uint64_t hits;
    SEMU_TEST_ASSERT(context, init(&f));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        open_path(&f, "actitmln/ongoing.bin", 1u));
    SEMU_TEST_EQ_U64(context, 0x920b4u, f.cpu.r[15]); /* Missing: native path. */
    SEMU_TEST_EQ_U64(context, 0u, f.layer.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        open_path(&f, "actitmln/ongoing.bin", 2u));
    handle = f.cpu.r[0];
    /* Deliberately synthetic payload, not native header/record bytes. */
    for (i = 0u; i < sizeof(payload); ++i) payload[i] = (uint8_t)(i ^ 0x5au);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f.bus, DATA, payload, sizeof(payload), &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x921a8u, handle, DATA, 24u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x921a8u, handle, DATA + 24u, 8u));
    for (i = 0u; i < 3u; ++i)
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            call(&f, 0x921a8u, handle, DATA + 32u + 40u * i, 40u));
    SEMU_TEST_EQ_U64(context, 152u,
        semu_sapporo_239_file_size(f.files, "actitmln/ongoing.bin"));
    semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f.files, &before, &f.error));
    hits = f.layer.hits;
    f.cpu.r[0] = handle; f.cpu.r[1] = DATA; f.cpu.r[2] = 1u;
    f.cpu.r[15] = 0x921a8u; expected = f.cpu;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, call(&f, 0x921a8u, handle, DATA, 1u));
    SEMU_TEST_ASSERT(context, strstr(f.error.text, "exceeds capacity") != NULL);
    SEMU_TEST_ASSERT(context, memcmp(&expected, &f.cpu, sizeof(expected)) == 0);
    SEMU_TEST_EQ_U64(context, hits, f.layer.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f.files, &after, &f.error));
    SEMU_TEST_EQ_U64(context, before.size, after.size);
    SEMU_TEST_ASSERT(context, memcmp(before.data, after.data, before.size) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920f4u, handle, 0u, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        open_path(&f, "actitmln/ongoing.bin", 2u));
    SEMU_TEST_ASSERT(context, handle != f.cpu.r[0]);
    handle = f.cpu.r[0];
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x92244u, handle, 0u, 0u));
    SEMU_TEST_EQ_U64(context, 152u, f.cpu.r[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        call(&f, 0x921dcu, handle, DATA + 512u, 152u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(f.bus, DATA + 512u, actual, sizeof(actual), &f.error));
    SEMU_TEST_ASSERT(context, memcmp(payload, actual, sizeof(actual)) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        open_path(&f, "actitmln/unknown.bin", 2u));
    SEMU_TEST_EQ_U64(context, 1u, semu_sapporo_239_files_count(f.files));
    hits = f.layer.hits;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        open_path(&f, "actitmln/ongoing.bin", 9u));
    SEMU_TEST_ASSERT(context, strstr(f.error.text, "unknown Sapporo 2.39 file open mode") != NULL);
    SEMU_TEST_EQ_U64(context, 0x920b4u, f.cpu.r[15]);
    SEMU_TEST_EQ_U64(context, hits, f.layer.hits);
    SEMU_TEST_EQ_U64(context, 152u,
        semu_sapporo_239_file_size(f.files, "actitmln/ongoing.bin"));
    semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
    destroy(&f);
}

static void test_ongoing_snapshot_versions_and_atomic_refusal(
    semu_test_context *context)
{
    fixture f;
    semu_sapporo_239_files *restored;
    semu_snapshot_writer legacy, current, roundtrip;
    semu_snapshot_reader reader;
    uint32_t handle;
    size_t i;
    /* Count, presence, size, handle file index and cursor corruptions. */
    static const size_t offsets[] = {8u, 8u, 8u, 8u, 27u, 28u, 193u, 189u};
    static const uint8_t values[] = {0u, 10u, 11u, 13u, 2u, 153u, 12u, 153u};
    SEMU_TEST_ASSERT(context, init(&f));
    restored = semu_sapporo_239_files_create(&f.error);
    SEMU_TEST_ASSERT(context, restored != NULL);
    semu_snapshot_writer_init(&legacy); semu_snapshot_writer_init(&current);
    semu_snapshot_writer_init(&roundtrip);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f.files, &legacy, &f.error));
    SEMU_TEST_EQ_U64(context, 91u, legacy.size);
    SEMU_TEST_EQ_U64(context, 11u, legacy.data[8]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        open_path(&f, "actitmln/ongoing.bin", 2u));
    handle = f.cpu.r[0];
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x921a8u, handle, DATA, 152u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f.files, &current, &f.error));
    SEMU_TEST_EQ_U64(context, 12u, current.data[8]);
    semu_snapshot_reader_init(&reader, current.data, current.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_read(restored, &reader, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(restored, &roundtrip, &f.error));
    SEMU_TEST_EQ_U64(context, current.size, roundtrip.size);
    SEMU_TEST_ASSERT(context, memcmp(current.data, roundtrip.data, current.size) == 0);
    for (i = 0u; i <= SEMU_ARRAY_LEN(offsets); ++i) {
        uint8_t saved = 0u;
        if (i < SEMU_ARRAY_LEN(offsets)) {
            saved = current.data[offsets[i]];
            current.data[offsets[i]] = values[i];
        }
        semu_snapshot_reader_init(&reader, current.data,
            current.size - (i == SEMU_ARRAY_LEN(offsets) ? 1u : 0u));
        SEMU_TEST_ASSERT(context,
            semu_sapporo_239_files_snapshot_read(restored, &reader, &f.error) != SEMU_OK);
        if (i < SEMU_ARRAY_LEN(offsets)) current.data[offsets[i]] = saved;
        semu_snapshot_writer_destroy(&roundtrip);
        semu_snapshot_writer_init(&roundtrip);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_239_files_snapshot_write(restored, &roundtrip, &f.error));
        SEMU_TEST_EQ_U64(context, current.size, roundtrip.size);
        SEMU_TEST_ASSERT(context, memcmp(current.data, roundtrip.data, current.size) == 0);
    }
    /* Loading an old snapshot clears the appended file and keeps old bytes. */
    semu_snapshot_reader_init(&reader, legacy.data, legacy.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_read(restored, &reader, &f.error));
    SEMU_TEST_EQ_U64(context, SIZE_MAX,
        semu_sapporo_239_file_size(restored, "actitmln/ongoing.bin"));
    semu_snapshot_writer_destroy(&roundtrip); semu_snapshot_writer_init(&roundtrip);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(restored, &roundtrip, &f.error));
    SEMU_TEST_EQ_U64(context, legacy.size, roundtrip.size);
    SEMU_TEST_ASSERT(context, memcmp(legacy.data, roundtrip.data, legacy.size) == 0);
    semu_snapshot_writer_destroy(&legacy); semu_snapshot_writer_destroy(&current);
    semu_snapshot_writer_destroy(&roundtrip);
    semu_sapporo_239_files_destroy(restored);
    destroy(&f);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_ongoing_capacity_and_lifecycle),
        SEMU_TEST_CASE(test_ongoing_snapshot_versions_and_atomic_refusal)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
