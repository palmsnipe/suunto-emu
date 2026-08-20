#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "../../src/cpu/armv7m/armv7m_internal.h"
#include "test.h"

#include <stddef.h>
#include <stdint.h>

#define CPU_SNAPSHOT_WAITING 232u
#define CPU_SNAPSHOT_SLEEP_MODE 1665u
#define CPU_SNAPSHOT_SLEEP_WAKE 1666u

static semu_status refuse_byte(semu_cpu_fixture *source,
                               semu_cpu_fixture *target, size_t offset,
                               uint8_t value)
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

static void test_sleep_linkage_refuses(semu_test_context *context)
{
    static const uint8_t program[] = { 0x00u, 0xbeu };
    semu_cpu_fixture source;
    semu_cpu_fixture target;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&source, program, sizeof(program)));
    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&target, program, sizeof(program)));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_byte(&source, &target,
                                 CPU_SNAPSHOT_SLEEP_MODE, ARMV7M_SLEEP_WFI));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_byte(&source, &target, CPU_SNAPSHOT_WAITING, 1u));

    /* A wake reason cannot be retained while a sleep is still active. */
    source.cpu->sleep_mode = ARMV7M_SLEEP_WFI;
    source.cpu->state.waiting_for_interrupt = 1;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_byte(&source, &target, CPU_SNAPSHOT_SLEEP_WAKE,
                                 1u));

    SEMU_TEST_EQ_U64(context, ARMV7M_SLEEP_NONE, target.cpu->sleep_mode);
    SEMU_TEST_EQ_U64(context, 0u, target.cpu->state.waiting_for_interrupt);
    semu_cpu_fixture_destroy(&target);
    semu_cpu_fixture_destroy(&source);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_sleep_linkage_refuses)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
