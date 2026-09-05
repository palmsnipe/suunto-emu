#include "test.h"
#include "../../src/compat/sapporo_239.h"
#include "../../src/compat/sapporo_239_files.h"
#include <stdio.h>
#include <string.h>

#define RAM UINT32_C(0x10000000)
#define SEEK_PC UINT32_C(0x00092182)

typedef struct fixture {
    semu_sapporo_239_files *files;
    semu_bus *bus;
    semu_layer_state layer;
    semu_cpu_state cpu;
    semu_logger logger;
    semu_error error;
    FILE *log;
} fixture;

static int init(fixture *f)
{
    static const char *const hashes[] = {
        "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5",
        "85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89",
        "49a3936f4c9d61dbceee12324f41412334c42aa98b900f6f2d3fc5633ae43aea"
    };
    memset(f, 0, sizeof(*f));
    f->log = tmpfile();
    semu_log_init(&f->logger, f->log, SEMU_LOG_INFO);
    f->files = semu_sapporo_239_files_create(&f->error);
    f->bus = semu_bus_create(&f->error);
    return f->log && f->files && f->bus &&
        semu_bus_map_ram(f->bus, "ram", RAM, 4096u, &f->error) == SEMU_OK &&
        semu_layer_enable_checked(&f->layer, &semu_sapporo_239_wbsto_layer,
            "sapporo-2.39.20", hashes, 3u, &f->error) == SEMU_OK;
}

static void destroy(fixture *f)
{
    semu_sapporo_239_files_destroy(f->files);
    semu_bus_destroy(f->bus);
    fclose(f->log);
}

static semu_status call(fixture *f, uint32_t pc, uint32_t a, uint32_t b,
                         uint32_t c)
{
    f->cpu.r[0] = a; f->cpu.r[1] = b; f->cpu.r[2] = c;
    f->cpu.r[15] = pc; f->cpu.r[14] = UINT32_C(0x80001);
    return semu_sapporo_239_apply_file_hook(f->files, f->bus, &f->cpu,
        &f->layer, &f->logger, &f->error);
}

static void setup_file(semu_test_context *context, fixture *f, uint32_t *handle)
{
    static const uint8_t path[] = "tssln/tss.bin";
    uint8_t data[64];
    size_t i;
    for (i = 0; i < sizeof(data); ++i) data[i] = (uint8_t)i;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f->bus, RAM, path, sizeof(path), &f->error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(f, 0x920b4u, RAM, 2u, 0u));
    *handle = f->cpu.r[0];
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f->bus, RAM + 128u, data, sizeof(data), &f->error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        call(f, 0x921a8u, *handle, RAM + 128u, sizeof(data)));
}

static void seek_ok(semu_test_context *context, fixture *f, uint32_t handle,
                    uint32_t offset, uint32_t origin, uint32_t position)
{
    uint64_t hits = f->layer.hits;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        call(f, SEEK_PC, handle, offset, origin));
    SEMU_TEST_EQ_U64(context, offset, f->cpu.r[0]);
    SEMU_TEST_EQ_U64(context, offset, f->cpu.r[1]);
    SEMU_TEST_EQ_U64(context, origin, f->cpu.r[2]);
    SEMU_TEST_EQ_U64(context, 0x80000u, f->cpu.r[15]);
    SEMU_TEST_EQ_U64(context, hits + 1u, f->layer.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(f, 0x92146u, handle, 0u, 0u));
    SEMU_TEST_EQ_U64(context, position, f->cpu.r[0]);
    SEMU_TEST_EQ_U64(context, 64u,
        semu_sapporo_239_file_size(f->files, "tssln/tss.bin"));
}

