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

static int write_byte(semu_cpu_fixture *fixture, uint32_t address,
                      uint32_t value)
{
    return semu_bus_write(fixture->bus, address, 1u, value,
                          &fixture->error) == SEMU_OK;
}

static void test_nvic_lifecycle_and_scs(semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();
    uint32_t value;

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd00u, &value));
    SEMU_TEST_EQ_U64(context, 0x410fc241u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_write(fixture.bus, SCS + 0xd00u, 4u, 0u,
                                    &fixture.error));
    SEMU_TEST_ASSERT(context, write_word(&fixture, 0x40u, 0x181u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x100u, 1u));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0x100u, &value));
    SEMU_TEST_EQ_U64(context, 1u, value);
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x180u, 1u));
    semu_cpu_set_irq(fixture.cpu, 0u, 1);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0x200u, &value));
    SEMU_TEST_EQ_U64(context, 1u, value);
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x100u, 1u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 16u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0x300u, &value));
    SEMU_TEST_EQ_U64(context, 1u, value);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd04u, &value));
    SEMU_TEST_ASSERT(context, (value & 0x1ffu) == 16u);
    semu_cpu_set_irq(fixture.cpu, 0u, 0);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xfffffff9u,
                                            &fixture.error));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0x300u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);

    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x200u, 1u));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0x200u, &value));
    SEMU_TEST_EQ_U64(context, 1u, value);
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x280u, 1u));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0x200u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);

    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x400u,
                                         0x00000080u));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0x400u, &value));
    SEMU_TEST_EQ_U64(context, 0x80u, value);
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x400u,
                                         0x000000ffu));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0x400u, &value));
    SEMU_TEST_EQ_U64(context, 0xe0u, value);
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd08u, 0x223u));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd08u, &value));
    SEMU_TEST_EQ_U64(context, 0x200u, value);
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd0cu,
                                         0x12340300u));
    SEMU_TEST_ASSERT(context, !semu_cpu_reset_requested(fixture.cpu));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd0cu,
                                         0x05fa0304u));
    SEMU_TEST_ASSERT(context, semu_cpu_reset_requested(fixture.cpu));
    semu_cpu_reset(fixture.cpu, 0u, &fixture.error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, fixture.error.code);
    SEMU_TEST_ASSERT(context, !semu_cpu_reset_requested(fixture.cpu));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd0cu,
                                         0x05fa0300u));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd0cu, &value));
    SEMU_TEST_EQ_U64(context, 0xfa050300u, value);
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd18u,
                                         0x00030201u));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd18u, &value));
    SEMU_TEST_EQ_U64(context, 0x00000000u, value);
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd88u,
                                         0x00f00000u));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd88u, &value));
    SEMU_TEST_EQ_U64(context, 0x00f00000u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus, SCS + 0x0fcu, 4u, &value,
                                   &fixture.error));
    semu_cpu_fixture_destroy(&fixture);
}

static void test_system_priority_mask_and_refusal(semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();
    uint32_t value;
    uint32_t before;

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd20u,
                                         0xffffffffu));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd20u, &value));
    SEMU_TEST_EQ_U64(context, 0xe0e00000u, value);
    before = value;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_write(fixture.bus, SCS + 0xd21u, 2u,
                                    0u, &fixture.error));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd20u, &value));
    SEMU_TEST_EQ_U64(context, before, value);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_irq_line_requires_nvic_enable(semu_test_context *context)
{
    static const uint8_t program[] = {
        0x00u, 0xbfu,             /* nop */
        0x00u, 0xbeu              /* bkpt */
    };
    semu_cpu_fixture fixture;
    uint32_t value;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program,
                                            sizeof(program)));
    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_load_u32(&fixture, 16u * 4u,
                                                0x181u));
    SEMU_TEST_ASSERT(context,
                     semu_bus_load(fixture.bus, 0x180u,
                                   (const uint8_t[]){0x70u, 0x47u}, 2u,
                                   &fixture.error) == SEMU_OK);

    semu_cpu_set_irq(fixture.cpu, 0u, 1);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0x100u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0x200u, &value));
    SEMU_TEST_EQ_U64(context, 1u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    SEMU_TEST_EQ_U64(context, 0x102u,
                     semu_cpu_get_state(fixture.cpu)->r[15]);

    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x100u, 1u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 16u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    semu_cpu_set_irq(fixture.cpu, 0u, 0);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_priority_pending_and_pendsv(semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_ASSERT(context, write_word(&fixture, 0x40u, 0x181u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, 0x44u, 0x1a1u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x100u, 3u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x400u,
                                         0x00002080u));
    semu_cpu_set_irq(fixture.cpu, 0u, 1);
    semu_cpu_set_irq(fixture.cpu, 1u, 1);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 17u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    semu_cpu_set_irq(fixture.cpu, 0u, 0);
    semu_cpu_set_irq(fixture.cpu, 1u, 0);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xfffffff9u,
                                            &fixture.error));

    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x400u,
                                         0x00004040u));
    semu_cpu_set_irq(fixture.cpu, 0u, 1);
    semu_cpu_set_irq(fixture.cpu, 1u, 1);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 16u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    semu_cpu_set_irq(fixture.cpu, 0u, 0);
    semu_cpu_set_irq(fixture.cpu, 1u, 0);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xfffffff9u,
                                            &fixture.error));

    SEMU_TEST_ASSERT(context, write_word(&fixture, 0x238u, 0x1c1u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd04u,
                                         1u << 28));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 14u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xfffffff9u,
                                            &fixture.error));
    semu_cpu_fixture_destroy(&fixture);
}

