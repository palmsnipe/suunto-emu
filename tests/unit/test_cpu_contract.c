#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "semu/types.h"
#include "test.h"

static void test_cpu_contract_reset(semu_test_context *context)
{
    static const uint8_t program[] = {0x00u, 0xbeu};
    semu_cpu_fixture fixture;
    const semu_cpu_state *state;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program,
                                            sizeof(program)));
    state = semu_cpu_get_state(fixture.cpu);
    SEMU_TEST_EQ_U64(context, 0x800u, state->msp);
    SEMU_TEST_EQ_U64(context, 0x800u, state->r[13]);
    SEMU_TEST_EQ_U64(context, 0x100u, state->r[15]);
    SEMU_TEST_EQ_U64(context, 1u << 24, state->xpsr);
    SEMU_TEST_EQ_U64(context, 0u, state->instructions);
    SEMU_TEST_ASSERT(context, semu_cpu_stop_reason(fixture.cpu) == SEMU_STOP_NONE);
    SEMU_TEST_ASSERT(context, !semu_cpu_fault_address(fixture.cpu, NULL));
    semu_cpu_fixture_destroy(&fixture);
}

static void test_cpu_contract_fetch_boundary(semu_test_context *context)
{
    static const uint8_t program[] = {0x00u, 0xf0u};
    semu_cpu_fixture fixture;
    const semu_cpu_state *state;

    SEMU_TEST_ASSERT(context,
        semu_cpu_fixture_init_at(&fixture, program, sizeof(program), 0xffeu));
    SEMU_TEST_ASSERT(context,
        semu_cpu_fixture_load_u32(&fixture, 0x0cu, 0x201u));
    SEMU_TEST_ASSERT(context,
        semu_cpu_fixture_load_u32(&fixture, 0x200u, 0x0000be00u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    state = semu_cpu_get_state(fixture.cpu);
    SEMU_TEST_EQ_U64(context, 0x202u, state->r[15]);
    SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
                     semu_cpu_stop_reason(fixture.cpu));
    semu_cpu_fixture_destroy(&fixture);
}

static void test_cpu_contract_dispatch_16_32(semu_test_context *context)
{
    static const uint8_t narrow[] = {0x07u, 0x20u, 0x00u, 0xbeu};
    static const uint8_t wide[] = {0x4fu, 0xeau, 0x0du, 0x00u, 0x00u, 0xbeu};
    semu_cpu_fixture fixture;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, narrow, sizeof(narrow)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 7u, semu_cpu_get_state(fixture.cpu)->r[0]);
    SEMU_TEST_EQ_U64(context, 0x102u, semu_cpu_get_state(fixture.cpu)->r[15]);
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, wide, sizeof(wide)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0x800u, semu_cpu_get_state(fixture.cpu)->r[0]);
    SEMU_TEST_EQ_U64(context, 0x104u, semu_cpu_get_state(fixture.cpu)->r[15]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_cpu_contract_unsupported(semu_test_context *context)
{
    static const uint8_t program[] = {0x00u, 0xdeu};
    semu_cpu_fixture fixture;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program,
                                            sizeof(program)));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_UNSUPPORTED_INSTRUCTION,
                     semu_cpu_stop_reason(fixture.cpu));
    SEMU_TEST_EQ_U64(context, 0xde00u,
                     semu_cpu_fault_instruction(fixture.cpu));
    SEMU_TEST_EQ_U64(context, 0u,
                     semu_cpu_get_state(fixture.cpu)->instructions);
    SEMU_TEST_ASSERT(context, !semu_cpu_fault_address(fixture.cpu, NULL));
    semu_cpu_fixture_destroy(&fixture);
}

static void test_cpu_contract_unmapped_fault_address(semu_test_context *context)
{
    static const uint8_t program[] = {0x08u, 0x68u};
    semu_cpu_fixture fixture;
    semu_cpu_state *state;
    uint32_t address = 0u;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program,
                                            sizeof(program)));
    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_load_u32(&fixture, 0x0cu, 0x201u));
    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_load_u32(&fixture, 0x200u, 0x0000be00u));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[1] = 0x1000u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
                     semu_cpu_stop_reason(fixture.cpu));
    SEMU_TEST_ASSERT(context, semu_cpu_fault_address(fixture.cpu, &address));
    SEMU_TEST_EQ_U64(context, 0x1000u, address);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_cpu_contract_irq_range(semu_test_context *context)
{
    static const uint8_t program[] = {0x00u, 0xbeu};
    semu_cpu_fixture fixture;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program,
                                            sizeof(program)));
    semu_cpu_set_irq(fixture.cpu, 256u, 1);
    semu_cpu_set_irq_priority(fixture.cpu, 256u, 0xffu);
    semu_cpu_signal_event(fixture.cpu);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
                     semu_cpu_stop_reason(fixture.cpu));
    SEMU_TEST_EQ_U64(context, 1u,
                     semu_cpu_get_state(fixture.cpu)->instructions);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_cpu_contract_deterministic_retirement(semu_test_context *context)
{
    static const uint8_t program[] = {
        0x01u, 0x20u, 0x02u, 0x30u, 0x00u, 0xbeu
    };
    static const semu_cpu_memory_byte memory[] = {
        {0x100u, 0x01u}, {0x101u, 0x20u}, {0x104u, 0x00u}
    };
    semu_cpu_fixture first;
    semu_cpu_fixture second;
    semu_cpu_state expected;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&first, program, sizeof(program)));
    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&second, program, sizeof(program)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_run(&first, 3u));
    expected = *semu_cpu_get_state(first.cpu);
    SEMU_TEST_EQ_U64(context, 3u, expected.instructions);
    SEMU_TEST_EQ_U64(context, 3u, expected.r[0]);
    SEMU_TEST_EQ_U64(context, 0x106u, expected.r[15]);
    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_expect(&second, &expected, memory,
                                             SEMU_ARRAY_LEN(memory)) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_run(&second, 3u));
    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_expect(&second, &expected, memory,
                                             SEMU_ARRAY_LEN(memory)));
    semu_cpu_fixture_destroy(&second);
    semu_cpu_fixture_destroy(&first);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_cpu_contract_reset),
        SEMU_TEST_CASE(test_cpu_contract_fetch_boundary),
        SEMU_TEST_CASE(test_cpu_contract_dispatch_16_32),
        SEMU_TEST_CASE(test_cpu_contract_unsupported),
        SEMU_TEST_CASE(test_cpu_contract_unmapped_fault_address),
        SEMU_TEST_CASE(test_cpu_contract_irq_range),
        SEMU_TEST_CASE(test_cpu_contract_deterministic_retirement)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
