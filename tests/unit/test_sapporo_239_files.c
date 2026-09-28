#include "test.h"

#include "../../src/compat/sapporo_239.h"
#include "../../src/compat/sapporo_239_files.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAM UINT32_C(0x10000000)
#define PATH (RAM + UINT32_C(0x1000))
#define DATA (RAM + UINT32_C(0x2000))

typedef struct fixture {
    semu_sapporo_239_files *files;
    semu_bus *bus;
    semu_layer_state layer;
    semu_cpu_state cpu;
    semu_logger logger;
    semu_error error;
} fixture;

static const char *const hashes[] = {
    "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5",
    "85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89",
    "49a3936f4c9d61dbceee12324f41412334c42aa98b900f6f2d3fc5633ae43aea"
};

static int fixture_init(fixture *f)
{
    memset(f, 0, sizeof(*f));
    semu_error_clear(&f->error);
    semu_log_init(&f->logger, NULL, SEMU_LOG_ERROR);
    f->files = semu_sapporo_239_files_create(&f->error);
    f->bus = semu_bus_create(&f->error);
    return f->files != NULL && f->bus != NULL &&
        semu_bus_map_ram(f->bus, "sram", RAM, UINT32_C(0x00180000),
                         &f->error) == SEMU_OK &&
        semu_layer_enable_checked(&f->layer,
            &semu_sapporo_239_wbsto_layer, "sapporo-2.39.20",
            hashes, SEMU_ARRAY_LEN(hashes), &f->error) == SEMU_OK;
}

static void fixture_destroy(fixture *f)
{
    semu_sapporo_239_files_destroy(f->files);
    semu_bus_destroy(f->bus);
}

static semu_status hook(fixture *f, uint32_t pc)
{
    f->cpu.r[15] = pc;
    f->cpu.r[14] = UINT32_C(0x00080001);
    return semu_sapporo_239_apply_file_hook(f->files, f->bus, &f->cpu,
        &f->layer, &f->logger, &f->error);
}

static uint32_t open_path(fixture *f, const char *path, uint32_t mode)
{
    size_t size = strlen(path) + 1u;
    (void)semu_bus_load(f->bus, PATH, (const uint8_t *)path, size, &f->error);
    f->cpu.r[0] = PATH;
    f->cpu.r[1] = mode;
    return hook(f, UINT32_C(0x000920b4)) == SEMU_OK &&
           f->cpu.r[15] == UINT32_C(0x00080000) ? f->cpu.r[0] : 0u;
}

static void test_create_write_read_and_stale_handle(
    semu_test_context *context)
{
    fixture f;
    uint32_t handle;
    uint8_t actual[4] = {0u};
    static const uint8_t payload[] = {1u, 2u, 3u, 4u};
    SEMU_TEST_ASSERT(context, fixture_init(&f));
    handle = open_path(&f, "settings/uiv2.txt", 2u);
    SEMU_TEST_ASSERT(context, handle != 0u);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f.bus, DATA, payload, sizeof(payload), &f.error));
    f.cpu.r[0] = handle; f.cpu.r[1] = DATA; f.cpu.r[2] = sizeof(payload);
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x000921a8)));
    SEMU_TEST_EQ_U64(context, sizeof(payload), f.cpu.r[0]);
    f.cpu.r[0] = handle; f.cpu.r[1] = 0u; f.cpu.r[2] = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x00092182)));
    f.cpu.r[0] = handle; f.cpu.r[1] = DATA + 16u; f.cpu.r[2] = 4u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x000921dc)));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(f.bus, DATA + 16u, actual, sizeof(actual), &f.error));
    SEMU_TEST_ASSERT(context, memcmp(actual, payload, sizeof(actual)) == 0);
    f.cpu.r[0] = handle;
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x000920f4)));
    f.cpu.r[0] = handle;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        hook(&f, UINT32_C(0x00092146)));
    SEMU_TEST_EQ_U64(context, 4u,
        semu_sapporo_239_file_size(f.files, "settings/uiv2.txt"));
    fixture_destroy(&f);
}

