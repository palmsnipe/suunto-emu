#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "test.h"

#include <string.h>

#define XPSR_N (1u << 31)
#define XPSR_Z (1u << 30)
#define XPSR_C (1u << 29)
#define XPSR_V (1u << 28)
#define XPSR_T (1u << 24)

#define SHIFT_IMM(type, amount) \
    (uint16_t)(((type) << 11) | ((amount) << 6))
#define SHIFT_REG(op) (uint16_t)(0x4000u | ((op) << 6) | (1u << 3))
#define ADD_SUB(op, operand) \
    (uint16_t)(0x1800u | ((op) << 9) | ((operand) << 6))
#define IMM(op, value) (uint16_t)(0x2000u | ((op) << 11) | (value))

static int run_one(uint16_t instruction, uint32_t r0, uint32_t r1,
                   uint32_t xpsr, uint32_t expected_r0,
                   uint32_t expected_xpsr)
{
    uint8_t program[] = {
        (uint8_t)instruction, (uint8_t)(instruction >> 8), 0x00u, 0xbeu
    };
    semu_cpu_fixture first = {0};
    semu_cpu_fixture second = {0};
    semu_cpu_state *state;
    semu_status first_status;
    semu_status second_status;
    int result = 0;

    if (!semu_cpu_fixture_init(&first, program, sizeof(program)) ||
        !semu_cpu_fixture_init(&second, program, sizeof(program))) {
        semu_cpu_fixture_destroy(&first);
        semu_cpu_fixture_destroy(&second);
        return 0;
    }
    state = semu_cpu_get_state_mutable(first.cpu);
    state->r[0] = r0;
    state->r[1] = r1;
    state->xpsr = xpsr;
    state = semu_cpu_get_state_mutable(second.cpu);
    state->r[0] = r0;
    state->r[1] = r1;
    state->xpsr = xpsr;
    first_status = semu_cpu_fixture_step(&first);
    second_status = semu_cpu_fixture_step(&second);
    state = semu_cpu_get_state_mutable(first.cpu);
    result = first_status == SEMU_OK && second_status == SEMU_OK &&
             state->r[0] == expected_r0 && state->xpsr == expected_xpsr &&
             state->r[15] == 0x102u && state->instructions == 1u &&
             semu_cpu_stop_reason(first.cpu) == SEMU_STOP_NONE &&
             memcmp(state, semu_cpu_get_state_mutable(second.cpu),
                    sizeof(*state)) == 0;
    semu_cpu_fixture_destroy(&second);
    semu_cpu_fixture_destroy(&first);
    return result;
}

static void test_shift_immediate_boundaries(semu_test_context *context)
{
    SEMU_TEST_ASSERT(context, run_one(SHIFT_IMM(0u, 0u), 0x80000001u, 0u,
                                      XPSR_T | XPSR_C | XPSR_V,
                                      0x80000001u,
                                      XPSR_T | XPSR_N | XPSR_C | XPSR_V));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_IMM(0u, 1u), 0x40000000u, 0u,
                                      XPSR_T | XPSR_C | XPSR_V,
                                      0x80000000u, XPSR_T | XPSR_N | XPSR_V));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_IMM(0u, 31u), 1u, 0u,
                                      XPSR_T | XPSR_C | XPSR_V,
                                      0x80000000u, XPSR_T | XPSR_N | XPSR_V));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_IMM(1u, 0u), 0x80000000u, 0u,
                                      XPSR_T | XPSR_V, 0u,
                                      XPSR_T | XPSR_Z | XPSR_C | XPSR_V));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_IMM(2u, 0u), 0x80000000u, 0u,
                                      XPSR_T | XPSR_C | XPSR_V, 0xffffffffu,
                                      XPSR_T | XPSR_N | XPSR_C | XPSR_V));
}

