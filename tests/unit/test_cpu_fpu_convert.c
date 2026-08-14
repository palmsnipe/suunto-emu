#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "../../src/cpu/armv7m/armv7m_internal.h"
#include "../../src/cpu/armv7m/fpu_softfloat.h"
#include "test.h"

#include <string.h>

#define CPACR UINT32_C(0xe000ed88)
#define SHCSR UINT32_C(0xe000ed24)
#define IDC UINT32_C(0x80)
#define IXC UINT32_C(0x10)
#define IOC UINT32_C(1)

static int write_word(semu_cpu_fixture *fixture, uint32_t address,
                      uint32_t value)
{
    return semu_bus_write(fixture->bus, address, 4u, value,
                          &fixture->error) == SEMU_OK;
}

static int init_enabled(semu_cpu_fixture *fixture)
{
    static const uint8_t program[] = {0x00u, 0xbeu};

    return semu_cpu_fixture_init(fixture, program, sizeof(program)) &&
           write_word(fixture, CPACR, UINT32_C(0x00f00000));
}

static uint32_t expected_immediate(uint8_t immediate)
{
    unsigned bit = ((unsigned)immediate >> 6u) & 1u;
    uint32_t exponent = ((bit ^ 1u) << 7u) |
                        (bit != 0u ? UINT32_C(0x7c) : 0u) |
                        ((uint32_t)(immediate >> 4u) & 3u);

    return (((uint32_t)immediate >> 7u) << 31u) |
           (exponent << 23u) | ((uint32_t)(immediate & 15u) << 19u);
}

static void test_compare_and_conversion_helpers(semu_test_context *context)
{
    uint32_t fpscr;
    static const uint32_t positive[] = {2u, 2u, 1u, 1u};
    static const uint32_t negative[] = {UINT32_C(0xfffffffe), UINT32_C(0xffffffff),
                                        UINT32_C(0xfffffffe), UINT32_C(0xffffffff)};
    unsigned mode;

    fpscr = 0u;
    SEMU_TEST_EQ_U64(context, UINT32_C(0x80000000),
                     semu_fpu_compare_flags(0xbf800000u, 0x3f800000u,
                                             0u, &fpscr));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x60000000),
                     semu_fpu_compare_flags(0x80000000u, 0u, 0u, &fpscr));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x30000000),
                     semu_fpu_compare_flags(0x7fc12345u, 0u, 0u, &fpscr));
    SEMU_TEST_EQ_U64(context, 0u, fpscr);
    SEMU_TEST_EQ_U64(context, UINT32_C(0x30000000),
                     semu_fpu_compare_flags(0x7fc12345u, 0u, 1u, &fpscr));
    SEMU_TEST_EQ_U64(context, IOC, fpscr & IOC);
    fpscr = 0u;
    (void)semu_fpu_compare_flags(0x7f800001u, 0u, 0u, &fpscr);
    SEMU_TEST_EQ_U64(context, IOC, fpscr & IOC);
    fpscr = ARMV7M_FPSCR_FZ;
    (void)semu_fpu_compare_flags(1u, 0u, 0u, &fpscr);
    SEMU_TEST_EQ_U64(context, IDC, fpscr & IDC);

    for (mode = 0u; mode < 4u; ++mode) {
        fpscr = mode << 22u;
        SEMU_TEST_EQ_U64(context, positive[mode],
                         semu_fpu_to_int_bits(0x3fc00000u, 0u, 0u,
                                              &fpscr));
        fpscr = mode << 22u;
        SEMU_TEST_EQ_U64(context, negative[mode],
                         semu_fpu_to_int_bits(0xbfc00000u, 0u, 0u,
                                              &fpscr));
    }
    fpscr = 0u;
    SEMU_TEST_EQ_U64(context, 3u,
                     semu_fpu_to_fixed_bits(0x3fc00000u, 16u, 1u, 0u,
                                            &fpscr));
    SEMU_TEST_EQ_U64(context, UINT32_C(0xfffd),
                     semu_fpu_to_fixed_bits(0xbfc00000u, 16u, 1u, 0u,
                                            &fpscr));
    SEMU_TEST_EQ_U64(context, 0x3fc00000u,
                     semu_fpu_from_fixed_bits(3u, 16u, 1u, 0u, &fpscr));
    SEMU_TEST_EQ_U64(context, 0xbfc00000u,
                     semu_fpu_from_fixed_bits(UINT32_C(0xfffd), 16u, 1u,
                                              0u, &fpscr));
    fpscr = 0u;
    SEMU_TEST_EQ_U64(context, UINT32_C(0x80000000),
                     semu_fpu_to_int_bits(0x4f000000u, 1u, 0u, &fpscr));
    SEMU_TEST_EQ_U64(context, 0u, fpscr & IOC);
    fpscr = 0u;
    SEMU_TEST_EQ_U64(context, UINT32_C(0x7fffffff),
                     semu_fpu_to_int_bits(0x4f000000u, 0u, 0u, &fpscr));
    SEMU_TEST_EQ_U64(context, IOC, fpscr & IOC);
    fpscr = 0u;
    SEMU_TEST_EQ_U64(context, 0u,
                     semu_fpu_to_int_bits(0xbf800000u, 1u, 1u, &fpscr));
    SEMU_TEST_EQ_U64(context, IOC, fpscr & IOC);
    fpscr = 0u;
    SEMU_TEST_EQ_U64(context, 0x4f800000u,
                     semu_fpu_from_int_bits(UINT32_MAX, 1u, &fpscr));
    SEMU_TEST_EQ_U64(context, IXC, fpscr & IXC);
    SEMU_TEST_EQ_U64(context, 0x3f800000u, semu_fpu_expand_imm8(0x70u));
    SEMU_TEST_EQ_U64(context, 0x3f000000u, semu_fpu_expand_imm8(0x60u));
    SEMU_TEST_EQ_U64(context, 0xc0000000u, semu_fpu_expand_imm8(0x80u));
    for (mode = 0u; mode < 256u; ++mode)
        SEMU_TEST_EQ_U64(context, expected_immediate((uint8_t)mode),
                         semu_fpu_expand_imm8((uint8_t)mode));
}

