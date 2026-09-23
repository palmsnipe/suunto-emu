#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "test.h"

#include <string.h>

#define XPSR_T (1u << 24)
#define XPSR_Q (1u << 27)

static semu_cpu_state initial_state(void)
{
    semu_cpu_state state;
    (void)memset(&state, 0, sizeof(state));
    state.r[13] = 0x800u;
    state.r[15] = 0x100u;
    state.msp = 0x800u;
    state.xpsr = XPSR_T;
    return state;
}

static uint16_t pkh_second(unsigned rd, unsigned amount, int top,
                           unsigned rm)
{
    return (uint16_t)(((amount >> 2u) << 12u) | (rd << 8u) |
                      ((amount & 3u) << 6u) | (top ? 0x20u : 0u) | rm);
}

static uint16_t extend_second(unsigned rd, unsigned rotation, unsigned rm)
{
    return (uint16_t)(0xf080u | (rd << 8u) | (rotation << 4u) | rm);
}

static uint16_t parallel_second(unsigned form, unsigned rd, unsigned rm)
{
    return (uint16_t)(0xf000u | (rd << 8u) | form | rm);
}

static int run_cpu(uint16_t first, uint16_t second, semu_cpu_state initial,
                   semu_status *status, semu_cpu_state *final,
                   semu_stop_reason *stop)
{
    uint8_t program[] = {(uint8_t)first, (uint8_t)(first >> 8u),
                         (uint8_t)second, (uint8_t)(second >> 8u),
                         0x00u, 0xbeu};
    semu_cpu_fixture fixture;

    if (!semu_cpu_fixture_init(&fixture, program, sizeof(program))) return 0;
    initial.r[15] = 0x100u;
    initial.instructions = 0u;
    initial.halted = 0;
    initial.waiting_for_interrupt = 0;
    semu_cpu_fixture_apply_state(&fixture, &initial);
    *status = semu_cpu_fixture_step(&fixture);
    *stop = semu_cpu_stop_reason(fixture.cpu);
    *final = *semu_cpu_get_state(fixture.cpu);
    semu_cpu_fixture_destroy(&fixture);
    return 1;
}

static int same_architecture(const semu_cpu_state *before,
                             const semu_cpu_state *after)
{
    semu_cpu_state copy = *after;
    copy.halted = before->halted;
    return memcmp(before, &copy, sizeof(copy)) == 0;
}

static void test_dispatches_dsp_and_preserves_system_data_owners(
    semu_test_context *context)
{
    semu_cpu_state state = initial_state(), final;
    semu_status status;
    semu_stop_reason stop;

    state.r[4] = 0x12345678u;
    state.r[5] = 0x89abcdefu;
    SEMU_TEST_ASSERT(context, run_cpu(0xeac4u, pkh_second(3u, 7u, 0, 5u),
                                      state, &status, &final, &stop));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0xd5e65678u, final.r[3]);

    state = initial_state();
    state.r[1] = 0xffffff38u;
    SEMU_TEST_ASSERT(context, run_cpu(0xf301u, 0x0006u, state, &status,
                                      &final, &stop));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0xffffffc0u, final.r[0]);
    SEMU_TEST_ASSERT(context, (final.xpsr & XPSR_Q) != 0u);

    state = initial_state();
    state.r[1] = 0xffffffffu;
    SEMU_TEST_ASSERT(context, run_cpu(0xf381u, 0x0007u, state, &status,
                                      &final, &stop));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0u, final.r[0]);
    SEMU_TEST_ASSERT(context, (final.xpsr & XPSR_Q) != 0u);

    state = initial_state();
    state.r[1] = 0x00010002u;
    state.r[2] = 0x80ff0001u;
    SEMU_TEST_ASSERT(context, run_cpu(0xfa01u, extend_second(0u, 1u, 2u),
                                      state, &status, &final, &stop));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x0000ff02u, final.r[0]);

    state = initial_state();
    state.r[7] = 0x000100ffu;
    state.r[8] = 0x00020001u;
    SEMU_TEST_ASSERT(context, run_cpu(0xfa87u,
                                      parallel_second(0x40u, 6u, 8u), state,
                                      &status, &final, &stop));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x00030000u, final.r[6]);

    state = initial_state();
    state.r[1] = 0x00f00000u;
    SEMU_TEST_ASSERT(context, run_cpu(0xfab1u, 0xf081u, state, &status,
                                      &final, &stop));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 8u, final.r[0]);

    state = initial_state();
    state.r[0] = 0x12345678u;
    SEMU_TEST_ASSERT(context, run_cpu(0xf380u, 0x8809u, state, &status,
                                      &final, &stop));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x12345678u, final.psp);
}

static void test_dispatch_refusals_are_atomic(semu_test_context *context)
{
    static const struct {
        uint16_t first;
        uint16_t second;
    } cases[] = {
        {0xeac1u, 0x8f02u},
        {0xf301u, 0x0026u},
        {0xfa91u, 0xf080u}
    };
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        semu_cpu_state before = initial_state(), final, expected;
        semu_status status;
        semu_stop_reason stop;

        before.r[0] = 0x13579bdfu;
        before.r[1] = 0x2468ace0u;
        expected = before;
        expected.r[15] = 0x100u;
        SEMU_TEST_ASSERT(context,
                         run_cpu(cases[index].first, cases[index].second,
                                 before, &status, &final, &stop));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, status);
        SEMU_TEST_ASSERT(context, same_architecture(&expected, &final));
        SEMU_TEST_ASSERT(context, final.halted != 0);
        SEMU_TEST_EQ_U64(context, SEMU_STOP_UNSUPPORTED_INSTRUCTION, stop);
    }
}

static void test_f57f_branches_are_not_barriers(semu_test_context *context)
{
    /* Thumb BPL.W T3 and B.W T4 encodings, previously mistaken for
     * barriers. Targets are relative to the fixture's PC of 0x100.
     * F57F AF87 occurs in the firmware's software double-add routine. */
    static const struct {
        uint16_t second;
        uint32_t target;
        int conditional;
    } cases[] = {
        {0xaf87u, 0x00000012u, 1},
        {0xaf07u, 0xffffff12u, 1},
        {0x9f07u, 0xff57ff12u, 0},
        {0xbf07u, 0xffd7ff12u, 0}
    };
    size_t index;
    unsigned negative;

    for (index = 0u; index < SEMU_ARRAY_LEN(cases); ++index) {
        for (negative = 0u; negative < 2u; ++negative) {
            semu_cpu_state before = initial_state(), final, expected;
            semu_status status;
            semu_stop_reason stop;

            before.xpsr |= negative ? UINT32_C(0x80000000) : 0u;
            before.r[0] = 0x12345678u;
            before.r[14] = 0x76543211u;
            expected = before;
            expected.r[15] = cases[index].conditional && negative ?
                               0x104u : cases[index].target;
            expected.instructions = 1u;
            SEMU_TEST_ASSERT(context,
                run_cpu(0xf57fu, cases[index].second, before, &status,
                        &final, &stop));
            SEMU_TEST_EQ_U64(context, SEMU_OK, status);
            SEMU_TEST_EQ_U64(context, expected.r[15], final.r[15]);
            SEMU_TEST_ASSERT(context,
                            memcmp(&expected, &final, sizeof(final)) == 0);
        }
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_dispatches_dsp_and_preserves_system_data_owners),
        SEMU_TEST_CASE(test_dispatch_refusals_are_atomic),
        SEMU_TEST_CASE(test_f57f_branches_are_not_barriers)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
