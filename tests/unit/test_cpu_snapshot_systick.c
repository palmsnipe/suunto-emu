#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "../../src/cpu/armv7m/armv7m_internal.h"
#include "test.h"

#include <stdint.h>

#define CPU_SNAPSHOT_SYSTICK_CONTROL 1668u
#define CPU_SNAPSHOT_SYSTICK_RELOAD 1672u
#define CPU_SNAPSHOT_SYSTICK_CURRENT 1676u
#define CPU_SNAPSHOT_SYSTICK_CALIBRATION 1680u
#define CPU_SNAPSHOT_SYSTICK_EVENT 1693u
#define CPU_SNAPSHOT_SYSTICK_EVENT_VALID 1701u

static void put_u32le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static void put_u64le(uint8_t *data, uint64_t value)
{
    unsigned index;
    for (index = 0u; index < 8u; ++index)
        data[index] = (uint8_t)(value >> (index * 8u));
}

static semu_status refuse_word(semu_cpu_fixture *source,
                               semu_cpu_fixture *target, size_t offset,
                               uint32_t value)
{
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_status status;

    semu_snapshot_writer_init(&writer);
    status = semu_cpu_snapshot_write(source->cpu, &writer, &source->error);
    if (status == SEMU_OK) {
        put_u32le(writer.data + offset, value);
        semu_snapshot_reader_init(&reader, writer.data, writer.size);
        status = semu_cpu_snapshot_read(target->cpu, &reader,
                                        &target->error);
    }
    semu_snapshot_writer_destroy(&writer);
    return status;
}

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

static semu_status refuse_u64(semu_cpu_fixture *source,
                              semu_cpu_fixture *target, size_t offset,
                              uint64_t value)
{
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_status status;

    semu_snapshot_writer_init(&writer);
    status = semu_cpu_snapshot_write(source->cpu, &writer, &source->error);
    if (status == SEMU_OK) {
        put_u64le(writer.data + offset, value);
        semu_snapshot_reader_init(&reader, writer.data, writer.size);
        status = semu_cpu_snapshot_read(target->cpu, &reader,
                                        &target->error);
    }
    semu_snapshot_writer_destroy(&writer);
    return status;
}

static void prepare(semu_cpu_fixture *source, semu_cpu_fixture *target,
                    semu_test_context *context)
{
    static const uint8_t program[] = { 0x00u, 0xbeu };
    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(source, program, sizeof(program)));
    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(target, program, sizeof(program)));
    source->cpu->systick_control = 5u;
    source->cpu->systick_reload = 3u;
    source->cpu->systick_current = 2u;
    source->cpu->systick_event = 1u;
    source->cpu->systick_event_valid = 1u;
}

static void test_register_state_refuses(semu_test_context *context)
{
    semu_cpu_fixture source;
    semu_cpu_fixture target;

    prepare(&source, &target, context);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_word(&source, &target,
                                 CPU_SNAPSHOT_SYSTICK_CONTROL, 0x8u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_word(&source, &target,
                                 CPU_SNAPSHOT_SYSTICK_CONTROL, 0x1u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_word(&source, &target,
                                 CPU_SNAPSHOT_SYSTICK_RELOAD, 0x01000000u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_word(&source, &target,
                                 CPU_SNAPSHOT_SYSTICK_CURRENT, 0x01000000u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_word(&source, &target,
                                 CPU_SNAPSHOT_SYSTICK_CALIBRATION, 1u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_u64(&source, &target, CPU_SNAPSHOT_SYSTICK_EVENT,
                                0u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_byte(&source, &target,
                                 CPU_SNAPSHOT_SYSTICK_EVENT_VALID, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_byte(&source, &target,
                                 CPU_SNAPSHOT_SYSTICK_EVENT_VALID, 2u));
    SEMU_TEST_EQ_U64(context, 0u, target.cpu->systick_control);
    SEMU_TEST_EQ_U64(context, 0u, target.cpu->systick_reload);
    SEMU_TEST_EQ_U64(context, 0u, target.cpu->systick_event_valid);
    semu_cpu_fixture_destroy(&target);
    semu_cpu_fixture_destroy(&source);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_register_state_refuses)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
