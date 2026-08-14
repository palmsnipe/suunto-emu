#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "../../src/cpu/armv7m/armv7m_internal.h"
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

static void test_subword_success_and_exact_address(semu_test_context *context)
{
    static const uint8_t byte_program[] = {
        0xd0u, 0xe8u, 0x4fu, 0x1fu,
        0xc0u, 0xe8u, 0x42u, 0x1fu,
        0x00u, 0xbeu
    };
    static const uint8_t half_program[] = {
        0xd0u, 0xe8u, 0x5fu, 0x1fu,
        0xc0u, 0xe8u, 0x52u, 0x1fu,
        0x00u, 0xbeu
    };
    static const uint8_t mismatch_program[] = {
        0xd0u, 0xe8u, 0x4fu, 0x1fu,
        0xc3u, 0xe8u, 0x42u, 0x1fu,
        0x00u, 0xbeu
    };
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;
    uint32_t value;

    state.r[0] = 0x201u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, byte_program,
                                      sizeof(byte_program), &state));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x200u,
                                                         0x11223344u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0x33u, semu_cpu_get_state(fixture.cpu)->r[1]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0u, semu_cpu_get_state(fixture.cpu)->r[2]);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    state.r[0] = 0x202u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, half_program,
                                      sizeof(half_program), &state));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x200u,
                                                         0x11223344u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0x1122u, semu_cpu_get_state(fixture.cpu)->r[1]);
    semu_cpu_get_state_mutable(fixture.cpu)->r[1] = 0xa5b6u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0u, semu_cpu_get_state(fixture.cpu)->r[2]);
    SEMU_TEST_ASSERT(context, read_u32(&fixture, 0x200u, &value));
    SEMU_TEST_EQ_U64(context, 0xa5b63344u, value);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    state.r[0] = 0x201u;
    state.r[3] = 0x203u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, mismatch_program,
                                      sizeof(mismatch_program), &state));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x200u,
                                                         0x11223344u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 1u, semu_cpu_get_state(fixture.cpu)->r[2]);
    SEMU_TEST_ASSERT(context, read_u32(&fixture, 0x200u, &value));
    SEMU_TEST_EQ_U64(context, 0x11223344u, value);
    semu_cpu_fixture_destroy(&fixture);

}