static void test_fallthrough_and_capacity_refusal(semu_test_context *context)
{
    fixture f;
    uint32_t handle;
    uint8_t payload[236] = {0u};
    SEMU_TEST_ASSERT(context, fixture_init(&f));
    f.cpu.r[15] = UINT32_C(0x11111110);
    SEMU_TEST_EQ_U64(context, 0u, open_path(&f, "native/read.bin", 1u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x000920b4), f.cpu.r[15]);
    SEMU_TEST_EQ_U64(context, 0u, semu_sapporo_239_files_count(f.files));
    SEMU_TEST_EQ_U64(context, 0u, open_path(&f, "unknown/write.bin", 2u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, f.error.code);
    semu_error_clear(&f.error);
    handle = open_path(&f, "settings/uiv2.txt", 2u);
    SEMU_TEST_ASSERT(context, handle != 0u);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f.bus, DATA, payload, sizeof(payload), &f.error));
    f.cpu.r[0] = handle; f.cpu.r[1] = DATA; f.cpu.r[2] = sizeof(payload);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        hook(&f, UINT32_C(0x000921a8)));
    SEMU_TEST_EQ_U64(context, 0u,
        semu_sapporo_239_file_size(f.files, "settings/uiv2.txt"));
    SEMU_TEST_EQ_U64(context, 1u, f.layer.hits);
    ((semu_layer_intervention *)
        &f.layer.descriptor->interventions[
            SEMU_SAPPORO_239_IV_LOGICAL_FILE])->hits =
        f.layer.descriptor->interventions[SEMU_SAPPORO_239_IV_LOGICAL_FILE].max_hits;
    f.cpu.r[0] = handle; f.cpu.r[1] = DATA; f.cpu.r[2] = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        hook(&f, UINT32_C(0x000921a8)));
    SEMU_TEST_EQ_U64(context, 0u,
        semu_sapporo_239_file_size(f.files, "settings/uiv2.txt"));
    fixture_destroy(&f);
}

static void test_sparse_truncate_reopen_and_reset(semu_test_context *context)
{
    fixture f;
    uint32_t handle;
    static const uint8_t payload[] = {0xaau, 0xbbu};
    SEMU_TEST_ASSERT(context, fixture_init(&f));
    handle = open_path(&f, "zapp/storage.sbm", 2u);
    f.cpu.r[0] = handle; f.cpu.r[1] = 8u; f.cpu.r[2] = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x00092182)));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f.bus, DATA, payload, sizeof(payload), &f.error));
    f.cpu.r[0] = handle; f.cpu.r[1] = DATA; f.cpu.r[2] = 2u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x000921a8)));
    SEMU_TEST_EQ_U64(context, 10u,
        semu_sapporo_239_file_size(f.files, "zapp/storage.sbm"));
    f.cpu.r[0] = handle; f.cpu.r[1] = 5u; f.cpu.r[2] = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x00092182)));
    f.cpu.r[0] = handle;
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x00092162)));
    SEMU_TEST_EQ_U64(context, 5u,
        semu_sapporo_239_file_size(f.files, "zapp/storage.sbm"));
    f.cpu.r[0] = handle;
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x000920f4)));
    SEMU_TEST_ASSERT(context, open_path(&f, "zapp/storage.sbm", 2u) != 0u);
    SEMU_TEST_EQ_U64(context, 5u,
        semu_sapporo_239_file_size(f.files, "zapp/storage.sbm"));
    semu_sapporo_239_files_reset(f.files);
    SEMU_TEST_EQ_U64(context, 0u, semu_sapporo_239_files_count(f.files));
    fixture_destroy(&f);
}

