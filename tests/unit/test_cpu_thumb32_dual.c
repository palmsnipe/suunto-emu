#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "test.h"

#include "../../src/cpu/armv7m/armv7m_internal.h"

#include <string.h>

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

static int read_u32(const semu_cpu_fixture *fixture, uint32_t address,
                    uint32_t *value)
{
    semu_error error;
    semu_error_clear(&error);
    return semu_bus_read(fixture->bus, address, 4u, value, &error) == SEMU_OK;
}

/* STRD r2, r3, [r4, #8]  →  first=0xe9c4, second=0x2302
 * STRD stores Rt at [addr] and Rt2 at [addr+4]. */
static void test_strd_offset(semu_test_context *context)
{
    static const uint8_t program[] = {
        0xc4u, 0xe9u, 0x02u, 0x23u,  /* strd r2, r3, [r4, #8] */
        0x00u, 0xbeu
    };
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;
    semu_status status;
    uint32_t value;

    state.r[4] = 0x200u;
    state.r[2] = 0x11223344u;
    state.r[3] = 0x55667788u;
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, program,
                                                      sizeof(program)));
    semu_cpu_fixture_apply_state(&fixture, &state);
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_ASSERT(context, read_u32(&fixture, 0x208u, &value));
    SEMU_TEST_EQ_U64(context, 0x11223344u, value);
    SEMU_TEST_ASSERT(context, read_u32(&fixture, 0x20cu, &value));
    SEMU_TEST_EQ_U64(context, 0x55667788u, value);
    SEMU_TEST_EQ_U64(context, 0x200u,
                     semu_cpu_get_state(fixture.cpu)->r[4]);
    semu_cpu_fixture_destroy(&fixture);
}

/* LDRD r2, r3, [r4, #8]  →  first=0xe9d4, second=0x2302
 * LDRD loads Rt from [addr] and Rt2 from [addr+4]. */
static void test_ldrd_offset(semu_test_context *context)
{
    static const uint8_t program[] = {
        0xd4u, 0xe9u, 0x02u, 0x23u,  /* ldrd r2, r3, [r4, #8] */
        0x00u, 0xbeu
    };
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;
    semu_status status;

    state.r[4] = 0x200u;
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, program,
                                                      sizeof(program)));
    semu_cpu_fixture_apply_state(&fixture, &state);
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x208u,
                                                         0xaabbccddu));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x20cu,
                                                         0x99887766u));
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0xaabbccddu,
                     semu_cpu_get_state(fixture.cpu)->r[2]);
    SEMU_TEST_EQ_U64(context, 0x99887766u,
                     semu_cpu_get_state(fixture.cpu)->r[3]);
    semu_cpu_fixture_destroy(&fixture);
}

/* STRD r2, r3, [r4, #8]!  →  first=0xe9e4, second=0x2302
 * Pre-indexed with writeback: addr = r4+8, r4 = r4+8. */
static void test_strd_pre_writeback(semu_test_context *context)
{
    static const uint8_t program[] = {
        0xe4u, 0xe9u, 0x02u, 0x23u,  /* strd r2, r3, [r4, #8]! */
        0x00u, 0xbeu
    };
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;
    semu_status status;
    uint32_t value;

    state.r[4] = 0x200u;
    state.r[2] = 0xdeadbeefu;
    state.r[3] = 0xcafef00du;
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, program,
                                                      sizeof(program)));
    semu_cpu_fixture_apply_state(&fixture, &state);
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_ASSERT(context, read_u32(&fixture, 0x208u, &value));
    SEMU_TEST_EQ_U64(context, 0xdeadbeefu, value);
    SEMU_TEST_ASSERT(context, read_u32(&fixture, 0x20cu, &value));
    SEMU_TEST_EQ_U64(context, 0xcafef00du, value);
    SEMU_TEST_EQ_U64(context, 0x208u,
                     semu_cpu_get_state(fixture.cpu)->r[4]);
    semu_cpu_fixture_destroy(&fixture);
}

/* LDRD r2, r3, [r4], #8  →  first=0xe8f4, second=0x2302
 * Post-indexed: addr = r4, r4 = r4+8. */