static semu_status run_instruction(semu_cpu_fixture *fixture,
                                    uint16_t first, uint16_t second,
                                    unsigned pc)
{
    return armv7m_exec32(fixture->cpu, first, second, pc,
                         &fixture->error);
}

static void test_instruction_forms(semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state *state;

    SEMU_TEST_ASSERT(context, init_enabled(&fixture));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->s[0] = 0xbf800000u;
    state->s[1] = 0x3f800000u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     run_instruction(&fixture, 0xeeb4u, 0x0a60u, 0x100u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x80000000),
                     state->fpscr & UINT32_C(0xf0000000));
    state->s[0] = 0x7fc12345u;
    state->fpscr = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     run_instruction(&fixture, 0xeeb5u, 0x0ac0u, 0x100u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x30000000),
                     state->fpscr & UINT32_C(0xf0000000));
    SEMU_TEST_EQ_U64(context, IOC, state->fpscr & IOC);
    state->s[1] = 0x3fc00000u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     run_instruction(&fixture, 0xeebdu, 0x0ae0u, 0x100u));
    SEMU_TEST_EQ_U64(context, 1u, state->s[0]);
    state->s[1] = 0x3fc00000u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     run_instruction(&fixture, 0xeebdu, 0x0a60u, 0x100u));
    SEMU_TEST_EQ_U64(context, 2u, state->s[0]);
    state->s[1] = UINT32_MAX;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     run_instruction(&fixture, 0xeeb8u, 0x0ae0u, 0x100u));
    SEMU_TEST_EQ_U64(context, 0xbf800000u, state->s[0]);
    state->s[0] = 0x3fc00000u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     run_instruction(&fixture, 0xeebeu, 0x0a44u, 0x100u));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x180), state->s[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     run_instruction(&fixture, 0xeebau, 0x0a44u, 0x100u));
    SEMU_TEST_EQ_U64(context, 0x3fc00000u, state->s[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     run_instruction(&fixture, 0xeeb7u, 0x0a00u, 0x100u));
    SEMU_TEST_EQ_U64(context, 0x3f800000u, state->s[0]);
    state->s[1] = 0x7fc12345u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     run_instruction(&fixture, 0xeeb0u, 0x0a60u, 0x100u));
    SEMU_TEST_EQ_U64(context, 0x7fc12345u, state->s[0]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_refusals_and_access(semu_test_context *context)
{
    static const uint8_t program[] = {0x00u, 0xbeu};
    semu_cpu_fixture fixture;
    semu_cpu_state *state;
    uint32_t before;

    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, program,
                                                     sizeof(program)));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SHCSR, UINT32_C(1) << 18));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->s[0] = 0x12345678u;
    before = state->s[0];
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     run_instruction(&fixture, 0xeebdu, 0x0ae0u, 0x100u));
    SEMU_TEST_EQ_U64(context, before, state->s[0]);
    SEMU_TEST_EQ_U64(context, 6u, state->xpsr & UINT32_C(0x1ff));
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context, init_enabled(&fixture));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->s[0] = 0x11111111u;
    before = state->s[0];
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     run_instruction(&fixture, 0xeeb4u, 0x0b41u, 0x100u));
    SEMU_TEST_EQ_U64(context, before, state->s[0]);
    SEMU_TEST_ASSERT(context, state->halted != 0u);
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context, init_enabled(&fixture));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->s[0] = 0x11111111u;
    before = state->s[0];
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     run_instruction(&fixture, 0xeebeu, 0x0a69u, 0x100u));
    SEMU_TEST_EQ_U64(context, before, state->s[0]);
    SEMU_TEST_ASSERT(context, state->halted != 0u);
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_compare_and_conversion_helpers),
        SEMU_TEST_CASE(test_instruction_forms),
        SEMU_TEST_CASE(test_refusals_and_access)
    };

    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
