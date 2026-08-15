#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "test.h"

#include "../../src/cpu/armv7m/armv7m_internal.h"

#include <string.h>

#define XPSR_N (1u << 31)
#define XPSR_Z (1u << 30)
#define XPSR_C (1u << 29)
#define XPSR_T (1u << 24)

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

static int run_shift(uint16_t first, uint16_t second, semu_cpu_state initial,
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
    *status = armv7m_exec32_shift(fixture.cpu, first, second, 0x100u,
                                  &fixture.error);
    *final = *semu_cpu_get_state(fixture.cpu);
    semu_cpu_fixture_destroy(&fixture);
    return 1;
}

static void test_shift_by_register(semu_test_context *context)
{
    /* LSL.W, LSR.W, ASR.W, ROR.W by register.
     * Encoding: first=0xFA00|(type<<5)|Rn, second=0xF000|(Rd<<8)|Rm
     * type: 0=LSL, 1=LSR, 2=ASR, 3=ROR */
    semu_cpu_state state = initial_state(), final;
    semu_status status;

    /* LSR.W r2, r0, r2: r0=0x80000001, r2=5 -> 0x80000001 >> 5 = 0x04000000
     * carry = bit[4] = 0 */
    state.r[0] = 0x80000001u;
    state.r[2] = 5u;
    SEMU_TEST_ASSERT(context, run_shift(0xfa20u, 0xf202u, state,
                                       &status, &final));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x04000000u, final.r[2]);

    /* LSL.W r3, r0, r1: r0=0x00000001, r1=31 -> 1 << 31 = 0x80000000
     * carry = bit[0] = 1 */
    state = initial_state();
    state.r[0] = 0x00000001u;
    state.r[1] = 31u;
    SEMU_TEST_ASSERT(context, run_shift(0xfa00u, 0xf301u, state,
                                       &status, &final));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x80000000u, final.r[3]);

    /* ASR.W r4, r0, r1: r0=0x80000000, r1=4 -> 0xF8000000
     * carry = bit[3] = 0 */
    state = initial_state();
    state.r[0] = 0x80000000u;
    state.r[1] = 4u;
    SEMU_TEST_ASSERT(context, run_shift(0xfa40u, 0xf401u, state,
                                       &status, &final));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0xf8000000u, final.r[4]);

    /* ROR.W r5, r0, r1: r0=0x00000002, r1=4 -> 0x20000000 */
    state = initial_state();
    state.r[0] = 0x00000002u;
    state.r[1] = 4u;
    SEMU_TEST_ASSERT(context, run_shift(0xfa60u, 0xf501u, state,
                                       &status, &final));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x20000000u, final.r[5]);

    /* Shift amount 0: result unchanged, carry preserved. */
    state = initial_state();
    state.r[0] = 0xdeadbeefu;
    state.r[1] = 0u;
    state.xpsr |= XPSR_C;
    SEMU_TEST_ASSERT(context, run_shift(0xfa20u, 0xf201u, state,
                                       &status, &final));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0xdeadbeefu, final.r[2]);
    SEMU_TEST_ASSERT(context, final.xpsr & XPSR_C);

    /* With S bit: flags updated. ASR.W r4, r0, r1 S=1 -> first=0xFE40
     * r0=0x80000000, r1=4 -> 0xF8000000, N=1, Z=0 */
    state = initial_state();
    state.r[0] = 0x80000000u;
    state.r[1] = 4u;
    SEMU_TEST_ASSERT(context, run_shift(0xfe40u, 0xf401u, state,
                                       &status, &final));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0xf8000000u, final.r[4]);
    SEMU_TEST_ASSERT(context, final.xpsr & XPSR_N);
    SEMU_TEST_ASSERT(context, !(final.xpsr & XPSR_Z));

    /* Refusal: Rm=PC (15) is not a data register. */
    state = initial_state();
    SEMU_TEST_ASSERT(context, run_shift(0xfa20u, 0xf20fu, state,
                                       &status, &final));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, status);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_shift_by_register)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