static void test_ldrd_post_indexed(semu_test_context *context)
{
    static const uint8_t program[] = {
        0xf4u, 0xe8u, 0x02u, 0x23u,  /* ldrd r2, r3, [r4], #8 */
        0x00u, 0xbeu
    };
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;
    semu_status status;

    state.r[4] = 0x200u;
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, program,
                                                      sizeof(program)));
    semu_cpu_fixture_apply_state(&fixture, &state);
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x200u,
                                                         0x11112222u));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x204u,
                                                         0x33334444u));
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x11112222u,
                     semu_cpu_get_state(fixture.cpu)->r[2]);
    SEMU_TEST_EQ_U64(context, 0x33334444u,
                     semu_cpu_get_state(fixture.cpu)->r[3]);
    SEMU_TEST_EQ_U64(context, 0x208u,
                     semu_cpu_get_state(fixture.cpu)->r[4]);
    semu_cpu_fixture_destroy(&fixture);
}

/* STRD with negative offset: strd r2, r3, [r4, #-8]  →  0xe944 0x2302 */
static void test_strd_negative_offset(semu_test_context *context)
{
    static const uint8_t program[] = {
        0x44u, 0xe9u, 0x02u, 0x23u,  /* strd r2, r3, [r4, #-8] */
        0x00u, 0xbeu
    };
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;
    semu_status status;
    uint32_t value;

    state.r[4] = 0x210u;
    state.r[2] = 0xbabe0010u;
    state.r[3] = 0xbabe0020u;
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, program,
                                                      sizeof(program)));
    semu_cpu_fixture_apply_state(&fixture, &state);
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_ASSERT(context, read_u32(&fixture, 0x208u, &value));
    SEMU_TEST_EQ_U64(context, 0xbabe0010u, value);
    SEMU_TEST_ASSERT(context, read_u32(&fixture, 0x20cu, &value));
    SEMU_TEST_EQ_U64(context, 0xbabe0020u, value);
    semu_cpu_fixture_destroy(&fixture);
}

/* Refusals: Rn=PC, Rt=PC, writeback with Rn==Rt, unaligned base.
 * Uses armv7m_exec32 (top-level dispatcher) to ensure proper PC setup. */
static void test_strd_ldrd_refusals(semu_test_context *context)
{
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;
    semu_status status;

    /* Rn=PC (r15): STRD r2, r3, [r15, #8] → first=0xe9cf */
    state.r[4] = 0x200u;
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture,
        (const uint8_t[]){0x00u, 0xbeu}, 2u));
    semu_cpu_fixture_apply_state(&fixture, &state);
    status = armv7m_exec32(fixture.cpu, 0xe9cfu, 0x2302u, 0x100u,
                           &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, status);
    semu_cpu_fixture_destroy(&fixture);

    /* Rt=PC: STRD r15, r3, [r4, #8] → second=0xf302 */
    state = initial_state();
    state.r[4] = 0x200u;
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture,
        (const uint8_t[]){0x00u, 0xbeu}, 2u));
    semu_cpu_fixture_apply_state(&fixture, &state);
    status = armv7m_exec32(fixture.cpu, 0xe9c4u, 0xf302u, 0x100u,
                           &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, status);
    semu_cpu_fixture_destroy(&fixture);

    /* Writeback with Rn==Rt: STRD r4, r3, [r4, #8]! → 0xe9e4 0x4302 */
    state = initial_state();
    state.r[4] = 0x200u;
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture,
        (const uint8_t[]){0x00u, 0xbeu}, 2u));
    semu_cpu_fixture_apply_state(&fixture, &state);
    status = armv7m_exec32(fixture.cpu, 0xe9e4u, 0x4302u, 0x100u,
                           &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, status);
    SEMU_TEST_EQ_U64(context, 0x200u,
                     semu_cpu_get_state(fixture.cpu)->r[4]);
    semu_cpu_fixture_destroy(&fixture);

    /* Unaligned base address */
    state = initial_state();
    state.r[4] = 0x202u;
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture,
        (const uint8_t[]){0x00u, 0xbeu}, 2u));
    semu_cpu_fixture_apply_state(&fixture, &state);
    status = armv7m_exec32(fixture.cpu, 0xe9c4u, 0x2302u, 0x100u,
                           &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, status);
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_strd_offset),
        SEMU_TEST_CASE(test_ldrd_offset),
        SEMU_TEST_CASE(test_strd_pre_writeback),
        SEMU_TEST_CASE(test_ldrd_post_indexed),
        SEMU_TEST_CASE(test_strd_negative_offset),
        SEMU_TEST_CASE(test_strd_ldrd_refusals)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
