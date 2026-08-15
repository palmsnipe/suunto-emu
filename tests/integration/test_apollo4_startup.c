#include "test.h"

#include <stdint.h>
#include <string.h>

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/cpu.h"
#include "semu/hash.h"
#include "semu/scheduler.h"
#include "../../fixtures/synthetic/apollo4/guest_image.h"

#define INSTRUCTION_LIMIT UINT64_C(250000)
#define TIME_LIMIT UINT64_C(100000000)

typedef struct apollo4_fixture {
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4 *soc;
    semu_cpu *cpu;
    semu_error error;
} apollo4_fixture;

static void irq_sink(void *context, unsigned irq, int level)
{
    apollo4_fixture *fixture = (apollo4_fixture *)context;
    if (fixture != NULL && fixture->cpu != NULL) {
        semu_cpu_set_irq(fixture->cpu, irq, level);
    }
}

static int fixture_init(apollo4_fixture *fixture)
{
    semu_error *error = &fixture->error;
    memset(fixture, 0, sizeof(*fixture));

    fixture->bus = semu_bus_create(error);
    if (fixture->bus == NULL) {
        return 0;
    }
    if (semu_bus_map_ram(fixture->bus, "apollo4.sram",
                         SEMU_APOLLO4_GUEST_SRAM_BASE,
                         SEMU_APOLLO4_GUEST_SRAM_SIZE, error) != SEMU_OK) {
        return 0;
    }
    fixture->scheduler = semu_scheduler_create(error);
    if (fixture->scheduler == NULL) {
        return 0;
    }
    fixture->soc = semu_apollo4_create(fixture->bus, error);
    if (fixture->soc == NULL) {
        return 0;
    }
    if (semu_apollo4_init(fixture->soc, fixture->scheduler, irq_sink, fixture,
                           error) != SEMU_OK) {
        return 0;
    }
    fixture->cpu = semu_cpu_create(fixture->bus, fixture->scheduler, error);
    if (fixture->cpu == NULL) {
        return 0;
    }
    if (semu_bus_load(fixture->bus, SEMU_APOLLO4_GUEST_SRAM_BASE,
                      semu_apollo4_guest_image,
                      SEMU_APOLLO4_GUEST_IMAGE_SIZE, error) != SEMU_OK) {
        return 0;
    }
    semu_cpu_reset(fixture->cpu, SEMU_APOLLO4_GUEST_VECTOR_TABLE, error);
    if (error->code != SEMU_OK) {
        return 0;
    }
    return 1;
}

static void fixture_destroy(apollo4_fixture *fixture)
{
    if (fixture == NULL) {
        return;
    }
    semu_cpu_destroy(fixture->cpu);
    semu_apollo4_destroy(fixture->soc);
    semu_scheduler_destroy(fixture->scheduler);
    semu_bus_destroy(fixture->bus);
}

static semu_status run_guest(apollo4_fixture *fixture,
                              uint64_t instruction_limit,
                              uint64_t time_limit)
{
    while (!semu_cpu_get_state(fixture->cpu)->halted) {
        const semu_cpu_state *state = semu_cpu_get_state(fixture->cpu);
        semu_status status;

        if (state->instructions >= instruction_limit ||
            semu_scheduler_now(fixture->scheduler) >= time_limit) {
            return SEMU_ERR_STATE;
        }
        status = semu_cpu_step(fixture->cpu, &fixture->error);
        if (status != SEMU_OK) {
            return status;
        }
    }
    return SEMU_OK;
}

static void test_stable_wfi(semu_test_context *context)
{
    apollo4_fixture fixture;
    semu_status status;

    if (!fixture_init(&fixture)) {
        semu_test_fail(context, __FILE__, __LINE__,
                       "fixture_init failed");
        return;
    }

    status = run_guest(&fixture, INSTRUCTION_LIMIT, TIME_LIMIT);
    SEMU_TEST_ASSERT(context, status == SEMU_OK);

    SEMU_TEST_EQ_U64(context, SEMU_STOP_WFI_DEADLOCK,
                     semu_cpu_stop_reason(fixture.cpu));

    {
        const semu_cpu_state *state = semu_cpu_get_state(fixture.cpu);
        SEMU_TEST_ASSERT(context, state->instructions < INSTRUCTION_LIMIT);
        SEMU_TEST_ASSERT(context,
                        semu_scheduler_now(fixture.scheduler) < TIME_LIMIT);
    }

    fixture_destroy(&fixture);
}

static void test_deterministic_repeat(semu_test_context *context)
{
    apollo4_fixture fixture;
    uint64_t first_instructions;
    uint64_t first_time;
    semu_status status;

    if (!fixture_init(&fixture)) {
        semu_test_fail(context, __FILE__, __LINE__,
                       "fixture_init failed");
        return;
    }

    status = run_guest(&fixture, INSTRUCTION_LIMIT, TIME_LIMIT);
    SEMU_TEST_ASSERT(context, status == SEMU_OK);
    SEMU_TEST_EQ_U64(context, SEMU_STOP_WFI_DEADLOCK,
                     semu_cpu_stop_reason(fixture.cpu));

    first_instructions = semu_cpu_get_state(fixture.cpu)->instructions;
    first_time = semu_scheduler_now(fixture.scheduler);

    fixture_destroy(&fixture);

    if (!fixture_init(&fixture)) {
        semu_test_fail(context, __FILE__, __LINE__,
                       "fixture_init failed (second run)");
        return;
    }

    status = run_guest(&fixture, INSTRUCTION_LIMIT, TIME_LIMIT);
    SEMU_TEST_ASSERT(context, status == SEMU_OK);
    SEMU_TEST_EQ_U64(context, SEMU_STOP_WFI_DEADLOCK,
                     semu_cpu_stop_reason(fixture.cpu));

    SEMU_TEST_EQ_U64(context, first_instructions,
                     semu_cpu_get_state(fixture.cpu)->instructions);
    SEMU_TEST_EQ_U64(context, first_time,
                     semu_scheduler_now(fixture.scheduler));

    fixture_destroy(&fixture);
}

static void test_unknown_access_refuses(semu_test_context *context)
{
    apollo4_fixture fixture;
    uint32_t value;

    if (!fixture_init(&fixture)) {
        semu_test_fail(context, __FILE__, __LINE__,
                       "fixture_init failed");
        return;
    }

    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus, 0x40004084u, 4u, &value,
                                   &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus, 0x40004000u, 4u, &value,
                                   &fixture.error));

    fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_stable_wfi),
        SEMU_TEST_CASE(test_deterministic_repeat),
        SEMU_TEST_CASE(test_unknown_access_refuses)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
