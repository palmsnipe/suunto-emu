#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "../../src/cpu/armv7m/armv7m_internal.h"
#include "../../src/cpu/armv7m/fpu_softfloat.h"
#include "test.h"

#include <string.h>

#define SCS 0xe000e000u
#define CPACR (SCS + 0xd88u)
#define SHCSR (SCS + 0xd24u)
#define IDC (1u << 7)
#define IXC (1u << 4)
#define UFC (1u << 3)
#define OFC (1u << 2)
#define DZC (1u << 1)
#define IOC 1u

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
           write_word(fixture, CPACR, 0x00f00000u);
}

static uint16_t tri_first(uint16_t base, unsigned d, unsigned n)
{
    return (uint16_t)(base | ((d & 1u) << 6u) | ((n >> 1u) & 15u));
}

static uint16_t tri_second(uint16_t operation, unsigned d, unsigned n,
                           unsigned m)
{
    return (uint16_t)(UINT16_C(0x0a00) | operation |
                      ((d >> 1u) << 12u) | ((n & 1u) << 7u) |
                      ((m & 1u) << 5u) | ((m >> 1u) & 15u));
}

static uint16_t unary_first(uint16_t base, unsigned d)
{
    return (uint16_t)(base | ((d & 1u) << 6u));
}

static uint16_t unary_second(uint16_t base, unsigned d, unsigned m)
{
    return (uint16_t)(base | ((d >> 1u) << 12u) |
                      ((m & 1u) << 5u) | ((m >> 1u) & 15u));
}

static void test_helpers_normal_ties_and_signed_zero(semu_test_context *context)
{
    semu_fpu_eval result;
    static const uint32_t tie_expected[] = {
        0x3f800000u, 0x3f800001u, 0x3f800000u, 0x3f800000u
    };
    static const uint32_t negative_tie_expected[] = {
        0xbf800000u, 0xbf800000u, 0xbf800001u, 0xbf800000u
    };
    unsigned mode;

    result = semu_fpu_add_bits(0x3f800000u, 0x40000000u, 0u);
    SEMU_TEST_EQ_U64(context, 0x40400000u, result.bits);
    SEMU_TEST_EQ_U64(context, 0u, result.fpscr);
    result = semu_fpu_sub_bits(0x40400000u, 0x3f800000u, 0u);
    SEMU_TEST_EQ_U64(context, 0x40000000u, result.bits);
    result = semu_fpu_mul_bits(0x3fc00000u, 0x40000000u, 0u);
    SEMU_TEST_EQ_U64(context, 0x40400000u, result.bits);
    result = semu_fpu_div_bits(0x40c00000u, 0x40000000u, 0u);
    SEMU_TEST_EQ_U64(context, 0x40400000u, result.bits);
    SEMU_TEST_EQ_U64(context, 0x7fc12345u,
                     semu_fpu_abs_bits(0xffc12345u));
    SEMU_TEST_EQ_U64(context, 0xffc12345u,
                     semu_fpu_neg_bits(0x7fc12345u));
    for (mode = 0u; mode < 4u; ++mode) {
        result = semu_fpu_add_bits(0x3f800000u, 0x33800000u,
                                   mode << 22u);
        SEMU_TEST_EQ_U64(context, tie_expected[mode], result.bits);
        result = semu_fpu_add_bits(0xbf800000u, 0xb3800000u,
                                   mode << 22u);
        SEMU_TEST_EQ_U64(context, negative_tie_expected[mode], result.bits);
    }
    result = semu_fpu_add_bits(0x00000000u, 0x80000000u, 0u);
    SEMU_TEST_EQ_U64(context, 0x00000000u, result.bits);
    result = semu_fpu_add_bits(0x00000000u, 0x80000000u, 2u << 22u);
    SEMU_TEST_EQ_U64(context, 0x80000000u, result.bits);
    result = semu_fpu_add_bits(0x80000000u, 0x80000000u, 0u);
    SEMU_TEST_EQ_U64(context, 0x80000000u, result.bits);
    result = semu_fpu_add_bits(0x3f800000u, 0xbf800000u, 2u << 22u);
    SEMU_TEST_EQ_U64(context, 0x80000000u, result.bits);
    result = semu_fpu_sub_bits(0x3f800000u, 0x3f800000u, 2u << 22u);
    SEMU_TEST_EQ_U64(context, 0x80000000u, result.bits);
}

