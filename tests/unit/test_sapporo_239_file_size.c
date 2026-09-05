#include "test.h"
#include "../../src/compat/sapporo_239.h"
#include "../../src/compat/sapporo_239_files.h"
#include <stdio.h>
#include <string.h>

#define RAM UINT32_C(0x10000000)
#define SIZE_PC UINT32_C(0x00092244)

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

static void open_file(semu_test_context *context, fixture *f, uint32_t mode)
{
    static const uint8_t path[] = "sleepln/sleep.bin";
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_load(f->bus, RAM, path, sizeof(path), &f->error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        call(f, 0x920b4u, RAM, mode, 0u));
}

static void query(semu_test_context *context, fixture *f, uint32_t handle,
                   semu_status status, uint32_t length, int intercepted)
{
    semu_snapshot_writer before, after;
    semu_cpu_state expected;
    uint64_t hits = f->layer.hits;
    semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f->files, &before, &f->error));
    f->cpu.r[0] = handle; f->cpu.r[15] = SIZE_PC;
    expected = f->cpu;
    if (intercepted) { expected.r[0] = length; expected.r[15] = 0x80000u; }
    SEMU_TEST_EQ_U64(context, status, semu_sapporo_239_apply_file_hook(
        f->files, f->bus, &f->cpu, &f->layer, &f->logger, &f->error));
    SEMU_TEST_ASSERT(context, memcmp(&expected, &f->cpu, sizeof(expected)) == 0);
    SEMU_TEST_EQ_U64(context, hits + (intercepted ? 1u : 0u), f->layer.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f->files, &after, &f->error));
    SEMU_TEST_EQ_U64(context, before.size, after.size);
    SEMU_TEST_ASSERT(context, memcmp(before.data, after.data, before.size) == 0);
    semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
}

static void test_file_size_lifecycle(semu_test_context *context)
{
    fixture f;
    uint32_t handle;
    char log[8192];
    size_t count;
    SEMU_TEST_ASSERT(context, init(&f));
    open_file(context, &f, 2u); handle = f.cpu.r[0];
    SEMU_TEST_ASSERT(context, semu_sapporo_239_file_hook_pc(SIZE_PC));
    SEMU_TEST_ASSERT(context, !semu_sapporo_239_file_hook_pc(SIZE_PC + 2u));
    query(context, &f, handle, SEMU_OK, 0u, 1);
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x921a8u, handle, RAM, 72u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x92182u, handle, 100u, 0u));
    query(context, &f, handle, SEMU_OK, 72u, 1);
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x92146u, handle, 0u, 0u));
    SEMU_TEST_EQ_U64(context, 100u, f.cpu.r[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x92182u, handle, 24u, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x92162u, handle, 0u, 0u));
    query(context, &f, handle, SEMU_OK, 24u, 1);
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x920f4u, handle, 0u, 0u));
    query(context, &f, handle, SEMU_ERR_STATE, 0u, 0);
    open_file(context, &f, 1u); handle = f.cpu.r[0];
    query(context, &f, handle, SEMU_OK, 24u, 1);
    rewind(f.log); count = fread(log, 1u, sizeof(log) - 1u, f.log);
    log[count] = '\0';
    SEMU_TEST_ASSERT(context, strstr(log,
        "operation=size path=sleepln/sleep.bin result=72 size=72 cursor=100"));
    semu_sapporo_239_files_reset(f.files);
    query(context, &f, handle, SEMU_ERR_STATE, 0u, 0);
    destroy(&f);
}

static void test_file_size_restore_and_refusals(semu_test_context *context)
{
    fixture f;
    uint32_t handle;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    SEMU_TEST_ASSERT(context, init(&f));
    open_file(context, &f, 2u); handle = f.cpu.r[0];
    SEMU_TEST_EQ_U64(context, SEMU_OK, call(&f, 0x921a8u, handle, RAM, 32u));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_write(f.files, &writer, &f.error));
    semu_sapporo_239_files_reset(f.files);
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_files_snapshot_read(f.files, &reader, &f.error));
    SEMU_TEST_ASSERT(context, semu_snapshot_reader_done(&reader));
    query(context, &f, handle, SEMU_OK, 32u, 1);
    query(context, &f, RAM, SEMU_OK, 0u, 0);
    query(context, &f, 0u, SEMU_OK, 0u, 0);
    query(context, &f, handle + 1u, SEMU_ERR_STATE, 0u, 0);
    query(context, &f, handle + 0x100u, SEMU_ERR_STATE, 0u, 0);
    f.layer.enabled = 0;
    query(context, &f, handle, SEMU_ERR_STATE, 0u, 0);
    f.layer.enabled = 1;
    ((semu_layer_intervention *)&f.layer.descriptor->interventions[
        SEMU_SAPPORO_239_IV_LOGICAL_FILE])->hits =
        f.layer.descriptor->interventions[SEMU_SAPPORO_239_IV_LOGICAL_FILE].max_hits;
    query(context, &f, handle, SEMU_ERR_STATE, 0u, 0);
    semu_snapshot_writer_destroy(&writer);
    destroy(&f);
}

int main(void)
{
    const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_file_size_lifecycle),
        SEMU_TEST_CASE(test_file_size_restore_and_refusals)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
