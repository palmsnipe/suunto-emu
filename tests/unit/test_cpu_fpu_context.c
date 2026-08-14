#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "../../src/cpu/armv7m/armv7m_internal.h"
#include "test.h"

#include <string.h>

#define CPACR UINT32_C(0xe000ed88)
#define FPCCR UINT32_C(0xe000ef34)
#define CFSR UINT32_C(0xe000ed28)
#define SHCSR UINT32_C(0xe000ed24)
#define LSPACT ARMV7M_FPCCR_LSPACT
#define BFSR_STKERR ARMV7M_CFSR_BFSR_STKERR
#define NOCP (1u << 19)
#define XPSR_T ARMV7M_XPSR_T
#define XPSR_STACK_ALIGN ARMV7M_XPSR_STACK_ALIGN

static int write_word(semu_cpu_fixture *fixture, uint32_t address,
                      uint32_t value)
{
    return semu_bus_write(fixture->bus, address, 4u, value,
                          &fixture->error) == SEMU_OK;
}

static int read_word(semu_cpu_fixture *fixture, uint32_t address,
                     uint32_t *value)
{
    return semu_bus_read(fixture->bus, address, 4u, value,
                         &fixture->error) == SEMU_OK;
}

static semu_cpu_state base_state(void)
{
    semu_cpu_state state;

    (void)memset(&state, 0, sizeof(state));
    state.msp = 0x800u;
    state.r[13] = state.msp;
    state.r[15] = 0x100u;
    state.xpsr = XPSR_T;
    return state;
}

static int prepare(semu_cpu_fixture *fixture, semu_cpu_state *state,
                   unsigned exception)
{
    static const uint8_t program[] = {0x00u, 0xbeu};
    uint32_t handler = 0x181u;

    if (!semu_cpu_fixture_init(fixture, program, sizeof(program)) ||
        !write_word(fixture, exception * 4u, handler)) {
        semu_cpu_fixture_destroy(fixture);
        return 0;
    }
    semu_cpu_fixture_apply_state(fixture, state);
    return 1;
}

static void set_fp_state(semu_cpu *cpu)
{
    unsigned index;

    for (index = 0u; index < 16u; ++index)
        cpu->state.s[index] = UINT32_C(0x10000000) + index;
    for (index = 16u; index < 32u; ++index)
        cpu->state.s[index] = UINT32_C(0x20000000) + index;
    cpu->state.fpscr = UINT32_C(0x03400000);
    cpu->fpca = 1u;
}

static int enable_fpu(semu_cpu_fixture *fixture)
{
    return write_word(fixture, CPACR, UINT32_C(0x00f00000));
}

static semu_status use_fp(semu_cpu_fixture *fixture, uint32_t value)
{
    fixture->cpu->state.r[0] = value;
    return armv7m_exec32_fpu(fixture->cpu, 0xee00u, 0x0a10u, 0x180u,
                             &fixture->error);
}

