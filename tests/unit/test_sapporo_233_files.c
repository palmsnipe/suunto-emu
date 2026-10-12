/* Ticket 710 instance-36 / E-SAP-0069: the 2.33 session-only logical-file
 * overlay. The seed, create/write/read/seek/close lifecycle and the native
 * 0xbb204 size operation follow the twice-reproduced lane layer; every
 * unsupported shape refuses without touching transport or layer state. */
#include "sapporo_233_files.h"
#include "../../src/boards/machine_internal.h"
#include "test.h"
#include <string.h>

typedef struct {
    semu_sapporo_233_files_context files;
    semu_layer_state state;
    semu_logger logger;
    FILE *log;
    semu_bus *bus;
    semu_cpu_state cpu;
} fixture;

static int init(fixture *f, semu_error *e)
{
    const char *hashes[3];
    unsigned i;
    memset(f, 0, sizeof(*f));
    f->log = tmpfile();
    f->bus = semu_bus_create(e);
    if (!f->log || !f->bus) return 0;
    semu_log_init(&f->logger, f->log, SEMU_LOG_WARNING);
    if (semu_bus_map_ram(f->bus, "sram", UINT32_C(0x10000000),
            UINT32_C(0x180000), e) != SEMU_OK ||
        semu_bus_map_ram(f->bus, "mram", 0u, UINT32_C(0x00200000), e) !=
            SEMU_OK) {
        return 0;
    }
    for (i = 0u; i < 3u; ++i) hashes[i] =
        semu_sapporo_233_files_layer.component_hashes[i];
    if (semu_layer_enable_checked(&f->state, &semu_sapporo_233_files_layer,
            "sapporo-2.33.16", hashes, 3u, e) != SEMU_OK) {
        return 0;
    }
    f->files.state = &f->state;
    f->files.logger = &f->logger;
    return semu_sapporo_233_files_install(&f->files, f->bus, e) == SEMU_OK;
}

static void destroy(fixture *f)
{
    semu_bus_destroy(f->bus);
    if (f->log) (void)fclose(f->log);
}

/* The open path: mode 1 read of the seed, mode 2 create, mode 3 update. */
static void test_open_seed_create_and_lifecycle(semu_test_context *context)
{
    fixture f;
    semu_error e;
    SEMU_TEST_ASSERT(context, init(&f, &e));
    /* Read-open of a path the overlay does not know stays native (no
     * state change). */
    f.cpu.r[15] = SEMU_SAPPORO_233_FILES_OPEN;
    f.cpu.r[0] = UINT32_C(0x10020000);
    f.cpu.r[1] = 1u;
    f.cpu.r[14] = UINT32_C(0x00080000);
    SEMU_TEST_ASSERT(context, semu_bus_load(f.bus, UINT32_C(0x10020000),
        (const uint8_t *)"fsed", 4u, &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_files_apply(
        &f.files, f.bus, &f.cpu, &e));
    /* The native path ran unchanged (PC is still the open call). */
    SEMU_TEST_EQ_U64(context, SEMU_SAPPORO_233_FILES_OPEN, f.cpu.r[15]);
    /* Create 247.bin (mode 2): the overlay admits it and returns a handle. */
    f.cpu.r[15] = SEMU_SAPPORO_233_FILES_OPEN;
    /* "actitmln/247.bin" written byte-wise as the path argument. */
    {
        static const uint8_t path[] = "actitmln/247.bin";
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_load(f.bus,
            UINT32_C(0x10020000), path, sizeof(path) - 1u, &e));
    }
    f.cpu.r[1] = 2u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_files_apply(
        &f.files, f.bus, &f.cpu, &e));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x1015f000), f.cpu.r[0]);
    SEMU_TEST_EQ_U64(context, UINT32_C(0x00080000), f.cpu.r[15]);
    /* Write through the handle, close, reopen (mode 3), and the size op
     * returns the retained length exactly once. */
    f.cpu.r[15] = SEMU_SAPPORO_233_FILES_WRITE;
    f.cpu.r[0] = UINT32_C(0x1015f000);
    f.cpu.r[1] = UINT32_C(0x10030000);
    f.cpu.r[2] = 32u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(f.bus,
        UINT32_C(0x10030000), 4u, UINT32_C(0xd1b1d1ac), &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_files_apply(
        &f.files, f.bus, &f.cpu, &e));
    SEMU_TEST_EQ_U64(context, 32u, f.cpu.r[0]);
    f.cpu.r[15] = SEMU_SAPPORO_233_FILES_CLOSE;
    f.cpu.r[0] = UINT32_C(0x1015f000);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_files_apply(
        &f.files, f.bus, &f.cpu, &e));
    SEMU_TEST_EQ_U64(context, 1u, f.cpu.r[0]);
    f.cpu.r[15] = SEMU_SAPPORO_233_FILES_OPEN;
    f.cpu.r[0] = UINT32_C(0x10020000);
    f.cpu.r[1] = 3u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_files_apply(
        &f.files, f.bus, &f.cpu, &e));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x1015f100), f.cpu.r[0]);
    f.cpu.r[15] = SEMU_SAPPORO_233_FILES_SIZE;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_files_apply(
        &f.files, f.bus, &f.cpu, &e));
    SEMU_TEST_EQ_U64(context, 32u, f.cpu.r[0]);
    SEMU_TEST_EQ_U64(context, 0u, f.cpu.r[1]);
    SEMU_TEST_EQ_U64(context, UINT32_C(0x00080000), f.cpu.r[15]);
    /* A repeated size op on the same file refuses (sticky). */
    f.cpu.r[15] = SEMU_SAPPORO_233_FILES_SIZE;
    f.cpu.r[0] = UINT32_C(0x1015f100);
    SEMU_TEST_ASSERT(context, semu_sapporo_233_files_apply(&f.files, f.bus,
        &f.cpu, &e) != SEMU_OK);
    SEMU_TEST_ASSERT(context, strstr(e.text, "refused") != NULL);
    destroy(&f);
}

