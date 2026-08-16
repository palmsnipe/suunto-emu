#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "test.h"

#include "../../src/cpu/armv7m/armv7m_internal.h"

#include <string.h>

#define XPSR_N (1u << 31)
#define XPSR_Z (1u << 30)
#define XPSR_C (1u << 29)
#define XPSR_V (1u << 28)
#define XPSR_Q (1u << 27)
#define XPSR_GE (0x0fu << 16)
#define XPSR_T (1u << 24)

static semu_stop_reason last_stop;

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

static uint16_t mul_second(unsigned ra, unsigned rd, unsigned form,
                           unsigned rm)
{
    return (uint16_t)((ra << 12u) | (rd << 8u) | (form << 4u) | rm);
}

static uint16_t long_first(unsigned op, unsigned rn)
{
    return (uint16_t)(0xfb00u | (op << 4u) | rn);
}

static uint16_t long_second(unsigned hi, unsigned lo, unsigned rm, int divide)
{
    return (uint16_t)((hi << 12u) | (lo << 8u) |
                      (divide ? 0xf0u : 0u) | rm);
}

static uint16_t parallel_first(unsigned op, unsigned rn)
{
    return (uint16_t)(0xfa80u | (op << 4u) | rn);
}

static uint16_t parallel_second(unsigned form, unsigned rd, unsigned rm)
{
    return (uint16_t)(0xf000u | (rd << 8u) | form | rm);
}

static uint16_t misc_first(unsigned op, unsigned rn)
{
    return (uint16_t)(0xfa80u | (op << 4u) | rn);
}

static uint16_t misc_second(unsigned op, unsigned rd, unsigned rm)
{
    return (uint16_t)(0xf000u | (rd << 8u) | op | rm);
}

static uint16_t extend_first(unsigned base, unsigned rn)
{
    return (uint16_t)(base | rn);
}

static uint16_t extend_second(unsigned rd, unsigned rotation, unsigned rm)
{
    return (uint16_t)(0xf080u | (rd << 8u) | (rotation << 4u) | rm);
}

static uint16_t pkh_second(unsigned rd, unsigned amount, int top, unsigned rm)
{
    return (uint16_t)((amount >> 2u) << 12u | (rd << 8u) |
                      ((amount & 3u) << 6u) | (top ? 0x20u : 0u) | rm);
}

static int run_dsp(uint16_t first, uint16_t second, semu_cpu_state initial,
                   semu_status *status, semu_cpu_state *final)
{
    static const uint8_t stop[] = {0x00u, 0xbeu};
    semu_cpu_fixture fixture;

    if (!semu_cpu_fixture_init(&fixture, stop, sizeof(stop))) return 0;
    initial.r[15] = 0x104u;
    initial.instructions = 0u;
    initial.halted = 0;
    initial.waiting_for_interrupt = 0;
    semu_cpu_fixture_apply_state(&fixture, &initial);
    semu_error_clear(&fixture.error);
    *status = armv7m_exec32_dsp(fixture.cpu, first, second, 0x100u,
                                &fixture.error);
    last_stop = semu_cpu_stop_reason(fixture.cpu);
    *final = *semu_cpu_get_state(fixture.cpu);
    semu_cpu_fixture_destroy(&fixture);
    return 1;
}

static int run_cpu(uint16_t first, uint16_t second, semu_cpu_state initial,
                   semu_status *status, semu_cpu_state *final)
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
    last_stop = semu_cpu_stop_reason(fixture.cpu);
    *final = *semu_cpu_get_state(fixture.cpu);
    semu_cpu_fixture_destroy(&fixture);
    return 1;
}

static int same_except_halt(const semu_cpu_state *before,
                            const semu_cpu_state *after)
{
    semu_cpu_state copy = *after;
    copy.halted = before->halted;
    return memcmp(before, &copy, sizeof(copy)) == 0;
}