static void test_tbh_and_malformed_forms(semu_test_context *context)
{
    static const uint8_t tbh_program[] = {
        0xd0u, 0xe8u, 0x11u, 0xf0u,
        0x02u, 0x00u, 0x00u, 0xbeu
    };
    static const uint8_t malformed_load[] = {
        0xd0u, 0xe8u, 0x40u, 0x1fu
    };
    static const uint8_t malformed_table[] = {
        0xd0u, 0xe8u, 0x20u, 0xf0u
    };
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;
    semu_status status;

    state.r[0] = 0x104u;
    state.r[1] = 0u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, tbh_program,
                                      sizeof(tbh_program), &state));
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x108u, semu_cpu_get_state(fixture.cpu)->r[15]);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    state.r[1] = 0xa5u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, malformed_load,
                                      sizeof(malformed_load), &state));
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, status);
    SEMU_TEST_EQ_U64(context, 0x100u, semu_cpu_get_state(fixture.cpu)->r[15]);
    SEMU_TEST_EQ_U64(context, 0xa5u, semu_cpu_get_state(fixture.cpu)->r[1]);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    SEMU_TEST_ASSERT(context, prepare(&fixture, malformed_table,
                                      sizeof(malformed_table), &state));
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, status);
    SEMU_TEST_EQ_U64(context, 0x100u, semu_cpu_get_state(fixture.cpu)->r[15]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_address_overflow_refusals(semu_test_context *context)
{
    static const uint8_t stop_program[] = {0x00u, 0xbeu};
    semu_cpu_state state;
    semu_cpu_fixture fixture;
    semu_status status;

    state = initial_state();
    state.r[0] = UINT32_MAX;
    state.r[1] = 0xa5u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, stop_program,
                                      sizeof(stop_program), &state));
    status = armv7m_exec32(fixture.cpu, 0xf890u, 0x1fffu, 0x100u,
                           &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, status);
    SEMU_TEST_EQ_U64(context, 0x100u, semu_cpu_get_state(fixture.cpu)->r[15]);
    SEMU_TEST_EQ_U64(context, 0xa5u, semu_cpu_get_state(fixture.cpu)->r[1]);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    SEMU_TEST_ASSERT(context, prepare(&fixture, stop_program,
                                      sizeof(stop_program), &state));
    status = armv7m_exec32(fixture.cpu, 0xf8dfu, 0x1000u, UINT32_MAX - 3u,
                           &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, status);
    SEMU_TEST_EQ_U64(context, UINT32_MAX - 3u,
                     semu_cpu_get_state(fixture.cpu)->r[15]);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    state.r[0] = 0u;
    state.r[1] = 0xa5u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, stop_program,
                                      sizeof(stop_program), &state));
    status = armv7m_exec32(fixture.cpu, 0xf850u, 0x1d04u, 0x100u,
                           &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, status);
    SEMU_TEST_EQ_U64(context, 0x100u, semu_cpu_get_state(fixture.cpu)->r[15]);
    SEMU_TEST_EQ_U64(context, 0xa5u, semu_cpu_get_state(fixture.cpu)->r[1]);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    SEMU_TEST_ASSERT(context, prepare(&fixture, stop_program,
                                      sizeof(stop_program), &state));
    status = armv7m_exec32(fixture.cpu, 0xe8dfu, 0xf001u,
                           UINT32_MAX - 3u, &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, status);
    SEMU_TEST_EQ_U64(context, UINT32_MAX - 3u,
                     semu_cpu_get_state(fixture.cpu)->r[15]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_single_data_transfer_matrix(semu_test_context *context)
{
    static const uint8_t program[] = {
        0x90u, 0xf8u, 0x04u, 0x10u,
        0xb0u, 0xf8u, 0x04u, 0x20u,
        0x90u, 0xf9u, 0x04u, 0x30u,
        0xb0u, 0xf9u, 0x04u, 0x40u,
        0x50u, 0xf8u, 0x27u, 0x50u,
        0x10u, 0xf9u, 0x27u, 0x60u,
        0x50u, 0xf8u, 0x04u, 0x8eu,
        0x10u, 0xf8u, 0x04u, 0x9eu,
        0x30u, 0xf8u, 0x04u, 0xae,
        0x10u, 0xf9u, 0x04u, 0xbeu,
        0x40u, 0xf8u, 0x08u, 0x1eu,
        0x00u, 0xf8u, 0x09u, 0x1eu,
        0xa0u, 0xf8u, 0x0cu, 0x20u
    };
    static const uint8_t negative_literal[] = {
        0x1fu, 0xf9u, 0x08u, 0x10u
    };
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;
    uint32_t value;

    state.r[0] = 0x200u;
    state.r[7] = 1u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, program, sizeof(program),
                                      &state));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x204u,
                                                         0xa1b280ffu));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0xffu, semu_cpu_get_state(fixture.cpu)->r[1]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0x80ffu, semu_cpu_get_state(fixture.cpu)->r[2]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0xffffffffu,
                     semu_cpu_get_state(fixture.cpu)->r[3]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0xffff80ffu,
                     semu_cpu_get_state(fixture.cpu)->r[4]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0xa1b280ffu,
                     semu_cpu_get_state(fixture.cpu)->r[5]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0xffffffffu,
                     semu_cpu_get_state(fixture.cpu)->r[6]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0xa1b280ffu,
                     semu_cpu_get_state(fixture.cpu)->r[8]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0xffu, semu_cpu_get_state(fixture.cpu)->r[9]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0x80ffu,
                     semu_cpu_get_state(fixture.cpu)->r[10]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0xffffffffu,
                     semu_cpu_get_state(fixture.cpu)->r[11]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_ASSERT(context, read_u32(&fixture, 0x208u, &value));
    SEMU_TEST_EQ_U64(context, 0x0000ffffu, value);
    SEMU_TEST_ASSERT(context, read_u32(&fixture, 0x20cu, &value));
    SEMU_TEST_EQ_U64(context, 0x000080ffu, value);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    SEMU_TEST_ASSERT(context, prepare(&fixture, negative_literal,
                                      sizeof(negative_literal), &state));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0xfcu,
                                                         0x11223380u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0xffffff80u,
                     semu_cpu_get_state(fixture.cpu)->r[1]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_single_data_transfer_refusals(semu_test_context *context)
{
    static const uint8_t stop_program[] = {0x00u, 0xbeu};
    semu_cpu_state state;
    semu_cpu_fixture fixture;
    semu_status status;

    state = initial_state();
    state.r[1] = 0xa5u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, stop_program,
                                      sizeof(stop_program), &state));
    status = armv7m_exec32(fixture.cpu, 0xf850u, 0x1062u, 0x100u,
                           &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, status);
    SEMU_TEST_EQ_U64(context, 0x100u, semu_cpu_get_state(fixture.cpu)->r[15]);
    SEMU_TEST_EQ_U64(context, 0xa5u, semu_cpu_get_state(fixture.cpu)->r[1]);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    state.r[15] = 0x100u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, stop_program,
                                      sizeof(stop_program), &state));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x204u,
                                                         0x11223344u));
    status = armv7m_exec32(fixture.cpu, 0xf890u, 0xf004u, 0x100u,
                           &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, status);
    SEMU_TEST_EQ_U64(context, 0x100u, semu_cpu_get_state(fixture.cpu)->r[15]);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    state.r[0] = 0x200u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, stop_program,
                                      sizeof(stop_program), &state));
    status = armv7m_exec32(fixture.cpu, 0xf850u, 0x0f04u, 0x100u,
                           &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, status);
    SEMU_TEST_EQ_U64(context, 0x200u, semu_cpu_get_state(fixture.cpu)->r[0]);
    SEMU_TEST_EQ_U64(context, 0x100u, semu_cpu_get_state(fixture.cpu)->r[15]);
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_subword_success_and_exact_address),
        SEMU_TEST_CASE(test_tbh_and_malformed_forms),
        SEMU_TEST_CASE(test_address_overflow_refusals),
        SEMU_TEST_CASE(test_single_data_transfer_matrix),
        SEMU_TEST_CASE(test_single_data_transfer_refusals)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