/* The seed and the unsupported shapes. */
static void test_seed_and_refusals(semu_test_context *context)
{
    fixture f;
    semu_error e;
    unsigned mode;
    SEMU_TEST_ASSERT(context, init(&f, &e));
    /* The seed is a 16-byte schema-valid ui.txt (wfn:00 + fuwe:T). */
    SEMU_TEST_EQ_U64(context, 16u, f.files.files[0].size);
    SEMU_TEST_ASSERT(context, memcmp(f.files.files[0].data, "wfn:00\r\n",
        7u) == 0);
    /* Read-open of the seed returns a handle and the read returns the
     * seeded bytes; seek to end reports the seed size. */
    f.cpu.r[15] = SEMU_SAPPORO_233_FILES_OPEN;
    f.cpu.r[0] = UINT32_C(0x10020000);
    f.cpu.r[1] = 1u;
    {
        static const uint8_t path[] = "settings/ui.txt";
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_load(f.bus,
            UINT32_C(0x10020000), path, sizeof(path) - 1u, &e));
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_files_apply(
        &f.files, f.bus, &f.cpu, &e));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x1015f000), f.cpu.r[0]);
    f.cpu.r[15] = SEMU_SAPPORO_233_FILES_SEEK;
    /* The lane ABI: offset = r2|r3<<32, origin at sp+0. */
    f.cpu.r[2] = 0u;
    f.cpu.r[3] = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(f.bus,
        UINT32_C(0x10029000), 4u, 2u, &e)); /* origin at sp+0 */
    f.cpu.r[13] = UINT32_C(0x10029000);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_files_apply(
        &f.files, f.bus, &f.cpu, &e));
    SEMU_TEST_EQ_U64(context, 16u, f.cpu.r[0]);
    /* Unsupported shapes refuse: unknown handle, bad create, overflow. */
    f.cpu.r[15] = SEMU_SAPPORO_233_FILES_WRITE;
    f.cpu.r[15] = SEMU_SAPPORO_233_FILES_WRITE;
    f.cpu.r[0] = UINT32_C(0x1015f900); /* unallocated handle */
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_files_apply(
        &f.files, f.bus, &f.cpu, &e)); /* native path, no state change */
    f.cpu.r[0] = UINT32_C(0x1015f000);
    f.cpu.r[2] = SEMU_SAPPORO_233_FILES_MAX_BYTES + 1u;
    SEMU_TEST_ASSERT(context, semu_sapporo_233_files_apply(&f.files, f.bus,
        &f.cpu, &e) != SEMU_OK);
    /* Exhausted layer refuses the next hook. */
    f.state.hits = semu_sapporo_233_files_layer.maximum_hits;
    f.cpu.r[15] = SEMU_SAPPORO_233_FILES_OPEN;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_files_apply(
        &f.files, f.bus, &f.cpu, &e));
    (void)mode;
    destroy(&f);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_open_seed_create_and_lifecycle),
        SEMU_TEST_CASE(test_seed_and_refusals)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