static void test_shift_register_boundaries(semu_test_context *context)
{
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(2u), 0x80000001u, 0u,
                                      XPSR_T | XPSR_C | XPSR_V,
                                      0x80000001u,
                                      XPSR_T | XPSR_N | XPSR_C | XPSR_V));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(2u), 0x40000000u, 1u,
                                      XPSR_T | XPSR_C | XPSR_V,
                                      0x80000000u, XPSR_T | XPSR_N | XPSR_V));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(2u), 1u, 31u,
                                      XPSR_T | XPSR_C | XPSR_V,
                                      0x80000000u, XPSR_T | XPSR_N | XPSR_V));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(2u), 1u, 32u,
                                      XPSR_T | XPSR_V, 0u,
                                      XPSR_T | XPSR_Z | XPSR_C | XPSR_V));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(2u), 1u, 33u,
                                      XPSR_T | XPSR_C | XPSR_V, 0u,
                                      XPSR_T | XPSR_Z | XPSR_V));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(2u), 0x80000000u, 255u,
                                      XPSR_T | XPSR_C | XPSR_V, 0u,
                                      XPSR_T | XPSR_Z | XPSR_V));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(3u), 0x80000000u, 32u,
                                      XPSR_T | XPSR_V, 0u,
                                      XPSR_T | XPSR_Z | XPSR_C | XPSR_V));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(4u), 0x80000000u, 32u,
                                      XPSR_T | XPSR_V, 0xffffffffu,
                                      XPSR_T | XPSR_N | XPSR_C | XPSR_V));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(7u), 0x80000001u, 32u,
                                      XPSR_T | XPSR_V, 0x80000001u,
                                      XPSR_T | XPSR_N | XPSR_C | XPSR_V));
}

static void test_add_sub_forms(semu_test_context *context)
{
    SEMU_TEST_ASSERT(context, run_one(ADD_SUB(0u, 1u), 0xffffffffu, 1u,
                                      XPSR_T, 0u,
                                      XPSR_T | XPSR_Z | XPSR_C));
    SEMU_TEST_ASSERT(context, run_one(ADD_SUB(1u, 1u), 0u, 1u, XPSR_T,
                                      0xffffffffu, XPSR_T | XPSR_N));
    SEMU_TEST_ASSERT(context, run_one(ADD_SUB(2u, 1u), 0xffffffffu, 0u,
                                      XPSR_T, 0u,
                                      XPSR_T | XPSR_Z | XPSR_C));
    SEMU_TEST_ASSERT(context, run_one(ADD_SUB(3u, 1u), 0u, 0u, XPSR_T,
                                      0xffffffffu, XPSR_T | XPSR_N));
    SEMU_TEST_ASSERT(context, run_one(ADD_SUB(0u, 1u), 0x7fffffffu, 1u,
                                      XPSR_T, 0x80000000u,
                                      XPSR_T | XPSR_N | XPSR_V));
    SEMU_TEST_ASSERT(context, run_one(ADD_SUB(1u, 1u), 0x80000000u, 1u,
                                      XPSR_T, 0x7fffffffu,
                                      XPSR_T | XPSR_V | XPSR_C));
}

static void test_immediate_forms(semu_test_context *context)
{
    SEMU_TEST_ASSERT(context, run_one(IMM(0u, 0u), 0xffffffffu, 0u,
                                      XPSR_T | XPSR_C | XPSR_V, 0u,
                                      XPSR_T | XPSR_Z | XPSR_C | XPSR_V));
    SEMU_TEST_ASSERT(context, run_one(IMM(1u, 1u), 0u, 0u, XPSR_T,
                                      0u, XPSR_T | XPSR_N));
    SEMU_TEST_ASSERT(context, run_one(IMM(2u, 1u), 0xffffffffu, 0u, XPSR_T,
                                      0u, XPSR_T | XPSR_Z | XPSR_C));
    SEMU_TEST_ASSERT(context, run_one(IMM(3u, 1u), 0u, 0u, XPSR_T,
                                      0xffffffffu, XPSR_T | XPSR_N));
}

