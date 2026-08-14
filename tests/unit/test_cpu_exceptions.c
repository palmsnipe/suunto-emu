#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "../../src/cpu/armv7m/armv7m_internal.h"
#include "test.h"

#include <string.h>

#define XPSR_T (1u << 24)
#define XPSR_STACK_ALIGN (1u << 9)

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

static int prepare(semu_cpu_fixture *fixture, const semu_cpu_state *state,
                   unsigned exception, uint32_t handler)
{
    static const uint8_t program[] = {0x00u, 0xbeu};
    uint32_t vector = 0u;

    if (!semu_cpu_fixture_init(fixture, program, sizeof(program))) return 0;
    if (exception < 16u + ARMV7M_IRQ_COUNT) {
        vector = exception * 4u;
        if (semu_bus_write(fixture->bus, vector, 4u, handler,
                           &fixture->error) != SEMU_OK) {
            semu_cpu_fixture_destroy(fixture);
            return 0;
        }
    }
    semu_cpu_fixture_apply_state(fixture, state);
    return 1;
}

static int read_word(const semu_cpu_fixture *fixture, uint32_t address,
                     uint32_t *value)
{
    semu_error error;
    semu_error_clear(&error);
    return semu_bus_read(fixture->bus, address, 4u, value, &error) == SEMU_OK;
}