static void test_special_values_flags_and_fz(semu_test_context *context)
{
    semu_fpu_eval result;
    unsigned mode;

    result = semu_fpu_add_bits(1u, 1u, 0u);
    SEMU_TEST_EQ_U64(context, 2u, result.bits);
    result = semu_fpu_add_bits(0x7f7fffffu, 0x7f7fffffu, 0u);
    SEMU_TEST_EQ_U64(context, 0x7f800000u, result.bits);
    SEMU_TEST_EQ_U64(context, OFC | IXC, result.fpscr & (OFC | IXC));
    for (mode = 1u; mode < 4u; ++mode) {
        result = semu_fpu_add_bits(0x7f7fffffu, 0x7f7fffffu,
                                   mode << 22u);
        if (mode == 1u)
            SEMU_TEST_EQ_U64(context, 0x7f800000u, result.bits);
        else
            SEMU_TEST_EQ_U64(context, 0x7f7fffffu, result.bits);
    }
    result = semu_fpu_add_bits(0x7f800000u, 0xff800000u, 0u);
    SEMU_TEST_EQ_U64(context, 0x7fc00000u, result.bits);
    SEMU_TEST_EQ_U64(context, IOC, result.fpscr & IOC);
    result = semu_fpu_add_bits(0x7fc01234u, 0x3f800000u, 0u);
    SEMU_TEST_EQ_U64(context, 0x7fc01234u, result.bits);
    result = semu_fpu_add_bits(0x7fa00123u, 0x3f800000u, 0u);
    SEMU_TEST_EQ_U64(context, 0x7fe00123u, result.bits);
    SEMU_TEST_EQ_U64(context, IOC, result.fpscr & IOC);
    result = semu_fpu_add_bits(0x7fc01234u, 0x3f800000u, 1u << 25u);
    SEMU_TEST_EQ_U64(context, 0x7fc00000u, result.bits);
    result = semu_fpu_div_bits(0x3f800000u, 0u, 0u);
    SEMU_TEST_EQ_U64(context, 0x7f800000u, result.bits);
    SEMU_TEST_EQ_U64(context, DZC, result.fpscr & DZC);
    result = semu_fpu_div_bits(0u, 0u, 0u);
    SEMU_TEST_EQ_U64(context, 0x7fc00000u, result.bits);
    SEMU_TEST_EQ_U64(context, IOC, result.fpscr & IOC);
    result = semu_fpu_add_bits(1u, 1u, IXC | DZC);
    SEMU_TEST_EQ_U64(context, IXC | DZC,
                     result.fpscr & (IXC | DZC));
    result = semu_fpu_add_bits(1u, 1u, 0u);
    SEMU_TEST_EQ_U64(context, 0x00000002u, result.bits);
    result = semu_fpu_add_bits(0x00000001u, 0x00000001u, 0u);
    SEMU_TEST_EQ_U64(context, 0x00000002u, result.bits);
    SEMU_TEST_EQ_U64(context, 0u, result.fpscr & (UFC | IXC));
    result = semu_fpu_add_bits(0x007fffffu, 0x00000001u, 0u);
    SEMU_TEST_EQ_U64(context, 0x00800000u, result.bits);
    SEMU_TEST_EQ_U64(context, 0u, result.fpscr & (UFC | IXC));
    result = semu_fpu_mul_bits(0x00800000u, 0x3f000000u, 0u);
    SEMU_TEST_EQ_U64(context, 0x00400000u, result.bits);
    SEMU_TEST_EQ_U64(context, 0u, result.fpscr & (UFC | IXC));
    result = semu_fpu_div_bits(0x00000001u, 0x40000000u, 0u);
    SEMU_TEST_EQ_U64(context, 0u, result.bits);
    SEMU_TEST_EQ_U64(context, UFC | IXC,
                     result.fpscr & (UFC | IXC));
    result = semu_fpu_mul_bits(0x00800000u, 0x3f000000u,
                               1u << 24u);
    SEMU_TEST_EQ_U64(context, 0u, result.bits);
    SEMU_TEST_EQ_U64(context, UFC, result.fpscr & UFC);
    SEMU_TEST_EQ_U64(context, 0u, result.fpscr & IXC);
    result = semu_fpu_add_bits(0x00000001u, 0x3f800000u,
                               (1u << 24u) | IDC);
    SEMU_TEST_EQ_U64(context, 0x3f800000u, result.bits);
    SEMU_TEST_EQ_U64(context, IDC, result.fpscr & IDC);
}