static void test_control_fpca_visibility(semu_test_context *context)
{
    semu_cpu_state state = base_state();
    semu_cpu_fixture fixture;

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u));
    fixture.cpu->fpca = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_exec32_system(fixture.cpu, 0xf3efu, 0x8014u,
                                          0x100u, &fixture.error));
    SEMU_TEST_EQ_U64(context, 4u, fixture.cpu->state.r[0]);
    fixture.cpu->fpca = 0u;
    fixture.cpu->state.control = 2u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_exec32_system(fixture.cpu, 0xf3efu, 0x8014u,
                                          0x100u, &fixture.error));
    SEMU_TEST_EQ_U64(context, 2u, fixture.cpu->state.r[0]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_basic_and_eager_extended_frames(semu_test_context *context)
{
    semu_cpu_state state = base_state();
    semu_cpu_fixture fixture;
    uint32_t value;
    unsigned index;

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 16u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 0x7e0u, fixture.cpu->state.msp);
    SEMU_TEST_EQ_U64(context, 0xfffffff9u, fixture.cpu->state.r[14]);
    SEMU_TEST_ASSERT(context, read_word(&fixture, 0x7fcu, &value));
    SEMU_TEST_EQ_U64(context, XPSR_T, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xfffffff9u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 0x800u, fixture.cpu->state.msp);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->fpca);
    semu_cpu_fixture_destroy(&fixture);

    state = base_state();
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u));
    SEMU_TEST_ASSERT(context, enable_fpu(&fixture));
    SEMU_TEST_ASSERT(context, write_word(&fixture, FPCCR,
                                         ARMV7M_FPCCR_ASPEN));
    set_fp_state(fixture.cpu);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 16u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 0x798u, fixture.cpu->state.msp);
    SEMU_TEST_EQ_U64(context, 0xffffffe9u, fixture.cpu->state.r[14]);
    SEMU_TEST_EQ_U64(context, 0u,
                     fixture.cpu->fpccr & ARMV7M_FPCCR_STATUS_MASK);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->fpcar);
    for (index = 0u; index < 16u; ++index)
        SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->state.s[index]);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->state.fpscr);
    SEMU_TEST_ASSERT(context, read_word(&fixture, 0x7b8u, &value));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x10000000), value);
    SEMU_TEST_ASSERT(context, read_word(&fixture, 0x7f8u, &value));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x03400000), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xffffffe9u,
                                            &fixture.error));
    for (index = 0u; index < 16u; ++index)
        SEMU_TEST_EQ_U64(context, UINT32_C(0x10000000) + index,
                         fixture.cpu->state.s[index]);
    SEMU_TEST_EQ_U64(context, UINT32_C(0x03400000), fixture.cpu->state.fpscr);
    SEMU_TEST_EQ_U64(context, 1u, fixture.cpu->fpca);
    SEMU_TEST_EQ_U64(context, 0x800u, fixture.cpu->state.msp);
    for (index = 16u; index < 32u; ++index)
        SEMU_TEST_EQ_U64(context, UINT32_C(0x20000000) + index,
                         fixture.cpu->state.s[index]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_lazy_untouched_and_materialized(semu_test_context *context)
{
    semu_cpu_state state = base_state();
    semu_cpu_fixture fixture;
    uint32_t value;
    unsigned index;

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u));
    SEMU_TEST_ASSERT(context, enable_fpu(&fixture));
    set_fp_state(fixture.cpu);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 16u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 0x7b8u, fixture.cpu->fpcar);
    SEMU_TEST_ASSERT(context, (fixture.cpu->fpccr & LSPACT) != 0u);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->fpca);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xffffffe9u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->fpcar);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->fpccr & LSPACT);
    for (index = 0u; index < 16u; ++index)
        SEMU_TEST_EQ_U64(context, UINT32_C(0x10000000) + index,
                         fixture.cpu->state.s[index]);
    semu_cpu_fixture_destroy(&fixture);

    state = base_state();
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u));
    SEMU_TEST_ASSERT(context, enable_fpu(&fixture));
    set_fp_state(fixture.cpu);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 16u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, use_fp(&fixture, UINT32_C(0xcafebabe)));
    SEMU_TEST_EQ_U64(context, UINT32_C(0xcafebabe), fixture.cpu->state.s[0]);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->fpcar);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->fpccr & LSPACT);
    SEMU_TEST_ASSERT(context, read_word(&fixture, 0x7b8u, &value));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x10000000), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xffffffe9u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x10000000), fixture.cpu->state.s[0]);
    SEMU_TEST_EQ_U64(context, UINT32_C(0x03400000), fixture.cpu->state.fpscr);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_nested_lazy_and_fp_contexts(semu_test_context *context)
{
    semu_cpu_state state = base_state();
    semu_cpu_fixture fixture;
    uint32_t original;

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, 17u * 4u, 0x191u));
    SEMU_TEST_ASSERT(context, enable_fpu(&fixture));
    set_fp_state(fixture.cpu);
    original = fixture.cpu->state.s[0];
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 16u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 17u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 0xfffffff1u, fixture.cpu->state.r[14]);
    SEMU_TEST_ASSERT(context, (fixture.cpu->fpccr & LSPACT) != 0u);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xfffffff1u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 16u,
                     fixture.cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK);
    SEMU_TEST_ASSERT(context, (fixture.cpu->fpccr & LSPACT) != 0u);
    SEMU_TEST_EQ_U64(context, SEMU_OK, use_fp(&fixture, 0xabcddcbau));
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->fpccr & LSPACT);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xffffffe9u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, original, fixture.cpu->state.s[0]);
    semu_cpu_fixture_destroy(&fixture);

    state = base_state();
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, 17u * 4u, 0x191u));
    SEMU_TEST_ASSERT(context, enable_fpu(&fixture));
    set_fp_state(fixture.cpu);
    original = fixture.cpu->state.s[0];
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 16u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, use_fp(&fixture, 0x11112222u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 17u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 0xffffffe1u, fixture.cpu->state.r[14]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, use_fp(&fixture, 0x33334444u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xffffffe1u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x11112222), fixture.cpu->state.s[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xffffffe9u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, original, fixture.cpu->state.s[0]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_psp_alignment_and_stack_refusal(semu_test_context *context)
{
    semu_cpu_state state = base_state();
    semu_cpu_fixture fixture;
    uint32_t value;

    state.control = 2u;
    state.psp = 0x804u;
    state.msp = 0x900u;
    state.r[13] = state.psp;
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u));
    SEMU_TEST_ASSERT(context, enable_fpu(&fixture));
    set_fp_state(fixture.cpu);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 16u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 0x798u, fixture.cpu->state.psp);
    SEMU_TEST_EQ_U64(context, 0x900u, fixture.cpu->state.r[13]);
    SEMU_TEST_EQ_U64(context, 0xffffffedu, fixture.cpu->state.r[14]);
    SEMU_TEST_ASSERT(context, read_word(&fixture, 0x7b4u, &value));
    SEMU_TEST_ASSERT(context, (value & XPSR_STACK_ALIGN) != 0u);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xffffffedu,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 0x804u, fixture.cpu->state.psp);
    SEMU_TEST_EQ_U64(context, 2u, fixture.cpu->state.control & 2u);
    semu_cpu_fixture_destroy(&fixture);

    state = base_state();
    state.msp = 0x1008u;
    state.r[13] = state.msp;
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u));
    SEMU_TEST_ASSERT(context, enable_fpu(&fixture));
    set_fp_state(fixture.cpu);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
                     armv7m_take_exception(fixture.cpu, 16u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 0x1008u, fixture.cpu->state.msp);
    SEMU_TEST_ASSERT(context, (fixture.cpu->cfsr & BFSR_STKERR) != 0u);
    SEMU_TEST_ASSERT(context, read_word(&fixture, CFSR, &value));
    SEMU_TEST_ASSERT(context, (value & BFSR_STKERR) != 0u);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_corrupt_and_refused_unstack(semu_test_context *context)
{
    semu_cpu_state state = base_state();
    semu_cpu_fixture fixture;
    uint32_t value;

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, 6u * 4u, 0x1c1u));
    SEMU_TEST_ASSERT(context, enable_fpu(&fixture));
    SEMU_TEST_ASSERT(context, write_word(&fixture, FPCCR,
                                         ARMV7M_FPCCR_ASPEN));
    set_fp_state(fixture.cpu);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 16u,
                                            &fixture.error));
    SEMU_TEST_ASSERT(context, write_word(&fixture, 0x7b4u, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xffffffe9u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 6u,
                     fixture.cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->state.s[0]);
    semu_cpu_fixture_destroy(&fixture);

    state = base_state();
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, 6u * 4u, 0x1c1u));
    SEMU_TEST_ASSERT(context, enable_fpu(&fixture));
    SEMU_TEST_ASSERT(context, write_word(&fixture, FPCCR,
                                         ARMV7M_FPCCR_ASPEN));
    set_fp_state(fixture.cpu);
    semu_cpu_set_irq_priority(fixture.cpu, 0u, 0xffu);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 16u,
                                            &fixture.error));
    SEMU_TEST_ASSERT(context, write_word(&fixture, CPACR, 0u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SHCSR, 1u << 18));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xffffffe9u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 6u,
                     fixture.cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK);
    SEMU_TEST_ASSERT(context, read_word(&fixture, CFSR, &value));
    SEMU_TEST_ASSERT(context, (value & NOCP) != 0u);
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_control_fpca_visibility),
        SEMU_TEST_CASE(test_basic_and_eager_extended_frames),
        SEMU_TEST_CASE(test_lazy_untouched_and_materialized),
        SEMU_TEST_CASE(test_nested_lazy_and_fp_contexts),
        SEMU_TEST_CASE(test_psp_alignment_and_stack_refusal),
        SEMU_TEST_CASE(test_corrupt_and_refused_unstack)
    };

    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
