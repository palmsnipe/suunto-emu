#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "../../src/cpu/armv7m/armv7m_internal.h"
#include "test.h"

#include <stddef.h>
#include <stdint.h>

#define CPU_SNAPSHOT_XPSR 64u
#define CPU_SNAPSHOT_ITSTATE 1663u

static semu_status read_mutated(semu_cpu_fixture *source,
                                semu_cpu_fixture *target,
                                size_t offset, uint8_t value)
{
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_status status;

    semu_snapshot_writer_init(&writer);
    status = semu_cpu_snapshot_write(source->cpu, &writer, &source->error);
    if (status == SEMU_OK) {
        writer.data[offset] = value;
        semu_snapshot_reader_init(&reader, writer.data, writer.size);
        status = semu_cpu_snapshot_read(target->cpu, &reader,
                                        &target->error);
    }
    semu_snapshot_writer_destroy(&writer);
    return status;
}

static void test_itstate_snapshot_linkage(semu_test_context *context)
{
    static const uint8_t program[] = {0x00u, 0xbeu};
    semu_cpu_fixture source;
    semu_cpu_fixture target;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&source, program, sizeof(program)));
    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&target, program, sizeof(program)));

    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     read_mutated(&source, &target, CPU_SNAPSHOT_ITSTATE,
                                  1u));
    SEMU_TEST_EQ_U64(context, 0u, target.cpu->itstate);
    SEMU_TEST_EQ_U64(context, ARMV7M_XPSR_T, target.cpu->state.xpsr);

    source.cpu->itstate = 0x19u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     read_mutated(&source, &target, CPU_SNAPSHOT_XPSR + 1u,
                                  0x04u));
    SEMU_TEST_EQ_U64(context, 0u, target.cpu->itstate);
    SEMU_TEST_EQ_U64(context, ARMV7M_XPSR_T, target.cpu->state.xpsr);

    armv7m_set_itstate(source.cpu, 0x19u);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     read_mutated(&source, &target, CPU_SNAPSHOT_ITSTATE,
                                  0x19u));
    SEMU_TEST_EQ_U64(context, 0x19u, target.cpu->itstate);
    SEMU_TEST_EQ_U64(context, (1u << 25u) | (6u << 10u) |
                              ARMV7M_XPSR_T,
                     target.cpu->state.xpsr);

    semu_cpu_fixture_destroy(&target);
    semu_cpu_fixture_destroy(&source);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_itstate_snapshot_linkage)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