static void test_logical_forms_preserve_carry_overflow(semu_test_context *context)
{
    const uint32_t flags = XPSR_T | XPSR_C | XPSR_V;

    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(0u), 0xf0f00000u,
                                      0x0ff00000u, flags, 0x00f00000u, flags));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(1u), 0xf0f00000u,
                                      0x0ff00000u, flags, 0xff000000u,
                                      flags | XPSR_N));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(8u), 0x00f00000u,
                                      0xffffffffu, flags, 0x00f00000u,
                                      flags));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(12u), 0xf0000000u,
                                      0x00f00000u, flags, 0xf0f00000u,
                                      flags | XPSR_N));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(14u), 0xffffffffu,
                                      0x0f0f0f0fu, flags, 0xf0f0f0f0u,
                                      flags | XPSR_N));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(15u), 0x00000000u,
                                      0x00000000u, flags, 0xffffffffu,
                                      flags | XPSR_N));
}

static void test_carry_compare_reverse_forms(semu_test_context *context)
{
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(5u), 0xffffffffu, 0u,
                                      XPSR_T | XPSR_C, 0u,
                                      XPSR_T | XPSR_Z | XPSR_C));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(6u), 0u, 0u, XPSR_T,
                                      0xffffffffu, XPSR_T | XPSR_N));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(9u), 0x12345678u, 1u,
                                      XPSR_T, 0xffffffffu, XPSR_T | XPSR_N));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(10u), 0u, 1u, XPSR_T,
                                      0u, XPSR_T | XPSR_N));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(11u), 0x7fffffffu, 1u,
                                      XPSR_T, 0x7fffffffu,
                                      XPSR_T | XPSR_N | XPSR_V));
    SEMU_TEST_ASSERT(context, run_one(IMM(1u, 1u), 0x80000000u, 0u, XPSR_T,
                                      0x80000000u, XPSR_T | XPSR_C | XPSR_V));
}

static void test_ror_and_multiply_wrap(semu_test_context *context)
{
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(7u), 0x80000001u, 1u,
                                      XPSR_T, 0xc0000000u,
                                      XPSR_T | XPSR_N | XPSR_C));
    SEMU_TEST_ASSERT(context, run_one(SHIFT_REG(13u), 0x00010000u,
                                      0x00010000u, XPSR_T | XPSR_C | XPSR_V,
                                      0u, XPSR_T | XPSR_Z | XPSR_C | XPSR_V));
}

static void test_unsupported_refusal(semu_test_context *context)
{
    static const uint8_t program[] = {0x00u, 0xdeu, 0x00u, 0xbeu};
    semu_cpu_fixture fixture;
    semu_cpu_state before;
    semu_cpu_state *state;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program,
                                            sizeof(program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = 0x13579bdfu;
    state->r[1] = 0x2468ace0u;
    state->xpsr = XPSR_T | XPSR_N | XPSR_C | XPSR_V;
    before = *state;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_cpu_fixture_step(&fixture));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    SEMU_TEST_EQ_U64(context, before.r[0], state->r[0]);
    SEMU_TEST_EQ_U64(context, before.r[1], state->r[1]);
    SEMU_TEST_EQ_U64(context, before.xpsr, state->xpsr);
    SEMU_TEST_EQ_U64(context, before.instructions, state->instructions);
    SEMU_TEST_EQ_U64(context, SEMU_STOP_UNSUPPORTED_INSTRUCTION,
                     semu_cpu_stop_reason(fixture.cpu));
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_shift_immediate_boundaries),
        SEMU_TEST_CASE(test_shift_register_boundaries),
        SEMU_TEST_CASE(test_add_sub_forms),
        SEMU_TEST_CASE(test_immediate_forms),
        SEMU_TEST_CASE(test_logical_forms_preserve_carry_overflow),
        SEMU_TEST_CASE(test_carry_compare_reverse_forms),
        SEMU_TEST_CASE(test_ror_and_multiply_wrap),
        SEMU_TEST_CASE(test_unsupported_refusal)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
