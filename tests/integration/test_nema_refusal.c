#include "test.h"
#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "../../src/devices/sapporo_nema_gpu.h"
#include "../../src/display/nema_backend.h"
#include "../../src/display/nema_framing.h"

#define RING 0x10000000u
#define STOP (NEMA_GPU_BASE + 0xecu)
static int write_word(semu_cpu_fixture *f, uint32_t address, uint32_t value)
{ return semu_bus_write(f->bus, address, 4u, value, &f->error) == SEMU_OK; }

static void test_guest_gpu_store_precise_fault(semu_test_context *context)
{
    static const uint8_t program[] = {0x08u, 0x60u, 0x00u, 0xbeu}; /* STR r0,[r1]; BKPT */
    unsigned refuse;
    for (refuse = 0u; refuse < 2u; ++refuse) {
        semu_cpu_fixture f; semu_nema_gpu *gpu; semu_nema_backend *backend;
        semu_snapshot_writer before, after; uint32_t value;
        SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&f, program, sizeof(program)));
        SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&f, 12u, 0x181u));
        SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&f, 0x180u, 0xbe00u));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_ram(f.bus, "ring", RING, 4096u, &f.error));
        backend = semu_nema_backend_create(&f.error);
        SEMU_TEST_ASSERT(context, backend != NULL);
        gpu = semu_nema_gpu_create(f.bus, &semu_nema_backend_ops, backend,
            NULL, NULL, NULL, NULL, f.scheduler, &f.error);
        SEMU_TEST_ASSERT(context, gpu != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_attach(gpu, &f.error));
        SEMU_TEST_ASSERT(context, write_word(&f, RING, NEMA_REG_CMDADDR));
        SEMU_TEST_ASSERT(context, write_word(&f, RING + 4u, RING + 256u));
        SEMU_TEST_ASSERT(context, write_word(&f, RING + 8u, NEMA_CL_PUSH | NEMA_REG_CMDSIZE));
        SEMU_TEST_ASSERT(context, write_word(&f, RING + 12u, 2u));
        SEMU_TEST_ASSERT(context, write_word(&f, RING + 256u,
            refuse ? 0x7770u : NEMA_REG_CLIPMIN));
        SEMU_TEST_ASSERT(context, write_word(&f, NEMA_GPU_BASE + NEMA_REG_CMDADDR, RING));
        SEMU_TEST_ASSERT(context, write_word(&f, NEMA_GPU_BASE + NEMA_REG_CMDSIZE, 256u));
        SEMU_TEST_ASSERT(context, write_word(&f, STOP, RING | 6u));
        SEMU_TEST_ASSERT(context, write_word(&f, NEMA_GPU_BASE + 0xfcu, 0u));
        semu_cpu_get_state_mutable(f.cpu)->r[0] = RING + 16u;
        semu_cpu_get_state_mutable(f.cpu)->r[1] = STOP;
        semu_snapshot_writer_init(&before); semu_snapshot_writer_init(&after);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_snapshot_write(gpu, &before, &f.error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&f));
        SEMU_TEST_EQ_U64(context, refuse ? 0x180u : 0x102u, semu_cpu_get_state(f.cpu)->r[15]);
        SEMU_TEST_EQ_U64(context, refuse ? 3u : 0u, semu_cpu_get_state(f.cpu)->xpsr & 0x1ffu);
        if (refuse) {
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(f.bus, 0xe000ed28u, 4u, &value, &f.error));
            SEMU_TEST_EQ_U64(context, 0x8200u, value);
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(f.bus, 0xe000ed38u, 4u, &value, &f.error));
            SEMU_TEST_EQ_U64(context, STOP, value);
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_snapshot_write(gpu, &after, &f.error));
            SEMU_TEST_EQ_U64(context, before.size, after.size);
            SEMU_TEST_ASSERT(context, memcmp(before.data, after.data, before.size) == 0);
        }
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&f));
        SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT, semu_cpu_stop_reason(f.cpu));
        SEMU_TEST_EQ_U64(context, 2u, semu_cpu_get_state(f.cpu)->instructions);
        semu_snapshot_writer_destroy(&before); semu_snapshot_writer_destroy(&after);
        semu_nema_gpu_destroy(gpu); semu_nema_backend_destroy(backend);
        semu_cpu_fixture_destroy(&f);
    }
}
int main(void)
{
    static const semu_test_case cases[] = {SEMU_TEST_CASE(test_guest_gpu_store_precise_fault)};
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
