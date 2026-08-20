#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "../../src/cpu/armv7m/armv7m_internal.h"
#include "test.h"

#include <stddef.h>
#include <string.h>

#define CPU_SNAPSHOT_FPCA 1659u
#define CPU_SNAPSHOT_FP_CONTEXT_FAULT 1660u
#define CPU_SNAPSHOT_STACK_FAULT 1661u
#define CPU_SNAPSHOT_BUS_FAULT 1662u
#define CPU_SNAPSHOT_EVENT_REGISTER 1664u
#define CPU_SNAPSHOT_SLEEP_MODE 1665u
#define CPU_SNAPSHOT_SLEEP_WAKE 1666u
#define CPU_SNAPSHOT_RESET_REQUESTED 1667u
#define CPU_SNAPSHOT_SYSTICK_COUNTFLAG 1692u
#define CPU_SNAPSHOT_SYSTICK_EVENT_VALID 1701u
#define CPU_SNAPSHOT_STACK_ALIGN 1702u
#define CPU_SNAPSHOT_EXCLUSIVE_VALID 1703u
#define CPU_SNAPSHOT_EXCLUSIVE_WIDTH 1708u
#define CPU_SNAPSHOT_IRQ_LEVEL 251u
#define CPU_SNAPSHOT_IRQ_ENABLED 507u
#define CPU_SNAPSHOT_IRQ_PENDING 763u
#define CPU_SNAPSHOT_IRQ_ACTIVE 1019u
#define CPU_SNAPSHOT_IRQ_PRIORITY 1275u
#define CPU_SNAPSHOT_IRQ_SOURCE_BITS 1531u
#define CPU_SNAPSHOT_SYSTEM_PRIORITY 1563u
#define CPU_SNAPSHOT_SYSTEM_PENDING 1579u
#define CPU_SNAPSHOT_SYSTEM_ACTIVE 1595u
#define CPU_SNAPSHOT_VECTOR_TABLE 247u
#define CPU_SNAPSHOT_SCR 1615u
#define CPU_SNAPSHOT_CCR 1619u
#define CPU_SNAPSHOT_SHCSR 1623u
#define CPU_SNAPSHOT_CFSR 1627u
#define CPU_SNAPSHOT_HFSR 1631u
#define CPU_SNAPSHOT_CPACR 1643u
#define CPU_SNAPSHOT_FPCCR 1647u
#define CPU_SNAPSHOT_FPCAR 1651u
#define CPU_SNAPSHOT_FPDSCR 1655u
#define CPU_SNAPSHOT_R13 52u
#define CPU_SNAPSHOT_PRIMASK 76u
#define CPU_SNAPSHOT_BASEPRI 80u
#define CPU_SNAPSHOT_FAULTMASK 84u
#define CPU_SNAPSHOT_CONTROL 88u
#define CPU_SNAPSHOT_XPSR 64u

static semu_cpu_state initial_state(void)
{
    semu_cpu_state state;
    (void)memset(&state, 0, sizeof(state));
    state.r[13] = 0x800u;
    state.r[15] = 0x100u;
    state.msp = 0x800u;
    state.xpsr = 1u << 24;
    return state;
}

static int prepare(semu_cpu_fixture *fixture, const semu_cpu_state *state)
{
    static const uint8_t program[] = {0x00u, 0xbeu};

    if (!semu_cpu_fixture_init(fixture, program, sizeof(program))) return 0;
    semu_cpu_fixture_apply_state(fixture, state);
    return 1;
}

static semu_status refuse_byte(semu_cpu_fixture *fixture, size_t offset,
                               uint8_t value)
{
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_status status;

    semu_snapshot_writer_init(&writer);
    status = semu_cpu_snapshot_write(fixture->cpu, &writer, &fixture->error);
    if (status == SEMU_OK) {
        if (writer.size <= offset) {
            status = SEMU_ERR_RANGE;
        } else {
            writer.data[offset] = value;
            semu_snapshot_reader_init(&reader, writer.data, writer.size);
            status = semu_cpu_snapshot_read(fixture->cpu, &reader,
                                             &fixture->error);
        }
    }
    semu_snapshot_writer_destroy(&writer);
    return status;
}