static void test_aligned_basic_frame_and_return(semu_test_context *context)
{
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;
    uint32_t value;

    state.r[0] = 0x11223344u;
    state.r[12] = 0xaabbccddu;
    state.r[14] = 0x301u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u, 0x181u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 16u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 0x7e0u, semu_cpu_get_state(fixture.cpu)->msp);
    SEMU_TEST_EQ_U64(context, 0x7e0u, semu_cpu_get_state(fixture.cpu)->r[13]);
    SEMU_TEST_EQ_U64(context, 0xfffffff9u,
                     semu_cpu_get_state(fixture.cpu)->r[14]);
    SEMU_TEST_EQ_U64(context, 0x180u, semu_cpu_get_state(fixture.cpu)->r[15]);
    SEMU_TEST_ASSERT(context, read_word(&fixture, 0x7e0u, &value));
    SEMU_TEST_EQ_U64(context, 0x11223344u, value);
    SEMU_TEST_ASSERT(context, read_word(&fixture, 0x7fcu, &value));
    SEMU_TEST_EQ_U64(context, XPSR_T, value);

    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xfffffff9u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 0x800u, semu_cpu_get_state(fixture.cpu)->msp);
    SEMU_TEST_EQ_U64(context, 0x100u, semu_cpu_get_state(fixture.cpu)->r[15]);
    SEMU_TEST_EQ_U64(context, XPSR_T, semu_cpu_get_state(fixture.cpu)->xpsr);
    SEMU_TEST_EQ_U64(context, 0x11223344u,
                     semu_cpu_get_state(fixture.cpu)->r[0]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_psp_alignment_and_nested_return(semu_test_context *context)
{
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;
    uint32_t value;

    state.control = 2u;
    state.r[13] = 0x804u;
    state.psp = 0x804u;
    state.msp = 0x900u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 11u, 0x181u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 11u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 0u,
                     semu_cpu_get_state(fixture.cpu)->control & 2u);
    SEMU_TEST_EQ_U64(context, 0x7e0u, semu_cpu_get_state(fixture.cpu)->psp);
    SEMU_TEST_EQ_U64(context, 0x900u, semu_cpu_get_state(fixture.cpu)->r[13]);
    SEMU_TEST_EQ_U64(context, 0xfffffffdu,
                     semu_cpu_get_state(fixture.cpu)->r[14]);
    SEMU_TEST_ASSERT(context, read_word(&fixture, 0x7fcu, &value));
    SEMU_TEST_EQ_U64(context, XPSR_T | XPSR_STACK_ALIGN, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xfffffffdu,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 0x804u, semu_cpu_get_state(fixture.cpu)->psp);
    SEMU_TEST_EQ_U64(context, 0x804u, semu_cpu_get_state(fixture.cpu)->r[13]);
    SEMU_TEST_EQ_U64(context, 0u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    SEMU_TEST_EQ_U64(context, XPSR_T,
                     semu_cpu_get_state(fixture.cpu)->xpsr);
    SEMU_TEST_EQ_U64(context, 2u,
                     semu_cpu_get_state(fixture.cpu)->control & 2u);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u, 0x181u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(fixture.bus, 17u * 4u, 4u, 0x191u,
                                    &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 16u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 17u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 0xfffffff1u,
                     semu_cpu_get_state(fixture.cpu)->r[14]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xfffffff1u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 16u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    SEMU_TEST_EQ_U64(context, 0x7e0u, semu_cpu_get_state(fixture.cpu)->msp);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_exception_refusals_are_precise(semu_test_context *context)
{
    semu_cpu_state state = initial_state();
    semu_cpu_fixture fixture;
    semu_status status;
    uint32_t value;

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u, 0x181u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(fixture.bus, 6u * 4u, 4u, 0x1c1u,
                                    &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 16u,
                                            &fixture.error));
    status = armv7m_branch_exchange(fixture.cpu, 0xfffffff5u,
                                    &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 6u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    SEMU_TEST_EQ_U64(context, 0x1c0u,
                     semu_cpu_get_state(fixture.cpu)->r[15]);
    SEMU_TEST_EQ_U64(context, 0xfffffff1u,
                     semu_cpu_get_state(fixture.cpu)->r[14]);
    SEMU_TEST_ASSERT(context, read_word(&fixture, 0x7d4u, &value));
    SEMU_TEST_EQ_U64(context, 0xfffffff5u, value);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    state.msp = 0x1008u;
    state.r[13] = 0x1008u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u, 0x181u));
    status = armv7m_take_exception(fixture.cpu, 16u, &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, status);
    SEMU_TEST_EQ_U64(context, 0x1008u, semu_cpu_get_state(fixture.cpu)->msp);
    SEMU_TEST_EQ_U64(context, 0x100u, semu_cpu_get_state(fixture.cpu)->r[15]);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u, 0x180u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(fixture.bus, 6u * 4u, 4u, 0x1c1u,
                                    &fixture.error));
    status = armv7m_take_exception(fixture.cpu, 16u, &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 16u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    SEMU_TEST_EQ_U64(context, 0x180u,
                     semu_cpu_get_state(fixture.cpu)->r[15]);
    SEMU_TEST_EQ_U64(context, 0u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & XPSR_T);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 6u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    SEMU_TEST_EQ_U64(context, 0x1c0u,
                     semu_cpu_get_state(fixture.cpu)->r[15]);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u, 0x181u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(fixture.bus, 6u * 4u, 4u, 0x1c1u,
                                    &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 16u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(fixture.bus, 0x7f8u, 4u, 0x101u,
                                    &fixture.error));
    status = armv7m_branch_exchange(fixture.cpu, 0xfffffff9u,
                                    &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 6u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    SEMU_TEST_EQ_U64(context, 0x1c0u,
                     semu_cpu_get_state(fixture.cpu)->r[15]);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u, 0x181u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(fixture.bus, 6u * 4u, 4u, 0x1c1u,
                                    &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 16u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(fixture.bus, 0x7fcu, 4u, 0u,
                                    &fixture.error));
    status = armv7m_branch_exchange(fixture.cpu, 0xfffffff9u,
                                    &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 6u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    SEMU_TEST_EQ_U64(context, 0x1c0u,
                     semu_cpu_get_state(fixture.cpu)->r[15]);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state, 16u, 0x181u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xfffffff5u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 0xfffffff4u,
                     semu_cpu_get_state(fixture.cpu)->r[15]);
    SEMU_TEST_EQ_U64(context, XPSR_T,
                     semu_cpu_get_state(fixture.cpu)->xpsr);
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_aligned_basic_frame_and_return),
        SEMU_TEST_CASE(test_psp_alignment_and_nested_return),
        SEMU_TEST_CASE(test_exception_refusals_are_precise)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
