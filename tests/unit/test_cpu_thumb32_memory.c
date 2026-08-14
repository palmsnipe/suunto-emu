#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "test.h"

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

static int prepare(semu_cpu_fixture *fixture, const uint8_t *program,
                   size_t program_size, const semu_cpu_state *state)
{
    if (!semu_cpu_fixture_init(fixture, program, program_size)) return 0;
    semu_cpu_fixture_apply_state(fixture, state);
    return 1;
}

static int read_u32(const semu_cpu_fixture *fixture, uint32_t address,
                    uint32_t *value)
{
    semu_error error;
    semu_error_clear(&error);
    return semu_bus_read(fixture->bus, address, 4u, value, &error) == SEMU_OK;
}

static void test_exclusive_word_success_and_failure(semu_test_context *context)
{
    static const uint8_t success_program[] = {
        0x50u, 0xe8u, 0x00u, 0x1fu,
        0x40u, 0xe8u, 0x00u, 0x12u,
        0x00u, 0xbeu
    };
    static const uint8_t failure_program[] = {
        0x40u, 0xe8u, 0x00u, 0x12u,
        0x00u, 0xbeu
    };
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;
    semu_status status;
    uint32_t value;

    state.r[0] = 0x200u;
    state.r[1] = 0xaaaaaaaaU;
    state.r[2] = 0xbbbbbbbbU;
    SEMU_TEST_ASSERT(context, prepare(&fixture, success_program,
                                      sizeof(success_program), &state));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x200u,
                                                         0x11223344u));
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x11223344u,
                     semu_cpu_get_state(fixture.cpu)->r[1]);
    semu_cpu_get_state_mutable(fixture.cpu)->r[1] = 0x55667788u;
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0u, semu_cpu_get_state(fixture.cpu)->r[2]);
    SEMU_TEST_ASSERT(context, read_u32(&fixture, 0x200u, &value));
    SEMU_TEST_EQ_U64(context, 0x55667788u, value);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    state.r[0] = 0x200u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, failure_program,
                                      sizeof(failure_program), &state));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x200u,
                                                         0x11223344u));
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 1u, semu_cpu_get_state(fixture.cpu)->r[2]);
    SEMU_TEST_ASSERT(context, read_u32(&fixture, 0x200u, &value));
    SEMU_TEST_EQ_U64(context, 0x11223344u, value);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_subword_clrex_and_local_conflict(semu_test_context *context)
{
    static const uint8_t clrex_program[] = {
        0xd0u, 0xe8u, 0x4fu, 0x1fu,
        0xbfu, 0xf3u, 0x2fu, 0x8fu,
        0xc0u, 0xe8u, 0x42u, 0x1fu,
        0x00u, 0xbeu
    };
    static const uint8_t conflict_program[] = {
        0x50u, 0xe8u, 0x00u, 0x1fu,
        0x2du, 0xe9u, 0x08u, 0x00u,
        0x40u, 0xe8u, 0x00u, 0x12u,
        0x00u, 0xbeu
    };
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;
    semu_status status;
    uint32_t value;

    state.r[0] = 0x200u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, clrex_program,
                                      sizeof(clrex_program), &state));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x200u,
                                                         0xaabbccddU));
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0xddu, semu_cpu_get_state(fixture.cpu)->r[1]);
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 1u, semu_cpu_get_state(fixture.cpu)->r[2]);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    state.r[0] = 0x200u;
    state.r[3] = 0xdeadbeefu;
    state.r[13] = 0x204u;
    state.msp = 0x204u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, conflict_program,
                                      sizeof(conflict_program), &state));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x200u,
                                                         0x11223344u));
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 1u, semu_cpu_get_state(fixture.cpu)->r[2]);
    SEMU_TEST_ASSERT(context, read_u32(&fixture, 0x200u, &value));
    SEMU_TEST_EQ_U64(context, 0xdeadbeefu, value);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_reset_and_exception_clear_monitor(semu_test_context *context)
{
    static const uint8_t reset_program[] = {
        0x50u, 0xe8u, 0x00u, 0x1fu,
        0x40u, 0xe8u, 0x00u, 0x12u,
        0x00u, 0xbeu
    };
    static const uint8_t exception_program[] = {
        0x50u, 0xe8u, 0x00u, 0x1fu,
        0x00u, 0xbeu
    };
    static const uint8_t handler[] = {
        0x40u, 0xe8u, 0x00u, 0x12u,
        0x00u, 0xbeu
    };
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;
    semu_status status;

    state.r[0] = 0x200u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, reset_program,
                                      sizeof(reset_program), &state));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x200u,
                                                         0x11223344u));
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    semu_cpu_reset(fixture.cpu, 0u, &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, fixture.error.code);
    semu_cpu_get_state_mutable(fixture.cpu)->r[0] = 0x200u;
    semu_cpu_get_state_mutable(fixture.cpu)->r[15] = 0x104u;
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 1u, semu_cpu_get_state(fixture.cpu)->r[2]);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    state.r[0] = 0x200u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, exception_program,
                                      sizeof(exception_program), &state));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x200u,
                                                         0x11223344u));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x40u,
                                                         0x109u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_load(fixture.bus, 0x108u, handler,
                                   sizeof(handler), &fixture.error));
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    semu_cpu_set_irq(fixture.cpu, 0u, 1);
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    semu_cpu_set_irq(fixture.cpu, 0u, 0);
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 1u, semu_cpu_get_state(fixture.cpu)->r[2]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_table_branch_and_barriers(semu_test_context *context)
{
    static const uint8_t tbb_program[] = {
        0xd0u, 0xe8u, 0x01u, 0xf0u, 0x02u, 0xbeu
    };
    static const uint8_t barrier_program[] = {
        0xbfu, 0xf3u, 0x5fu, 0x8fu,
        0xbfu, 0xf3u, 0x4fu, 0x8fu,
        0xbfu, 0xf3u, 0x6fu, 0x8fu,
        0x00u, 0xbeu
    };
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;
    semu_status status;

    state.r[0] = 0x104u;
    state.r[1] = 0u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, tbb_program,
                                      sizeof(tbb_program), &state));
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x108u, semu_cpu_get_state(fixture.cpu)->r[15]);
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context, prepare(&fixture, barrier_program,
                                      sizeof(barrier_program), &state));
    status = semu_cpu_fixture_run(&fixture, 3u);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x10cu, semu_cpu_get_state(fixture.cpu)->r[15]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_multiple_store_preflight_refuses_partial_write(
    semu_test_context *context)
{
    static const uint8_t program[] = {0x2du, 0xe9u, 0x03u, 0x00u};
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;
    semu_status status;
    uint32_t value;

    state.r[13] = 0x1004u;
    state.msp = 0x1004u;
    state.r[0] = 0x11223344u;
    state.r[1] = 0x55667788u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, program, sizeof(program),
                                      &state));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0xffcu,
                                                         0xaabbccddu));
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, status);
    SEMU_TEST_EQ_U64(context, 0x1004u, semu_cpu_get_state(fixture.cpu)->r[13]);
    SEMU_TEST_ASSERT(context, read_u32(&fixture, 0xffcu, &value));
    SEMU_TEST_EQ_U64(context, 0xaabbccddu, value);
    SEMU_TEST_ASSERT(context, semu_cpu_get_state(fixture.cpu)->halted != 0);
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_exclusive_word_success_and_failure),
        SEMU_TEST_CASE(test_subword_clrex_and_local_conflict),
        SEMU_TEST_CASE(test_reset_and_exception_clear_monitor),
        SEMU_TEST_CASE(test_table_branch_and_barriers),
        SEMU_TEST_CASE(test_multiple_store_preflight_refuses_partial_write)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
