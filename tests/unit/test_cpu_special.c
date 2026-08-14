#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "../../src/cpu/armv7m/armv7m_internal.h"
#include "test.h"

#include <string.h>

#define XPSR_T (1u << 24)
#define XPSR_N (1u << 31)
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

static int prepare(semu_cpu_fixture *fixture, const semu_cpu_state *state)
{
    static const uint8_t program[] = {0x00u, 0xbeu};

    if (!semu_cpu_fixture_init(fixture, program, sizeof(program))) return 0;
    semu_cpu_fixture_apply_state(fixture, state);
    return 1;
}

static semu_status call_system(semu_cpu_fixture *fixture, uint16_t first,
                               uint16_t second)
{
    semu_error_clear(&fixture->error);
    return armv7m_exec32_system(fixture->cpu, first, second, 0x100u,
                                &fixture->error);
}

static int same_state_except_halted(const semu_cpu_state *before,
                                    const semu_cpu_state *after)
{
    semu_cpu_state copy = *after;

    copy.halted = before->halted;
    return memcmp(before, &copy, sizeof(copy)) == 0;
}

static void test_mrs_reads_privileged_and_unprivileged(
    semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();
    const semu_cpu_state *actual;

    state.xpsr |= XPSR_N | XPSR_Q | (0x0fu << 16u) | 7u;
    state.msp = 0x12345u;
    state.psp = 0x23456u;
    state.primask = 1u;
    state.basepri = 0x80u;
    state.faultmask = 1u;
    state.control = 5u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf3efu, 0x8000u));
    SEMU_TEST_EQ_U64(context, XPSR_N | XPSR_Q | (0x0fu << 16u),
                     semu_cpu_get_state(fixture.cpu)->r[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf3efu, 0x8103u));
    SEMU_TEST_EQ_U64(context, XPSR_N | XPSR_Q | (0x0fu << 16u) | 7u,
                     semu_cpu_get_state(fixture.cpu)->r[1]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf3efu, 0x8205u));
    SEMU_TEST_EQ_U64(context, 7u, semu_cpu_get_state(fixture.cpu)->r[2]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf3efu, 0x8306u));
    SEMU_TEST_EQ_U64(context, 0u, semu_cpu_get_state(fixture.cpu)->r[3]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf3efu, 0x8408u));
    SEMU_TEST_EQ_U64(context, 0x12344u,
                     semu_cpu_get_state(fixture.cpu)->r[4]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf3efu, 0x8509u));
    SEMU_TEST_EQ_U64(context, 0x23454u,
                     semu_cpu_get_state(fixture.cpu)->r[5]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf3efu, 0x8610u));
    SEMU_TEST_EQ_U64(context, 1u, semu_cpu_get_state(fixture.cpu)->r[6]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf3efu, 0x8711u));
    SEMU_TEST_EQ_U64(context, 0x80u, semu_cpu_get_state(fixture.cpu)->r[7]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf3efu, 0x8812u));
    SEMU_TEST_EQ_U64(context, 0x80u, semu_cpu_get_state(fixture.cpu)->r[8]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf3efu, 0x8913u));
    SEMU_TEST_EQ_U64(context, 1u, semu_cpu_get_state(fixture.cpu)->r[9]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf3efu, 0x8a14u));
    SEMU_TEST_EQ_U64(context, 1u, semu_cpu_get_state(fixture.cpu)->r[10]);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    state.control = 1u;
    state.msp = 0x12344u;
    state.psp = 0x23454u;
    state.primask = 1u;
    state.basepri = 0x80u;
    state.faultmask = 1u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    actual = semu_cpu_get_state(fixture.cpu);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf3efu, 0x8008u));
    SEMU_TEST_EQ_U64(context, 0u, actual->r[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf3efu, 0x8110u));
    SEMU_TEST_EQ_U64(context, 0u, actual->r[1]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf3efu, 0x8211u));
    SEMU_TEST_EQ_U64(context, 0u, actual->r[2]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf3efu, 0x8313u));
    SEMU_TEST_EQ_U64(context, 0u, actual->r[3]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf3efu, 0x8414u));
    SEMU_TEST_EQ_U64(context, 1u, actual->r[4]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf3efu, 0x8509u));
    SEMU_TEST_EQ_U64(context, 0u, actual->r[5]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_msr_apsr_masks_stacks_and_control(
    semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();

    state.r[0] = XPSR_N | XPSR_Q | (0x0au << 16u);
    state.r[1] = 0x00050000u;
    state.r[2] = XPSR_N | (0x03u << 16u);
    state.r[3] = 1u;
    state.r[4] = 0x80u;
    state.r[5] = 0x20u;
    state.r[6] = 0xf0u;
    state.r[7] = 0x12347u;
    state.r[8] = 0x23459u;
    state.r[9] = 3u;
    state.r[10] = 3u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf380u, 0x8800u));
    SEMU_TEST_EQ_U64(context, XPSR_N | XPSR_Q,
                     semu_cpu_get_state(fixture.cpu)->xpsr &
                     (XPSR_N | XPSR_Q));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf381u, 0x8400u));
    SEMU_TEST_EQ_U64(context, 0x05u,
                     (semu_cpu_get_state(fixture.cpu)->xpsr >> 16u) & 0xfu);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf382u, 0x8c00u));
    SEMU_TEST_EQ_U64(context, XPSR_N | (0x03u << 16u),
                     semu_cpu_get_state(fixture.cpu)->xpsr &
                     (XPSR_N | 0x000f0000u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf383u, 0x8810u));
    SEMU_TEST_EQ_U64(context, 1u, semu_cpu_get_state(fixture.cpu)->primask);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf384u, 0x8811u));
    SEMU_TEST_EQ_U64(context, 0x80u, semu_cpu_get_state(fixture.cpu)->basepri);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf385u, 0x8812u));
    SEMU_TEST_EQ_U64(context, 0x20u, semu_cpu_get_state(fixture.cpu)->basepri);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf386u, 0x8812u));
    SEMU_TEST_EQ_U64(context, 0x20u, semu_cpu_get_state(fixture.cpu)->basepri);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf387u, 0x8813u));
    SEMU_TEST_EQ_U64(context, 1u, semu_cpu_get_state(fixture.cpu)->faultmask);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf387u, 0x8808u));
    SEMU_TEST_EQ_U64(context, 0x12344u, semu_cpu_get_state(fixture.cpu)->msp);
    SEMU_TEST_EQ_U64(context, 0x12344u, semu_cpu_get_state(fixture.cpu)->r[13]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf388u, 0x8809u));
    SEMU_TEST_EQ_U64(context, 0x23458u, semu_cpu_get_state(fixture.cpu)->psp);
    SEMU_TEST_EQ_U64(context, 0x12344u, semu_cpu_get_state(fixture.cpu)->r[13]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf38au, 0x8814u));
    SEMU_TEST_EQ_U64(context, 3u, semu_cpu_get_state(fixture.cpu)->control);
    SEMU_TEST_EQ_U64(context, 0x23458u, semu_cpu_get_state(fixture.cpu)->r[13]);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    state.control = 1u;
    state.r[0] = 0xffffffffu;
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf380u, 0x8808u));
    SEMU_TEST_EQ_U64(context, 0x800u, semu_cpu_get_state(fixture.cpu)->msp);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf380u, 0x8810u));
    SEMU_TEST_EQ_U64(context, 0u, semu_cpu_get_state(fixture.cpu)->primask);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     call_system(&fixture, 0xf380u, 0x8814u));
    SEMU_TEST_EQ_U64(context, 1u, semu_cpu_get_state(fixture.cpu)->control);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_special_encoding_refusals_are_atomic(
    semu_test_context *context)
{
    static const struct {
        uint16_t first;
        uint16_t second;
    } cases[] = {
        {0xf3efu, 0x8d00u},
        {0xf3efu, 0x8004u},
        {0xf3efu, 0x9000u},
        {0xf38du, 0x8808u},
        {0xf380u, 0x8008u},
        {0xf380u, 0x880au}
    };
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        semu_cpu_fixture fixture;
        semu_cpu_state before = initial_state();
        semu_cpu_state after;
        semu_status status;

        before.r[0] = 0x13579bdfu;
        before.r[13] = before.msp = 0x900u;
        SEMU_TEST_ASSERT(context, prepare(&fixture, &before));
        status = call_system(&fixture, cases[index].first,
                             cases[index].second);
        after = *semu_cpu_get_state(fixture.cpu);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, status);
        SEMU_TEST_ASSERT(context, same_state_except_halted(&before, &after));
        SEMU_TEST_ASSERT(context, after.halted != 0);
        SEMU_TEST_EQ_U64(context, SEMU_STOP_UNSUPPORTED_INSTRUCTION,
                         semu_cpu_stop_reason(fixture.cpu));
        semu_cpu_fixture_destroy(&fixture);
    }
}

static void test_top_level_system_dispatch(semu_test_context *context)
{
    static const uint8_t program[] = {
        0xefu, 0xf3u, 0x00u, 0x80u,
        0x00u, 0xbeu
    };
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;

    state.xpsr |= XPSR_N;
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, program,
                                                    sizeof(program)));
    semu_cpu_fixture_apply_state(&fixture, &state);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, XPSR_N,
                     semu_cpu_get_state(fixture.cpu)->r[0]);
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_mrs_reads_privileged_and_unprivileged),
        SEMU_TEST_CASE(test_msr_apsr_masks_stacks_and_control),
        SEMU_TEST_CASE(test_special_encoding_refusals_are_atomic),
        SEMU_TEST_CASE(test_top_level_system_dispatch)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