static void test_prigroup_masks_and_nested_return(semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();
    uint32_t value;

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_ASSERT(context, write_word(&fixture, 16u * 4u, 0x181u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, 17u * 4u, 0x1a1u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd0cu,
                                         0x05fa0400u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x400u,
                                         0x00002120u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x100u, 1u));
    semu_cpu_set_irq(fixture.cpu, 0u, 1);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 16u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);

    semu_cpu_set_irq(fixture.cpu, 0u, 0);
    semu_cpu_set_irq(fixture.cpu, 1u, 1);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 16u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    semu_cpu_set_irq(fixture.cpu, 1u, 0);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xfffffff9u,
                                            &fixture.error));

    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x400u,
                                         0x00002040u));
    semu_cpu_set_irq(fixture.cpu, 0u, 1);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    semu_cpu_set_irq(fixture.cpu, 0u, 0);
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x100u, 2u));
    semu_cpu_set_irq(fixture.cpu, 1u, 1);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 17u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    semu_cpu_set_irq(fixture.cpu, 1u, 0);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0x300u, &value));
    SEMU_TEST_EQ_U64(context, 3u, value);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd04u, &value));
    SEMU_TEST_ASSERT(context, (value & (1u << 11)) == 0u);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xfffffff1u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 16u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    SEMU_TEST_EQ_U64(context, 1u, fixture.cpu->irq_active[0u]);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0x300u, &value));
    SEMU_TEST_EQ_U64(context, 1u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xfffffff9u,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 0u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->irq_active[0u]);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0x300u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_nmi_mask_bypass_and_reserved_refusal(semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();
    uint32_t value = 0u;
    semu_cpu_state before;

    state.primask = 1u;
    state.basepri = 0xffu;
    state.faultmask = 1u;
    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_ASSERT(context, write_word(&fixture, 2u * 4u, 0x1a1u));
    SEMU_TEST_ASSERT(context, write_byte(&fixture, SCS + 0xd22u, 0x77u));
    SEMU_TEST_ASSERT(context, write_byte(&fixture, SCS + 0xd23u, 0x88u));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd20u, &value));
    SEMU_TEST_EQ_U64(context, 0x80600000u, value);
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd04u,
                                         1u << 31));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SCS + 0xd04u, &value));
    SEMU_TEST_EQ_U64(context, 2u, (value >> 12) & 0x1ffu);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 2u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);

    before = *semu_cpu_get_state(fixture.cpu);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus, SCS + 0x120u, 4u, &value,
                                   &fixture.error));
    SEMU_TEST_ASSERT(context,
                     memcmp(&before, semu_cpu_get_state(fixture.cpu),
                            sizeof(before)) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus, SCS + 0x4f0u, 1u, &value,
                                   &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     armv7m_branch_exchange(fixture.cpu, 0xfffffff1u,
                                            &fixture.error));
    semu_cpu_fixture_destroy(&fixture);
}

static void test_pending_source_count_tracks_edges(semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_cpu_state state = initial_state();

    SEMU_TEST_ASSERT(context, prepare(&fixture, &state));
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->pending_source_count);
    SEMU_TEST_EQ_U64(context, (uint64_t)-1,
                     (uint64_t)armv7m_pending_exception(fixture.cpu));

    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x100u, 1u));
    semu_cpu_set_irq(fixture.cpu, 0u, 1);
    SEMU_TEST_EQ_U64(context, 1u, fixture.cpu->pending_source_count);
    SEMU_TEST_EQ_U64(context, 16u,
                     (uint64_t)armv7m_pending_exception(fixture.cpu));
    semu_cpu_set_irq(fixture.cpu, 0u, 0);
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->pending_source_count);
    SEMU_TEST_EQ_U64(context, (uint64_t)-1,
                     (uint64_t)armv7m_pending_exception(fixture.cpu));

    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd04u,
                                         1u << 28));
    SEMU_TEST_EQ_U64(context, 1u, fixture.cpu->pending_source_count);
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0xd04u,
                                         1u << 27));
    SEMU_TEST_EQ_U64(context, 0u, fixture.cpu->pending_source_count);
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_nvic_lifecycle_and_scs),
        SEMU_TEST_CASE(test_irq_line_requires_nvic_enable),
        SEMU_TEST_CASE(test_priority_pending_and_pendsv),
        SEMU_TEST_CASE(test_prigroup_masks_and_nested_return),
        SEMU_TEST_CASE(test_nmi_mask_bypass_and_reserved_refusal),
        SEMU_TEST_CASE(test_pending_source_count_tracks_edges),
        SEMU_TEST_CASE(test_system_priority_mask_and_refusal)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