static void test_sqrt_and_mul_add(semu_test_context *context)
{
    semu_fpu_eval result;

    result = semu_fpu_sqrt_bits(0x40800000u, 0u);
    SEMU_TEST_EQ_U64(context, 0x40000000u, result.bits);
    result = semu_fpu_sqrt_bits(0x40000000u, 0u);
    SEMU_TEST_EQ_U64(context, 0x3fb504f3u, result.bits);
    SEMU_TEST_EQ_U64(context, IXC, result.fpscr & IXC);
    result = semu_fpu_sqrt_bits(0xbf800000u, 0u);
    SEMU_TEST_EQ_U64(context, 0x7fc00000u, result.bits);
    SEMU_TEST_EQ_U64(context, IOC, result.fpscr & IOC);
    result = semu_fpu_mul_add_bits(0x3f800000u, 0x40000000u,
                                   0x40400000u, 0u);
    SEMU_TEST_EQ_U64(context, 0x40e00000u, result.bits);
    SEMU_TEST_EQ_U64(context, 0u, result.fpscr);
}

static void test_decoder_ready_entry_and_refusals(semu_test_context *context)
{
    static const uint8_t program[] = {0x00u, 0xbeu};
    semu_cpu_fixture fixture;
    semu_cpu_state *state;
    semu_cpu_state before;
    semu_error error_before;
    semu_status status;

    SEMU_TEST_ASSERT(context, init_enabled(&fixture));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->s[7] = 0x3f800000u;
    state->s[8] = 0x40000000u;
    status = armv7m_fpu_arith(fixture.cpu, tri_first(0xee30u, 6u, 7u),
                              tri_second(0u, 6u, 7u, 8u), &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x40400000u, state->s[6]);
    state->s[1] = 0xffc12345u;
    status = armv7m_fpu_arith(fixture.cpu,
                              unary_first(0xeeb0u, 0u),
                              unary_second(0x0ac0u, 0u, 1u),
                              &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x7fc12345u, state->s[0]);
    before = *state;
    fixture.error.code = SEMU_ERR_STATE;
    (void)strcpy(fixture.error.text, "sentinel");
    error_before = fixture.error;
    status = armv7m_fpu_arith(fixture.cpu, 0xee00u, 0x0a10u,
                              &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, status);
    SEMU_TEST_ASSERT(context,
                     memcmp(&before, semu_cpu_get_state(fixture.cpu),
                            sizeof(before)) == 0);
    SEMU_TEST_ASSERT(context,
                     memcmp(&error_before, &fixture.error,
                            sizeof(error_before)) == 0);
    status = armv7m_fpu_arith(fixture.cpu, 0xee30u, 0x0b80u,
                              &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, status);
    SEMU_TEST_ASSERT(context, state->halted != 0);
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, program,
                                                     sizeof(program)));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SHCSR, 1u << 18));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    before = *state;
    status = armv7m_fpu_arith(fixture.cpu, tri_first(0xee30u, 6u, 7u),
                              tri_second(0u, 6u, 7u, 8u), &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x80000u, fixture.cpu->cfsr);
    SEMU_TEST_EQ_U64(context, 6u, state->xpsr & 0x1ffu);
    SEMU_TEST_EQ_U64(context, before.s[6], state->s[6]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_multiply_accumulate_variants(semu_test_context *context)
{
    static const uint16_t first[] = {0xee00u, 0xee00u, 0xee10u,
                                     0xee10u, 0xee20u};
    static const uint16_t second[] = {0x0a81u, 0x0ac1u, 0x0ac1u,
                                      0x0a81u, 0x0ac1u};
    static const uint32_t expected[] = {
        0x40e00000u, 0xc0a00000u, 0xc0e00000u, 0x40a00000u,
        0xc0c00000u
    };
    semu_cpu_fixture fixture;
    semu_cpu_state *state;
    unsigned index;

    SEMU_TEST_ASSERT(context, init_enabled(&fixture));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->s[0] = 0x3f800000u;
    state->s[1] = 0x40000000u;
    state->s[2] = 0x40400000u;
    for (index = 0u; index < SEMU_ARRAY_LEN(first); ++index) {
        state->s[0] = 0x3f800000u;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         armv7m_fpu_arith(fixture.cpu, first[index],
                                           second[index], &fixture.error));
        SEMU_TEST_EQ_U64(context, expected[index], state->s[0]);
    }
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_helpers_normal_ties_and_signed_zero),
        SEMU_TEST_CASE(test_special_values_flags_and_fz),
        SEMU_TEST_CASE(test_sqrt_and_mul_add),
        SEMU_TEST_CASE(test_decoder_ready_entry_and_refusals),
        SEMU_TEST_CASE(test_multiply_accumulate_variants)
    };

    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