static void put_u32le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static semu_status refuse_word(semu_cpu_fixture *fixture, size_t offset,
                               uint32_t value)
{
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_status status;

    semu_snapshot_writer_init(&writer);
    status = semu_cpu_snapshot_write(fixture->cpu, &writer, &fixture->error);
    if (status == SEMU_OK) {
        if (writer.size < offset + 4u) {
            status = SEMU_ERR_RANGE;
        } else {
            put_u32le(writer.data + offset, value);
            semu_snapshot_reader_init(&reader, writer.data, writer.size);
            status = semu_cpu_snapshot_read(fixture->cpu, &reader,
                                             &fixture->error);
        }
    }
    semu_snapshot_writer_destroy(&writer);
    return status;
}

static void test_binary_snapshot_flags_refuse(semu_test_context *context)
{
    static const size_t offsets[] = {
        CPU_SNAPSHOT_FPCA, CPU_SNAPSHOT_FP_CONTEXT_FAULT,
        CPU_SNAPSHOT_STACK_FAULT, CPU_SNAPSHOT_BUS_FAULT,
        CPU_SNAPSHOT_EVENT_REGISTER, CPU_SNAPSHOT_RESET_REQUESTED,
        CPU_SNAPSHOT_SYSTICK_COUNTFLAG, CPU_SNAPSHOT_SYSTICK_EVENT_VALID,
        CPU_SNAPSHOT_STACK_ALIGN, CPU_SNAPSHOT_EXCLUSIVE_VALID
    };
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();
    size_t index;

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    for (index = 0u; index < SEMU_ARRAY_LEN(offsets); ++index) {
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                         refuse_byte(&fixture, offsets[index], 2u));
    }
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->fpca);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->fp_context_fault);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->stack_fault_active);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->bus_fault_active);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->event_register);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->reset_requested);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->systick_countflag);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->systick_event_valid);
    SEMU_TEST_EQ_U64(context, 1u, fixture.cpu->stack_align);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->exclusive_valid);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_sleep_snapshot_enums_refuse(semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_byte(&fixture, CPU_SNAPSHOT_SLEEP_MODE, 3u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_byte(&fixture, CPU_SNAPSHOT_SLEEP_WAKE, 4u));
    SEMU_TEST_EQ_U64(context, ARMV7M_SLEEP_NONE, fixture.cpu->sleep_mode);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->sleep_wake_source);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_stack_alignment_linkage_refuses(semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_byte(&fixture, CPU_SNAPSHOT_STACK_ALIGN, 0u));
    SEMU_TEST_EQ_U64(context, 1u, fixture.cpu->stack_align);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_stack_pointer_linkage_refuses(semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_word(&fixture, CPU_SNAPSHOT_R13, 0x804u));
    SEMU_TEST_EQ_U64(context, 0x800u,
                     semu_cpu_get_state(fixture.cpu)->r[13]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_core_mask_state_refuses(semu_test_context *context)
{
    static const size_t offsets[] = {
        CPU_SNAPSHOT_PRIMASK, CPU_SNAPSHOT_BASEPRI,
        CPU_SNAPSHOT_FAULTMASK, CPU_SNAPSHOT_CONTROL
    };
    static const uint32_t values[] = { 2u, 0x100u, 2u, 4u };
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();
    size_t index;

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    for (index = 0u; index < SEMU_ARRAY_LEN(offsets); ++index) {
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                         refuse_word(&fixture, offsets[index], values[index]));
    }
    SEMU_TEST_EQ_U64(context, 0u,
                     semu_cpu_get_state(fixture.cpu)->primask);
    SEMU_TEST_EQ_U64(context, 0u,
                     semu_cpu_get_state(fixture.cpu)->basepri);
    SEMU_TEST_EQ_U64(context, 0u,
                     semu_cpu_get_state(fixture.cpu)->faultmask);
    SEMU_TEST_EQ_U64(context, 0u,
                     semu_cpu_get_state(fixture.cpu)->control);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_xpsr_state_refuses(semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_word(&fixture, CPU_SNAPSHOT_XPSR,
                                 (1u << 24) | (1u << 23)));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_word(&fixture, CPU_SNAPSHOT_XPSR, 0u));
    SEMU_TEST_EQ_U64(context, 1u << 24,
                     semu_cpu_get_state(fixture.cpu)->xpsr);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_interrupt_snapshot_arrays_refuse(semu_test_context *context)
{
    static const size_t offsets[] = {
        CPU_SNAPSHOT_IRQ_LEVEL, CPU_SNAPSHOT_IRQ_ENABLED,
        CPU_SNAPSHOT_IRQ_PENDING, CPU_SNAPSHOT_IRQ_ACTIVE,
        CPU_SNAPSHOT_IRQ_PRIORITY, CPU_SNAPSHOT_IRQ_SOURCE_BITS,
        CPU_SNAPSHOT_SYSTEM_PRIORITY, CPU_SNAPSHOT_SYSTEM_PENDING,
        CPU_SNAPSHOT_SYSTEM_ACTIVE
    };
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();
    size_t index;

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    for (index = 0u; index < SEMU_ARRAY_LEN(offsets); ++index) {
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                         refuse_byte(&fixture, offsets[index],
                                     index == 5u ? 1u : 2u));
    }
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->irq_level[0u]);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->irq_enabled[0u]);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->irq_pending[0u]);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->irq_active[0u]);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->irq_priority[0u]);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->irq_source_bits[0u]);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->system_priority[0u]);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->system_pending[0u]);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->system_active[0u]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_system_snapshot_masks_refuse(semu_test_context *context)
{
    static const size_t offsets[] = {
        CPU_SNAPSHOT_VECTOR_TABLE, CPU_SNAPSHOT_SCR, CPU_SNAPSHOT_CCR,
        CPU_SNAPSHOT_SHCSR, CPU_SNAPSHOT_CFSR, CPU_SNAPSHOT_HFSR,
        CPU_SNAPSHOT_CPACR, CPU_SNAPSHOT_FPCCR, CPU_SNAPSHOT_FPCAR,
        CPU_SNAPSHOT_FPDSCR
    };
    static const uint8_t values[] = {
        1u, 1u, 1u, 1u, 1u, 1u, 1u, 4u, 1u, 1u
    };
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();
    size_t index;

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    for (index = 0u; index < SEMU_ARRAY_LEN(offsets); ++index) {
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                         refuse_byte(&fixture, offsets[index], values[index]));
    }
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->vector_table);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->scr);
    SEMU_TEST_EQ_U64(context, 1u << 9, fixture.cpu->ccr);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->shcsr);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->cfsr);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->hfsr);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->cpacr);
    SEMU_TEST_EQ_U64(context, ARMV7M_FPCCR_CONTROL_MASK, fixture.cpu->fpccr);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->fpcar);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->fpdscr);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_exclusive_snapshot_width_refuse(semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_word(&fixture, CPU_SNAPSHOT_EXCLUSIVE_WIDTH, 3u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     refuse_word(&fixture, CPU_SNAPSHOT_EXCLUSIVE_WIDTH, 1u));
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->exclusive_valid);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->exclusive_width);
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_binary_snapshot_flags_refuse),
        SEMU_TEST_CASE(test_sleep_snapshot_enums_refuse),
        SEMU_TEST_CASE(test_stack_alignment_linkage_refuses),
        SEMU_TEST_CASE(test_stack_pointer_linkage_refuses),
        SEMU_TEST_CASE(test_core_mask_state_refuses),
        SEMU_TEST_CASE(test_xpsr_state_refuses),
        SEMU_TEST_CASE(test_interrupt_snapshot_arrays_refuse),
        SEMU_TEST_CASE(test_system_snapshot_masks_refuse),
        SEMU_TEST_CASE(test_exclusive_snapshot_width_refuse)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
