#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "../../src/cpu/armv7m/armv7m_internal.h"
#include "test.h"

#include <string.h>

#define SCS 0xe000e000u
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

static int prepare(semu_cpu_fixture *fixture, const semu_cpu_state *state)
{
    static const uint8_t program[] = {0x00u, 0xbeu};

    if (!semu_cpu_fixture_init(fixture, program, sizeof(program))) return 0;
    semu_cpu_fixture_apply_state(fixture, state);
    return 1;
}

static int write_word(semu_cpu_fixture *fixture, uint32_t address,
                      uint32_t value)
{
    return semu_bus_write(fixture->bus, address, 4u, value,
                          &fixture->error) == SEMU_OK;
}

static int read_word(semu_cpu_fixture *fixture, uint32_t address,
                     uint32_t *value)
{
    return semu_bus_read(fixture->bus, address, 4u, value,
                         &fixture->error) == SEMU_OK;
}

static void test_enabled_faults_and_w1c(semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();
    uint32_t value;

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_ASSERT(context, write_word(&fixture, 20u, 0x1a1u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd24u,
                                         1u << 17));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_request_fault(fixture.cpu, 5u, 0x8200u, 0x456u,
                                          1, &fixture.error));
    SEMU_TEST_EQ_U64(context, 5u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd28u, &value));
    SEMU_TEST_EQ_U64(context, 0x8200u, value);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd38u, &value));
    SEMU_TEST_EQ_U64(context, 0x456u, value);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd24u, &value));
    SEMU_TEST_ASSERT(context, (value & (1u << 1)) != 0u);
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd24u, 0u));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd24u, &value));
    SEMU_TEST_ASSERT(context, (value & (1u << 1)) == 0u);
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd28u, 0x8200u));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd28u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd38u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_disabled_escalation_and_refusals(semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();
    semu_cpu_state before;
    uint32_t value;

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_ASSERT(context, write_word(&fixture, 12u, 0x181u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_request_fault(fixture.cpu, 5u, 0x8200u, 0xabcdu,
                                          1, &fixture.error));
    SEMU_TEST_EQ_U64(context, 3u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd2cu, &value));
    SEMU_TEST_ASSERT(context, (value & (1u << 30)) != 0u);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd28u, &value));
    SEMU_TEST_EQ_U64(context, 0x8200u, value);
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    before = *semu_cpu_get_state(fixture.cpu);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus, SCS + 0xfcu, 4u, &value,
                                   &fixture.error));
    SEMU_TEST_ASSERT(context,
                     memcmp(&before, semu_cpu_get_state(fixture.cpu),
                            sizeof(before)) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
                     semu_bus_read(fixture.bus, SCS + 0xfcu, 3u, &value,
                                   &fixture.error));
    semu_cpu_fixture_destroy(&fixture);

    state = initial_state();
    state.control = 1u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    before = *semu_cpu_get_state(fixture.cpu);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus, SCS + 0xd04u, 4u, &value,
                                   &fixture.error));
    SEMU_TEST_ASSERT(context,
                     memcmp(&before, semu_cpu_get_state(fixture.cpu),
                            sizeof(before)) == 0);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_enabled_usage_fault_and_pending_clear(semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();
    uint32_t value;

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_ASSERT(context, write_word(&fixture, 6u * 4u, 0x1c1u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd24u,
                                         1u << 18));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_request_fault(fixture.cpu, 6u, 1u << 16, 0u,
                                          0, &fixture.error));
    SEMU_TEST_EQ_U64(context, 6u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd28u, &value));
    SEMU_TEST_EQ_U64(context, 1u << 16, value);

    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd24u,
                                         1u << 15));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd24u, &value));
    SEMU_TEST_ASSERT(context, (value & (1u << 15)) != 0u);
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd24u, 0u));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd24u, &value));
    SEMU_TEST_ASSERT(context, (value & (1u << 15)) == 0u);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_fault_in_hardfault_enters_lockup(semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_ASSERT(context, write_word(&fixture, 3u * 4u, 0x181u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_take_exception(fixture.cpu, 3u,
                                           &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
                     armv7m_request_fault(fixture.cpu, 5u, 0x8200u, 0xabcdu,
                                          1, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_FIRMWARE_ASSERT,
                     semu_cpu_stop_reason(fixture.cpu));
    SEMU_TEST_ASSERT(context, semu_cpu_get_state(fixture.cpu)->halted != 0);
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_enabled_faults_and_w1c),
        SEMU_TEST_CASE(test_disabled_escalation_and_refusals),
        SEMU_TEST_CASE(test_enabled_usage_fault_and_pending_clear),
        SEMU_TEST_CASE(test_fault_in_hardfault_enters_lockup)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
