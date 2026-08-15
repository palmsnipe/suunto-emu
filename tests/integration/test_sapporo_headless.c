#include "test.h"

#include <stdint.h>
#include <string.h>

#include "sapporo_devices.h"
#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/cpu.h"
#include "semu/scheduler.h"

/*
 * Synthetic Sapporo startup guest.  Same Apollo4 core sequence as the
 * apollo4 guest: read CHIPREV, write/read CAL, write INTR, WFI.  This
 * proves the full Sapporo wiring (SoC init, device attach, I2C mux,
 * UART bridge, MSPI refusal) does not interfere with deterministic
 * startup.  The guest is hand-encoded Thumb-2, not authentic firmware.
 */
#define GUEST_SIZE 256u
#define GUEST_SRAM_BASE 0x10000000u
#define GUEST_VECTOR_TABLE GUEST_SRAM_BASE
#define INSTRUCTION_LIMIT UINT64_C(1000000)
#define TIME_LIMIT UINT64_C(500000000)

static const uint8_t guest_image[GUEST_SIZE] = {
    [0x00] = 0x00, [0x01] = 0x10, [0x02] = 0x00, [0x03] = 0x10,
    [0x04] = 0x09, [0x05] = 0x00, [0x06] = 0x00, [0x07] = 0x10,
    [0x08] = 0x40, [0x09] = 0xF2, [0x0A] = 0x0C, [0x0B] = 0x00,
    [0x0C] = 0xC4, [0x0D] = 0xF2, [0x0E] = 0x02, [0x0F] = 0x00,
    [0x10] = 0x01, [0x11] = 0x68,
    [0x12] = 0x44, [0x13] = 0xF2, [0x14] = 0x44, [0x15] = 0x00,
    [0x16] = 0xC4, [0x17] = 0xF2, [0x18] = 0x00, [0x19] = 0x00,
    [0x1A] = 0x40, [0x1B] = 0xF2, [0x1C] = 0x00, [0x1D] = 0x02,
    [0x1E] = 0xC0, [0x1F] = 0xF2, [0x20] = 0xF8, [0x21] = 0x02,
    [0x22] = 0x02, [0x23] = 0x60,
    [0x24] = 0x03, [0x25] = 0x68,
    [0x26] = 0x00, [0x27] = 0x24,
    [0x28] = 0x44, [0x29] = 0xF2, [0x2A] = 0x0C, [0x2B] = 0x00,
    [0x2C] = 0xC4, [0x2D] = 0xF2, [0x2E] = 0x00, [0x2F] = 0x00,
    [0x30] = 0x04, [0x31] = 0x60,
    [0x32] = 0x30, [0x33] = 0xBF,
};

typedef struct sapporo_fixture {
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4 *soc;
    semu_sapporo_devices *devices;
    semu_cpu *cpu;
    semu_error error;
} sapporo_fixture;

static void irq_sink(void *context, unsigned irq, int level)
{
    sapporo_fixture *fixture = (sapporo_fixture *)context;
    if (fixture != NULL && fixture->cpu != NULL) {
        semu_cpu_set_irq(fixture->cpu, irq, level);
    }
}

static int fixture_init(sapporo_fixture *fixture)
{
    semu_error *error = &fixture->error;
    memset(fixture, 0, sizeof(*fixture));

    fixture->bus = semu_bus_create(error);
    if (fixture->bus == NULL) return 0;
    if (semu_bus_map_ram(fixture->bus, "sapporo.mram", 0x00000000u,
                         0x00200000u, error) != SEMU_OK) return 0;
    if (semu_bus_map_ram(fixture->bus, "sapporo.sram", 0x10000000u,
                         0x00180000u, error) != SEMU_OK) return 0;
    if (semu_bus_map_ram(fixture->bus, "sapporo.external-flash", 0x14000000u,
                         0x02000000u, error) != SEMU_OK) return 0;

    fixture->scheduler = semu_scheduler_create(error);
    if (fixture->scheduler == NULL) return 0;

    fixture->soc = semu_apollo4_create(fixture->bus, error);
    if (fixture->soc == NULL) return 0;
    if (semu_apollo4_init(fixture->soc, fixture->scheduler, irq_sink,
                           fixture, error) != SEMU_OK) return 0;

    fixture->devices = semu_sapporo_devices_create(fixture->scheduler, error);
    if (fixture->devices == NULL) return 0;

    if (semu_sapporo_devices_attach(fixture->devices, fixture->soc,
                                     error) != SEMU_OK) return 0;

    fixture->cpu = semu_cpu_create(fixture->bus, fixture->scheduler, error);
    if (fixture->cpu == NULL) return 0;

    if (semu_bus_load(fixture->bus, GUEST_SRAM_BASE, guest_image,
                      GUEST_SIZE, error) != SEMU_OK) return 0;

    semu_cpu_reset(fixture->cpu, GUEST_VECTOR_TABLE, error);
    if (error->code != SEMU_OK) return 0;
    return 1;
}

static void fixture_destroy(sapporo_fixture *fixture)
{
    if (fixture == NULL) return;
    semu_cpu_destroy(fixture->cpu);
    semu_sapporo_devices_destroy(fixture->devices);
    semu_apollo4_destroy(fixture->soc);
    semu_scheduler_destroy(fixture->scheduler);
    semu_bus_destroy(fixture->bus);
}

static semu_status run_guest(sapporo_fixture *fixture,
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

static void test_sapporo_startup_complete(semu_test_context *context)
{
    sapporo_fixture fixture;
    semu_status status;

    if (!fixture_init(&fixture)) {
        semu_test_fail(context, __FILE__, __LINE__, "fixture_init failed");
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

static void test_sapporo_deterministic_repeat(semu_test_context *context)
{
    sapporo_fixture fixture;
    uint64_t first_instructions;
    uint64_t first_time;
    semu_status status;

    if (!fixture_init(&fixture)) {
        semu_test_fail(context, __FILE__, __LINE__, "fixture_init failed");
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
        semu_test_fail(context, __FILE__, __LINE__, "fixture_init failed");
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

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_sapporo_startup_complete),
        SEMU_TEST_CASE(test_sapporo_deterministic_repeat)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