static void test_multiply_long_and_divide(semu_test_context *context)
{
    semu_cpu_state state = initial_state(), final;
    semu_status status;
    state.r[0] = 4u;
    state.r[1] = 2u;
    state.r[2] = 3u;
    SEMU_TEST_ASSERT(context, run_dsp(0xfb12u, mul_second(0u, 0u, 0u, 1u),
                                      state, &status, &final));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 10u, final.r[0]);
    state.r[0] = 2u;
    state.r[1] = 3u;
    SEMU_TEST_ASSERT(context, run_dsp(0xfb10u, mul_second(15u, 0u, 0u, 1u),
                                      state, &status, &final));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 6u, final.r[0]);
    state.r[1] = 0xffffffffu;
    state.r[2] = 2u;
    SEMU_TEST_ASSERT(context, run_cpu(0xfb01u, mul_second(15u, 0u, 0u, 2u),
                                      state, &status, &final));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0xfffffffeu, final.r[0]);
    SEMU_TEST_EQ_U64(context, 0x104u, final.r[15]);
    state = initial_state();
    state.r[1] = 0xffffffffu;
    state.r[2] = 2u;
    state.r[3] = 3u;
    SEMU_TEST_ASSERT(context, run_dsp(0xfb01u, mul_second(3u, 0u, 0u, 2u),
                                      state, &status, &final));
    SEMU_TEST_EQ_U64(context, 1u, final.r[0]);
    SEMU_TEST_ASSERT(context, run_dsp(0xfb01u, mul_second(3u, 0u, 1u, 2u),
                                      state, &status, &final));
    SEMU_TEST_EQ_U64(context, 0x00000005u, final.r[0]);
    state = initial_state();
    state.r[2] = 0xffffffffu;
    state.r[3] = 2u;
    SEMU_TEST_ASSERT(context, run_dsp(long_first(10u, 2u),
                                      long_second(1u, 0u, 3u, 0), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0xfffffffeu, final.r[0]);
    SEMU_TEST_EQ_U64(context, 1u, final.r[1]);
    state = initial_state();
    state.r[2] = 0xffffffffu;
    state.r[3] = 2u;
    SEMU_TEST_ASSERT(context, run_dsp(long_first(14u, 2u),
                                      long_second(1u, 0u, 3u, 0), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0xfffffffeu, final.r[0]);
    SEMU_TEST_EQ_U64(context, 1u, final.r[1]);
    state = initial_state();
    state.r[2] = 0xffffffffu;
    state.r[3] = 2u;
    SEMU_TEST_ASSERT(context, run_dsp(long_first(8u, 2u),
                                      long_second(1u, 0u, 3u, 0), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0xfffffffeu, final.r[0]);
    SEMU_TEST_EQ_U64(context, 0xffffffffu, final.r[1]);
    state = initial_state();
    state.r[0] = 0xffffffffu;
    state.r[1] = 0xffffffffu;
    state.r[2] = 1u;
    state.r[3] = 1u;
    SEMU_TEST_ASSERT(context, run_dsp(long_first(12u, 2u),
                                      long_second(1u, 0u, 3u, 0), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0u, final.r[0]);
    SEMU_TEST_EQ_U64(context, 0u, final.r[1]);
    state = initial_state();
    state.r[5] = 0xfffffff4u;
    state.r[6] = 3u;
    SEMU_TEST_ASSERT(context, run_dsp(long_first(9u, 5u),
                                      long_second(15u, 4u, 6u, 1), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0xfffffffcu, final.r[4]);
    state.r[5] = 10u;
    state.r[6] = 0u;
    state.xpsr |= XPSR_N | XPSR_C | XPSR_V;
    SEMU_TEST_ASSERT(context, run_dsp(long_first(11u, 5u),
                                      long_second(15u, 4u, 6u, 1), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0u, final.r[4]);
    SEMU_TEST_EQ_U64(context, XPSR_T | XPSR_N | XPSR_C | XPSR_V, final.xpsr);
    state = initial_state();
    state.r[5] = 0x80000000u;
    state.r[6] = 0u;
    state.xpsr |= XPSR_N | XPSR_C | XPSR_V;
    SEMU_TEST_ASSERT(context, run_dsp(long_first(9u, 5u),
                                      long_second(15u, 4u, 6u, 1), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0u, final.r[4]);
    SEMU_TEST_EQ_U64(context, XPSR_T | XPSR_N | XPSR_C | XPSR_V, final.xpsr);
    state = initial_state();
    state.r[5] = 0x80000000u;
    state.r[6] = 0xffffffffu;
    SEMU_TEST_ASSERT(context, run_dsp(long_first(9u, 5u),
                                      long_second(15u, 4u, 6u, 1), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0x80000000u, final.r[4]);
    state.r[5] = 0xffffffffu;
    state.r[6] = 1u;
    SEMU_TEST_ASSERT(context, run_dsp(long_first(11u, 5u),
                                      long_second(15u, 4u, 6u, 1), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0xffffffffu, final.r[4]);
}

static void test_saturation_and_q(semu_test_context *context)
{
    semu_cpu_state state = initial_state(), final;
    semu_status status;

    state.r[1] = 0x7fffffffu;
    state.r[2] = 1u;
    SEMU_TEST_ASSERT(context, run_dsp(0xfa82u, 0xf081u, state, &status,
                                      &final));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x7fffffffu, final.r[0]);
    SEMU_TEST_ASSERT(context, (final.xpsr & XPSR_Q) != 0u);
    SEMU_TEST_EQ_U64(context, 0x104u, final.r[15]);

    state = initial_state();
    state.r[1] = 7u;
    state.r[2] = 9u;
    SEMU_TEST_ASSERT(context, run_dsp(0xfa82u, 0xf0a1u, state, &status,
                                      &final));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 2u, final.r[0]);
    SEMU_TEST_EQ_U64(context, XPSR_T, final.xpsr);
    SEMU_TEST_EQ_U64(context, 0x104u, final.r[15]);

    state = initial_state();
    state.r[1] = 1u;
    state.r[2] = 2u;
    SEMU_TEST_ASSERT(context, run_dsp(0xfa82u, 0xf091u, state, &status,
                                      &final));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 4u, final.r[0]);
    SEMU_TEST_EQ_U64(context, XPSR_T, final.xpsr);
    SEMU_TEST_EQ_U64(context, 0x104u, final.r[15]);

    state = initial_state();
    state.r[1] = 10u;
    state.r[2] = 2u;
    SEMU_TEST_ASSERT(context, run_dsp(0xfa82u, 0xf0b1u, state, &status,
                                      &final));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0xffffffeeu, final.r[0]);
    SEMU_TEST_EQ_U64(context, XPSR_T, final.xpsr);
    SEMU_TEST_EQ_U64(context, 0x104u, final.r[15]);

    state = initial_state();
    state.r[1] = 0x40000000u;
    SEMU_TEST_ASSERT(context, run_dsp(0xfa82u, 0xf091u, state, &status,
                                      &final));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x7fffffffu, final.r[0]);
    SEMU_TEST_EQ_U64(context, XPSR_T | XPSR_Q, final.xpsr);
    SEMU_TEST_EQ_U64(context, 0x104u, final.r[15]);

    state = initial_state();
    state.xpsr |= XPSR_Q;
    state.r[1] = 1u;
    state.r[2] = 2u;
    SEMU_TEST_ASSERT(context, run_dsp(0xfa82u, 0xf081u, state, &status,
                                      &final));
    SEMU_TEST_EQ_U64(context, 3u, final.r[0]);
    SEMU_TEST_ASSERT(context, (final.xpsr & XPSR_Q) != 0u);

    state = initial_state();
    state.r[1] = 0xffffff38u;
    SEMU_TEST_ASSERT(context, run_dsp(0xf301u, 0x0006u, state, &status,
                                      &final));
    SEMU_TEST_EQ_U64(context, 0xffffffc0u, final.r[0]);
    SEMU_TEST_ASSERT(context, (final.xpsr & XPSR_Q) != 0u);

    state = initial_state();
    state.r[1] = 0xffffffffu;
    SEMU_TEST_ASSERT(context, run_dsp(0xf381u, 0x0007u, state, &status,
                                      &final));
    SEMU_TEST_EQ_U64(context, 0u, final.r[0]);
    SEMU_TEST_ASSERT(context, (final.xpsr & XPSR_Q) != 0u);

    state = initial_state();
    state.r[1] = 0x7fff8000u;
    SEMU_TEST_ASSERT(context, run_dsp(0xf321u, 0x000fu, state, &status,
                                      &final));
    SEMU_TEST_EQ_U64(context, 0x7fff8000u, final.r[0]);
    state.r[1] = 0xffff0080u;
    SEMU_TEST_ASSERT(context, run_dsp(0xf3a1u, 0x0008u, state, &status,
                                      &final));
    SEMU_TEST_EQ_U64(context, 0x00000080u, final.r[0]);
    SEMU_TEST_ASSERT(context, (final.xpsr & XPSR_Q) != 0u);

    state = initial_state();
    state.r[7] = 0x000100ffu;
    state.r[8] = 0x00020001u;
    SEMU_TEST_ASSERT(context, run_dsp(parallel_first(0u, 7u),
                                      parallel_second(0x40u, 6u, 8u), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0x00030000u, final.r[6]);
    SEMU_TEST_EQ_U64(context, XPSR_T | (0x01u << 16u), final.xpsr);
}

static void test_parallel_ge_and_selection(semu_test_context *context)
{
    semu_cpu_state state = initial_state(), final;
    semu_status status;

    state.r[1] = 0xff01fe80u;
    state.r[2] = 0x01feffffu;
    SEMU_TEST_ASSERT(context, run_dsp(parallel_first(0u, 1u),
                                      parallel_second(0u, 0u, 2u), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0x00fffd7fu, final.r[0]);
    SEMU_TEST_EQ_U64(context, XPSR_T | (8u << 16u), final.xpsr);

    state = initial_state();
    state.r[1] = 0x00010002u;
    state.r[2] = 0x00030001u;
    SEMU_TEST_ASSERT(context, run_dsp(parallel_first(5u, 1u),
                                      parallel_second(0x40u, 0u, 2u), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0xfffe0001u, final.r[0]);
    SEMU_TEST_EQ_U64(context, XPSR_T | (3u << 16u), final.xpsr);

    state = initial_state();
    state.r[1] = 0xffff0002u;
    state.r[2] = 0x00030004u;
    SEMU_TEST_ASSERT(context, run_dsp(parallel_first(2u, 1u),
                                      parallel_second(0u, 0u, 2u), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0x0003ffffu, final.r[0]);
    SEMU_TEST_EQ_U64(context, XPSR_T | (0x0cu << 16u), final.xpsr);
    SEMU_TEST_ASSERT(context, run_dsp(parallel_first(6u, 1u),
                                      parallel_second(0x40u, 0u, 2u), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0xfffb0005u, final.r[0]);
    SEMU_TEST_EQ_U64(context, XPSR_T | (0x0cu << 16u), final.xpsr);

    state = initial_state();
    state.xpsr |= 0x0au << 16u;
    state.r[1] = 0x11223344u;
    state.r[2] = 0xaabbccddu;
    SEMU_TEST_ASSERT(context, run_dsp(misc_first(2u, 1u),
                                      misc_second(0x80u, 0u, 2u), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0x11bb33ddu, final.r[0]);
    SEMU_TEST_EQ_U64(context, 0x0au << 16u, final.xpsr & XPSR_GE);
}

static void test_packing_extends_and_reverse(semu_test_context *context)
{
    semu_cpu_state state = initial_state(), final;
    semu_status status;
    unsigned rotation;

    state.r[4] = 0x12345678u;
    state.r[5] = 0x89abcdefu;
    SEMU_TEST_ASSERT(context, run_dsp(0xeac4u, pkh_second(3u, 7u, 0, 5u),
                                      state, &status, &final));
    SEMU_TEST_EQ_U64(context, 0xd5e65678u, final.r[3]);
    SEMU_TEST_ASSERT(context, run_dsp(0xeac4u, pkh_second(6u, 8u, 1, 5u),
                                      state, &status, &final));
    SEMU_TEST_EQ_U64(context, 0x1234abcdu, final.r[6]);

    state = initial_state();
    state.r[1] = 0x00000100u;
    state.r[2] = 0x80ff0001u;
    for (rotation = 0u; rotation < 4u; ++rotation) {
        uint32_t rotated = rotation == 0u ? state.r[2] :
                            (state.r[2] >> (rotation * 8u)) |
                            (state.r[2] << (32u - rotation * 8u));
        SEMU_TEST_ASSERT(context, run_dsp(extend_first(0xfa40u, 1u),
                                          extend_second(0u, rotation, 2u),
                                          state, &status, &final));
        SEMU_TEST_EQ_U64(context, state.r[1] +
                         (uint32_t)(int32_t)(int8_t)(rotated & 0xffu),
                         final.r[0]);
    }

    state.r[1] = 0x00010002u;
    state.r[2] = 0x80ff0001u;
    SEMU_TEST_ASSERT(context, run_dsp(extend_first(0xfa00u, 1u),
                                      extend_second(0u, 1u, 2u), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0x0000ff02u, final.r[0]);
    SEMU_TEST_ASSERT(context, run_dsp(extend_first(0xfa10u, 1u),
                                      extend_second(0u, 1u, 2u), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0x0001ff02u, final.r[0]);
    SEMU_TEST_ASSERT(context, run_dsp(extend_first(0xfa20u, 1u),
                                      extend_second(0u, 0u, 2u), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0x00000003u, final.r[0]);
    SEMU_TEST_ASSERT(context, run_dsp(extend_first(0xfa30u, 1u),
                                      extend_second(0u, 0u, 2u), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0x01000003u, final.r[0]);
    SEMU_TEST_ASSERT(context, run_dsp(extend_first(0xfa50u, 1u),
                                      extend_second(1u, 0u, 2u), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0x00010003u, final.r[1]);

    state = initial_state();
    state.r[7] = 0x12345678u;
    SEMU_TEST_ASSERT(context, run_dsp(misc_first(1u, 7u),
                                      misc_second(0x80u, 0u, 7u), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0x78563412u, final.r[0]);
    SEMU_TEST_ASSERT(context, run_dsp(misc_first(1u, 7u),
                                      misc_second(0x90u, 2u, 7u), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0x34127856u, final.r[2]);
    SEMU_TEST_ASSERT(context, run_dsp(misc_first(1u, 7u),
                                      misc_second(0xa0u, 1u, 7u), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 0x1e6a2c48u, final.r[1]);
    state.r[3] = 0x00f00000u;
    SEMU_TEST_ASSERT(context, run_dsp(misc_first(3u, 3u),
                                      misc_second(0x80u, 2u, 3u), state,
                                      &status, &final));
    SEMU_TEST_EQ_U64(context, 8u, final.r[2]);
}

static void test_reserved_forms_refuse(semu_test_context *context)
{
    static const struct { uint16_t first; uint16_t second; } cases[] = {
        {0xfb21u, 0x3002u},
        {0xfa81u, 0xf012u},
        {0xfb01u, 0xd002u},
        {0xeac1u, 0x8f02u},
        {0xfa91u, 0xf080u},
        {0xfb82u, 0x0003u},
        {0xfa8fu, 0xf0a1u}
    };
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        semu_cpu_state before = initial_state(), final, expected;
        semu_status status;
        before.r[0] = 0x13579bdfu;
        before.r[1] = 0x2468ace0u;
        before.xpsr |= XPSR_N | XPSR_C | XPSR_V | XPSR_GE;
        expected = before;
        expected.r[15] = 0x100u;
        SEMU_TEST_ASSERT(context, run_dsp(cases[index].first,
                                          cases[index].second, before,
                                          &status, &final));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, status);
        SEMU_TEST_ASSERT(context, same_except_halt(&expected, &final));
        SEMU_TEST_ASSERT(context, final.halted != 0);
        SEMU_TEST_EQ_U64(context, SEMU_STOP_UNSUPPORTED_INSTRUCTION,
                         last_stop);
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_multiply_long_and_divide),
        SEMU_TEST_CASE(test_saturation_and_q),
        SEMU_TEST_CASE(test_parallel_ge_and_selection),
        SEMU_TEST_CASE(test_packing_extends_and_reverse),
        SEMU_TEST_CASE(test_reserved_forms_refuse)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
