#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "test.h"

#define SCS 0xe000e000u
#define SCB_SCR (SCS + 0xd10u)
#define SCR_SEVONPEND (1u << 4)
#define SCR_SLEEPONEXIT (1u << 1)
#define XPSR_T (1u << 24)

static int write_word(semu_cpu_fixture *fixture, uint32_t address,
                      uint32_t value)
{
    return semu_bus_write(fixture->bus, address, 4u, value,
                          &fixture->error) == SEMU_OK;
}

static void signal_callback(void *context, uint64_t now_ns)
{
    (void)now_ns;
    semu_cpu_signal_event((semu_cpu *)context);
}

static int load_irq_handler(semu_cpu_fixture *fixture, uint32_t address)
{
    return semu_cpu_fixture_load_u32(fixture, 16u * 4u, address | 1u) &&
           semu_bus_load(fixture->bus, address,
                         (const uint8_t[]){0x70u, 0x47u}, 2u,
                         &fixture->error) == SEMU_OK;
}

static void test_masked_unmasked_wake(semu_test_context *context)
{
    static const uint8_t program[] = {0x30u, 0xbfu, 0x00u, 0xbeu};
    semu_cpu_fixture fixture;
    semu_cpu_state *state;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program, sizeof(program)));
    SEMU_TEST_ASSERT(context, load_irq_handler(&fixture, 0x180u));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->primask = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x100u, 1u));
    semu_cpu_set_irq(fixture.cpu, 0u, 1);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_ASSERT(context, !state->waiting_for_interrupt);
    SEMU_TEST_EQ_U64(context, 0u, state->xpsr & 0x1ffu);
    state->primask = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 16u, state->xpsr & 0x1ffu);
    semu_cpu_set_irq(fixture.cpu, 0u, 0);
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program, sizeof(program)));
    SEMU_TEST_ASSERT(context, load_irq_handler(&fixture, 0x180u));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x100u, 1u));
    semu_cpu_set_irq(fixture.cpu, 0u, 1);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 16u, state->xpsr & 0x1ffu);
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_now(fixture.scheduler));
    semu_cpu_fixture_destroy(&fixture);
}

static void test_wfe_event_forms_and_scheduled_wake(semu_test_context *context)
{
    static const uint8_t pre_signaled[] = {0x20u, 0xbfu, 0x00u, 0xbeu};
    static const uint8_t one_shot[] = {
        0x20u, 0xbfu, 0x20u, 0xbfu, 0x00u, 0xbeu
    };
    semu_cpu_fixture fixture;
    semu_event_id event_id;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, pre_signaled,
                                           sizeof(pre_signaled)));
    semu_cpu_signal_event(fixture.cpu);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_ASSERT(context,
                     !semu_cpu_get_state(fixture.cpu)->waiting_for_interrupt);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
                     semu_cpu_stop_reason(fixture.cpu));
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, one_shot,
                                           sizeof(one_shot)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_ASSERT(context,
                     semu_cpu_get_state(fixture.cpu)->waiting_for_interrupt);
    semu_cpu_signal_event(fixture.cpu);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_ASSERT(context,
                     semu_cpu_get_state(fixture.cpu)->waiting_for_interrupt);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_WFI_DEADLOCK,
                     semu_cpu_stop_reason(fixture.cpu));
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, pre_signaled,
                                           sizeof(pre_signaled)));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_schedule(fixture.scheduler, 4u,
                                              signal_callback, fixture.cpu,
                                              &event_id, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 4u, semu_scheduler_now(fixture.scheduler));
    SEMU_TEST_ASSERT(context,
                     !semu_cpu_get_state(fixture.cpu)->waiting_for_interrupt);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
                     semu_cpu_stop_reason(fixture.cpu));
    SEMU_TEST_EQ_U64(context, 5u, semu_scheduler_now(fixture.scheduler));
    semu_cpu_fixture_destroy(&fixture);
}

static void test_sevonpend_sleeponexit_and_refusal(semu_test_context *context)
{
    static const uint8_t wfe_program[] = {0x20u, 0xbfu, 0x00u, 0xbeu};
    static const uint8_t thread_program[] = {0x00u, 0xbfu, 0x00u, 0xbeu};
    semu_cpu_fixture fixture;
    semu_cpu_state *state;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, wfe_program,
                                           sizeof(wfe_program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->primask = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCB_SCR, SCR_SEVONPEND));
    semu_cpu_set_irq(fixture.cpu, 0u, 1);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_ASSERT(context, !state->waiting_for_interrupt);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
                     semu_cpu_stop_reason(fixture.cpu));
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, thread_program,
                                           sizeof(thread_program)));
    SEMU_TEST_ASSERT(context, load_irq_handler(&fixture, 0x180u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCB_SCR, SCR_SLEEPONEXIT));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SCS + 0x100u, 1u));
    semu_cpu_set_irq(fixture.cpu, 0u, 1);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    SEMU_TEST_ASSERT(context, state->waiting_for_interrupt);
    SEMU_TEST_EQ_U64(context, 0u, state->xpsr & 0x1ffu);
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_now(fixture.scheduler));
    semu_cpu_set_irq(fixture.cpu, 0u, 0);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_WFI_DEADLOCK,
                     semu_cpu_stop_reason(fixture.cpu));
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, thread_program,
                                           sizeof(thread_program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->control = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_write(fixture.bus, SCB_SCR, 4u,
                                    SCR_SLEEPONEXIT, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus, SCB_SCR, 4u, &value,
                                   &fixture.error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_masked_unmasked_wake),
        SEMU_TEST_CASE(test_wfe_event_forms_and_scheduled_wake),
        SEMU_TEST_CASE(test_sevonpend_sleeponexit_and_refusal)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