static void test_seek_return_and_cursor(semu_test_context *context)
{
    fixture f;
    uint32_t handle, byte;
    char log[8192];
    size_t count;
    semu_snapshot_writer saved;
    semu_snapshot_reader reader;
    SEMU_TEST_ASSERT(context, init(&f));
    setup_file(context, &f, &handle);
    seek_ok(context, &f, handle, 32u, 0u, 32u);
    seek_ok(context, &f, handle, 8u, 1u, 40u);
    seek_ok(context, &f, handle, UINT32_C(0xfffffff8), 1u, 32u);
    seek_ok(context, &f, handle, UINT32_C(0xffffffe0), 2u, 32u);
    seek_ok(context, &f, handle, 8u, 2u, 72u);
    seek_ok(context, &f, handle, 2384u, 0u, 2384u);
    seek_ok(context, &f, handle, 0u, 0u, 0u);
    seek_ok(context, &f, handle, 32u, 0u, 32u);
    semu_snapshot_writer_init(&saved);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f.files, &saved, &f.error));
    semu_sapporo_239_files_reset(f.files);
    semu_snapshot_reader_init(&reader, saved.data, saved.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_read(f.files, &reader, &f.error));
    SEMU_TEST_ASSERT(context, semu_snapshot_reader_done(&reader));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        call(&f, 0x921dcu, handle, RAM + 256u, 1u));
    SEMU_TEST_EQ_U64(context, 1u, f.cpu.r[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_read(f.bus, RAM + 256u, 1u, &byte, &f.error));
    SEMU_TEST_EQ_U64(context, 32u, byte);
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x92146u, handle, 0u, 0u));
    SEMU_TEST_EQ_U64(context, 33u, f.cpu.r[0]);
    rewind(f.log); count = fread(log, 1u, sizeof(log) - 1u, f.log);
    log[count] = '\0';
    SEMU_TEST_ASSERT(context, strstr(log,
        "operation=seek path=tssln/tss.bin result=8 size=64 cursor=40"));
    semu_snapshot_writer_destroy(&saved);
    destroy(&f);
}

static void unchanged(semu_test_context *context, fixture *f, uint32_t handle,
                       uint32_t offset, uint32_t origin, semu_status status)
{
    semu_snapshot_writer before, after;
    semu_cpu_state expected;
    uint64_t hits = f->layer.hits;
    f->cpu.r[0] = handle; f->cpu.r[1] = offset; f->cpu.r[2] = origin;
    f->cpu.r[15] = SEEK_PC; f->cpu.r[14] = 0x80001u;
    expected = f->cpu;
    semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f->files, &before, &f->error));
    SEMU_TEST_EQ_U64(context, status, call(f, SEEK_PC, handle, offset, origin));
    SEMU_TEST_ASSERT(context, memcmp(&expected, &f->cpu, sizeof(expected)) == 0);
    SEMU_TEST_EQ_U64(context, hits, f->layer.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f->files, &after, &f->error));
    SEMU_TEST_EQ_U64(context, before.size, after.size);
    SEMU_TEST_ASSERT(context, memcmp(before.data, after.data, before.size) == 0);
    semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
}

static void test_seek_atomic_refusals(semu_test_context *context)
{
    fixture f;
    uint32_t handle;
    SEMU_TEST_ASSERT(context, init(&f));
    setup_file(context, &f, &handle);
    unchanged(context, &f, handle, 1u, 3u, SEMU_ERR_STATE);
    unchanged(context, &f, handle, UINT32_MAX, 0u, SEMU_ERR_STATE);
    unchanged(context, &f, handle, UINT32_C(0x80000000), 1u, SEMU_ERR_STATE);
    unchanged(context, &f, handle, UINT32_C(0x7fffffff), 2u, SEMU_ERR_STATE);
    unchanged(context, &f, handle, 2385u, 0u, SEMU_ERR_STATE);
    unchanged(context, &f, handle + 1u, 32u, 0u, SEMU_ERR_STATE);
    unchanged(context, &f, RAM, 32u, 0u, SEMU_OK);
    f.layer.enabled = 0;
    unchanged(context, &f, handle, 32u, 0u, SEMU_ERR_STATE);
    f.layer.enabled = 1;
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920f4u, handle, 0u, 0u));
    unchanged(context, &f, handle, 32u, 0u, SEMU_ERR_STATE);
    setup_file(context, &f, &handle);
    ((semu_layer_intervention *)&f.layer.descriptor->interventions[
        SEMU_SAPPORO_239_IV_LOGICAL_FILE])->hits =
        f.layer.descriptor->interventions[SEMU_SAPPORO_239_IV_LOGICAL_FILE].max_hits;
    unchanged(context, &f, handle, 32u, 0u, SEMU_ERR_STATE);
    destroy(&f);
}

int main(void)
{
    const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_seek_return_and_cursor),
        SEMU_TEST_CASE(test_seek_atomic_refusals)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