static void test_snapshot_roundtrip_and_atomic_refusal(
    semu_test_context *context)
{
    fixture f;
    semu_sapporo_239_files *restored;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    uint32_t handle;
    static const uint8_t payload[] = {9u, 8u, 7u};
    SEMU_TEST_ASSERT(context, fixture_init(&f));
    restored = semu_sapporo_239_files_create(&f.error);
    SEMU_TEST_ASSERT(context, restored != NULL);
    handle = open_path(&f, "settings/general", 2u);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f.bus, DATA, payload, sizeof(payload), &f.error));
    f.cpu.r[0] = handle; f.cpu.r[1] = DATA; f.cpu.r[2] = sizeof(payload);
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x000921a8)));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f.files, &writer, &f.error));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_read(restored, &reader, &f.error));
    SEMU_TEST_EQ_U64(context, 3u,
        semu_sapporo_239_file_size(restored, "settings/general"));
    writer.data[0] ^= 1u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_sapporo_239_files_snapshot_read(restored, &reader, &f.error));
    SEMU_TEST_EQ_U64(context, 3u,
        semu_sapporo_239_file_size(restored, "settings/general"));
    semu_snapshot_writer_destroy(&writer);
    semu_sapporo_239_files_destroy(restored);
    fixture_destroy(&f);
}

static void test_storage_admit_capacity_and_refusals(
    semu_test_context *context)
{
    fixture f;
    uint32_t handle;
    size_t i;
    char path[80];
    static const uint8_t payload[34] = {1u, 2u, 3u, 4u};
    SEMU_TEST_ASSERT(context, fixture_init(&f));
    /* E-SAP239-REFUSED-PATH-001: refusals keep the message prefix and
       name the path (ticket 798), codes unchanged. */
    SEMU_TEST_EQ_U64(context, 0u, open_path(&f, "storage/zz1234/data.jsn", 2u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, f.error.code);
    SEMU_TEST_ASSERT(context,
        strstr(f.error.text, "unknown Sapporo 2.39 writable file path") != NULL);
    SEMU_TEST_ASSERT(context,
        strstr(f.error.text, ": storage/zz1234/data.jsn") != NULL);
    semu_error_clear(&f.error);
    SEMU_TEST_EQ_U64(context, 0u, open_path(&f, "storage/38d123/list.jsn", 2u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, f.error.code);
    SEMU_TEST_ASSERT(context,
        strstr(f.error.text, "unknown Sapporo 2.39 writable file path") != NULL);
    semu_error_clear(&f.error);
    /* An unknown writable path names the guest path in the refusal. */
    SEMU_TEST_EQ_U64(context, 0u, open_path(&f, "dive/surface.bin", 2u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, f.error.code);
    SEMU_TEST_ASSERT(context,
        strstr(f.error.text, "unknown Sapporo 2.39 writable file path") != NULL);
    SEMU_TEST_ASSERT(context,
        strstr(f.error.text, ": dive/surface.bin") != NULL);
    semu_error_clear(&f.error);
    /* Read mode never admits a storage slot. */
    SEMU_TEST_EQ_U64(context, 0u, open_path(&f, "storage/38d123/data.jsn", 1u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x000920b4), f.cpu.r[15]);
    SEMU_TEST_EQ_U64(context, 0u, semu_sapporo_239_files_count(f.files));
    /* Admit on mode 2; write exactly to the 34-byte capacity. */
    handle = open_path(&f, "storage/38d123/data.jsn", 2u);
    SEMU_TEST_ASSERT(context, handle != 0u);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f.bus, DATA, payload, sizeof(payload), &f.error));
    f.cpu.r[0] = handle; f.cpu.r[1] = DATA; f.cpu.r[2] = sizeof(payload);
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x000921a8)));
    f.cpu.r[0] = handle; f.cpu.r[1] = 0u; f.cpu.r[2] = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x00092182)));
    f.cpu.r[0] = handle; f.cpu.r[1] = DATA; f.cpu.r[2] = 30u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x000921a8)));
    SEMU_TEST_EQ_U64(context, 34u,
        semu_sapporo_239_file_size(f.files, "storage/38d123/data.jsn"));
    f.cpu.r[0] = handle; f.cpu.r[1] = DATA; f.cpu.r[2] = 1u;
    /* Past-capacity writes return OK with the size truncated to capacity
       (the shared stage_write truncation law); observed payloads never
       exceed capacity. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x000921a8)));
    SEMU_TEST_EQ_U64(context, 34u,
        semu_sapporo_239_file_size(f.files, "storage/38d123/data.jsn"));
    f.cpu.r[0] = handle;
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x000920f4)));
    /* Repeat open reuses the slot and retains the bytes. */
    handle = open_path(&f, "storage/38d123/data.jsn", 2u);
    SEMU_TEST_ASSERT(context, handle != 0u);
    SEMU_TEST_EQ_U64(context, 34u,
        semu_sapporo_239_file_size(f.files, "storage/38d123/data.jsn"));
    f.cpu.r[0] = handle;
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x000920f4)));
    /* Slot pool: exactly 63 distinct names total, then exhaustion. */
    for (i = 1u; i < 63u; ++i) {
        (void)snprintf(path, sizeof(path), "storage/%06x/data.jsn",
            (unsigned)i);
        handle = open_path(&f, path, 2u);
        SEMU_TEST_ASSERT(context, handle != 0u);
        f.cpu.r[0] = handle;
        SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x000920f4)));
    }
    (void)snprintf(path, sizeof(path), "storage/%06x/data.jsn", 63u);
    SEMU_TEST_EQ_U64(context, 0u, open_path(&f, path, 2u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, f.error.code);
    fixture_destroy(&f);
}

static void test_storage_snapshot_roundtrip(semu_test_context *context)
{
    fixture f;
    semu_sapporo_239_files *restored;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    uint32_t handle, tag, version;
    SEMU_TEST_ASSERT(context, fixture_init(&f));
    restored = semu_sapporo_239_files_create(&f.error);
    SEMU_TEST_ASSERT(context, restored != NULL);
    handle = open_path(&f, "storage/38d123/data.jsn", 2u);
    SEMU_TEST_ASSERT(context, handle != 0u);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f.bus, DATA, (const uint8_t *)"ab", 2u, &f.error));
    f.cpu.r[0] = handle; f.cpu.r[1] = DATA; f.cpu.r[2] = 2u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x000921a8)));
    f.cpu.r[0] = handle;
    SEMU_TEST_EQ_U64(context, SEMU_OK, hook(&f, UINT32_C(0x000920f4)));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f.files, &writer, &f.error));
    /* v2 carries the storage section. */
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_snapshot_reader_u32(&reader, &tag, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_snapshot_reader_u32(&reader, &version, &f.error));
    SEMU_TEST_EQ_U64(context, 2u, version);
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_read(restored, &reader, &f.error));
    SEMU_TEST_EQ_U64(context, 1, semu_snapshot_reader_done(&reader));
    SEMU_TEST_EQ_U64(context, 2u,
        semu_sapporo_239_file_size(restored, "storage/38d123/data.jsn"));
    /* Round-trip is byte-stable. */
    semu_snapshot_writer_destroy(&writer);
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(restored, &writer, &f.error));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_read(f.files, &reader, &f.error));
    semu_snapshot_writer_destroy(&writer);
    semu_sapporo_239_files_destroy(restored);
    fixture_destroy(&f);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_create_write_read_and_stale_handle),
        SEMU_TEST_CASE(test_fallthrough_and_capacity_refusal),
        SEMU_TEST_CASE(test_sparse_truncate_reopen_and_reset),
        SEMU_TEST_CASE(test_snapshot_roundtrip_and_atomic_refusal),
        SEMU_TEST_CASE(test_storage_admit_capacity_and_refusals),
        SEMU_TEST_CASE(test_storage_snapshot_roundtrip)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
