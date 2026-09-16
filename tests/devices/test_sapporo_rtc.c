/*
 * Sapporo 2.35.34 live RTC tests (ticket 710 instance-3, E-SAP-0032,
 * law E-ULS-0048/E-ULS-0036).
 *
 * Only the selected sapporo-2.35.34 profile dispatches the auxiliary
 * RTC window to this module; other profiles keep the inert stub (first
 * case). The live block answers the E-SAP-0032 census set: framework
 * stores at +0x00/+0x30/+0x200/+0x208, the BCD-hundredths scheduler
 * counter at +0x20, constant 0 at +0x24, everything else refused
 * (including +0x20/+0x24 writes, never observed on Sapporo). The
 * observed pair 0x200=1 and 0x208=1 arms the one-second alarm pulsing
 * IRQ line 2 for 61,035 ns with the repeat scheduled at the occurrence;
 * other values store without arming. Reset cancels the pending alarm.
 * The boot case replays the recorded post-wake stop and is
 * manifest-gated (E-SAP-0033).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/machine.h"
#include "semu/manifest.h"
#include "semu/scheduler.h"
#include "test.h"
#include "../../src/boards/machine_internal.h"
#include "../../src/devices/sapporo_rtc.h"

static unsigned g_alarm_count;
static unsigned g_alarm_irq[24];
static unsigned g_alarm_level[24];

static void alarm_sink(void *context, unsigned irq, int level)
{
    (void)context;
    if (g_alarm_count < 24u) {
        g_alarm_irq[g_alarm_count] = irq;
        g_alarm_level[g_alarm_count] = level != 0 ? 1u : 0u;
    }
    ++g_alarm_count;
}

/* Live-mode bus: scheduler + SoC + explicit attach (maps reset detach). */
static semu_bus *live_bus(semu_scheduler **scheduler, semu_error *error)
{
    semu_apollo4 *soc;
    semu_bus *bus;

    *scheduler = semu_scheduler_create(error);
    if (*scheduler == NULL) {
        return NULL;
    }
    bus = semu_bus_create(error);
    if (bus == NULL) {
        return NULL;
    }
    soc = semu_apollo4_create(bus, error);
    if (soc == NULL || semu_apollo4_init(soc, *scheduler, NULL, NULL,
                                         error) != SEMU_OK ||
        semu_apollo4_select_profile(soc, "sapporo-2.35.34",
                                    error) != SEMU_OK) {
        return NULL;
    }
    semu_sapporo_rtc_attach(*scheduler, alarm_sink, NULL);
    return bus;
}

static void test_stub_profiles_stay_inert(semu_test_context *context)
{
    semu_scheduler *scheduler;
    semu_bus *bus;
    semu_apollo4 *soc;
    semu_error error;
    uint32_t value = 0xdeadbeefu;

    g_alarm_count = 0u;
    semu_error_clear(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    soc = semu_apollo4_create(bus, &error);
    SEMU_TEST_ASSERT(context, soc != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_init(soc, scheduler, NULL, NULL,
                                       &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_select_profile(
        soc, "sapporo-2.33.16", &error));
    SEMU_TEST_ASSERT(context, semu_apollo4_select_profile(NULL, NULL,
                                                          &error) != SEMU_OK);
    semu_sapporo_rtc_attach(scheduler, alarm_sink, NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A00u, 4u, UINT32_C(1),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A08u, 4u, UINT32_C(1),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(2100000000),
                                            &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), g_alarm_count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004820u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value); /* stub answers 0 */
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}

static void test_stores_and_counter(semu_test_context *context)
{
    semu_scheduler *scheduler;
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0xdeadbeefu;

    g_alarm_count = 0u;
    semu_error_clear(&error);
    bus = live_bus(&scheduler, &error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004800u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004800u, 4u, UINT32_C(0xE),
                                    &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004800u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0xE), value);
    /* +0x20 answers BCD hundredths of scheduler virtual time. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004820u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(1000000000),
                                            &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004820u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x100), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004824u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value); /* +0x24 answers 0 */
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}

static void test_unobserved_accesses_refuse(semu_test_context *context)
{
    semu_scheduler *scheduler;
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;
    const uint32_t offsets[] = {0x40004804u, 0x40004808u, 0x4000481Cu,
                                0x40004834u, 0x400049FCu, 0x40004A04u,
                                0x40004A0Cu, 0x40004FFCu};
    size_t index;

    g_alarm_count = 0u;
    semu_error_clear(&error);
    bus = live_bus(&scheduler, &error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    for (index = 0u; index < sizeof(offsets) / sizeof(offsets[0]); ++index) {
        semu_error_clear(&error);
        SEMU_TEST_ASSERT(context,
                         semu_bus_read(bus, offsets[index], 4u, &value,
                                       &error) != SEMU_OK);
        semu_error_clear(&error);
        SEMU_TEST_ASSERT(context,
                         semu_bus_write(bus, offsets[index], 4u,
                                        UINT32_C(1), &error) != SEMU_OK);
    }
    /* Unobserved widths, alignment, and the never-observed +0x20/+0x24
     * stores (the census pair never writes the counter window). */
    SEMU_TEST_ASSERT(context, semu_bus_write(bus, 0x40004820u, 4u,
                                             UINT32_C(0x100),
                                             &error) != SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(bus, 0x40004824u, 4u,
                                             UINT32_C(20230101),
                                             &error) != SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_read(bus, 0x40004800u, 2u, &value,
                                            &error) != SEMU_OK);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, semu_bus_write(bus, 0x40004802u, 2u,
                                             UINT32_C(1), &error) != SEMU_OK);
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}

static void test_alarm_pulse_and_repeat(semu_test_context *context)
{
    semu_scheduler *scheduler;
    semu_bus *bus;
    semu_error error;
    const uint64_t period_ns = UINT64_C(1000000000);
    const uint64_t fall_ns = UINT64_C(61035);

    g_alarm_count = 0u;
    semu_error_clear(&error);
    bus = live_bus(&scheduler, &error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A08u, 4u, UINT32_C(1),
                                    &error));
    /* The pair's second half arms one second ahead. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A00u, 4u, UINT32_C(1),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, period_ns - 1u,
                                            &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), g_alarm_count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(1), &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(1), g_alarm_count);
    SEMU_TEST_EQ_U64(context, UINT64_C(2), g_alarm_irq[0]);
    SEMU_TEST_EQ_U64(context, UINT64_C(1), g_alarm_level[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, fall_ns - 1u,
                                            &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(1), g_alarm_count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(1), &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(2), g_alarm_count);
    SEMU_TEST_EQ_U64(context, UINT64_C(2), g_alarm_irq[1]);
    SEMU_TEST_EQ_U64(context, UINT64_C(0), g_alarm_level[1]);
    /* Rewriting the pair while pending does not reschedule. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A08u, 4u, UINT32_C(1),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler,
                                            period_ns - fall_ns - 2u,
                                            &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(2), g_alarm_count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(2), &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(3), g_alarm_count);
    SEMU_TEST_EQ_U64(context, UINT64_C(1), g_alarm_level[2]);
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}

static void test_unobserved_values_do_not_arm(semu_test_context *context)
{
    semu_scheduler *scheduler;
    semu_bus *bus;
    semu_error error;

    g_alarm_count = 0u;
    semu_error_clear(&error);
    bus = live_bus(&scheduler, &error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A00u, 4u, UINT32_C(1),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A08u, 4u, UINT32_C(2),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(2100000000),
                                            &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), g_alarm_count);
    /* Either store order completes the pair and arms. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A00u, 4u, UINT32_C(2),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A08u, 4u, UINT32_C(1),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(2100000000),
                                            &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), g_alarm_count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A00u, 4u, UINT32_C(1),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(1000000001),
                                            &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(1), g_alarm_count);
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}

static void test_reset_cancels_pending_alarm(semu_test_context *context)
{
    semu_scheduler *scheduler;
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0xdeadbeefu;

    g_alarm_count = 0u;
    semu_error_clear(&error);
    bus = live_bus(&scheduler, &error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A08u, 4u, UINT32_C(1),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A00u, 4u, UINT32_C(1),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler,
                                            UINT64_C(999000000), &error));
    semu_bus_reset(bus);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler,
                                            UINT64_C(1500000000), &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), g_alarm_count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004A00u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value); /* stores cleared */
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}

static void test_boot_recorded_stop(semu_test_context *context)
{
    const char *manifest_path = getenv("SEMU_SAPPORO_235_FIRMWARE_MANIFEST");
    FILE *probe;
    semu_firmware_manifest firmware;
    semu_machine_options options;
    semu_machine *machine;
    semu_profile profile;
    semu_error error;
    unsigned pass;

    if (manifest_path == NULL || manifest_path[0] == '\0') {
        manifest_path = "tests/private/sapporo-2.35.34.18929/firmware.semu";
    }
    probe = fopen(manifest_path, "rb");
    if (probe == NULL) {
        printf("SKIP sapporo 2.35.34 private manifest absent: %s\n",
               manifest_path);
        return;
    }
    fclose(probe);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_profile_load(
        "profiles/sapporo/2.35.34/profile.semu", &profile, &error));
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_manifest_load(manifest_path, &firmware, &error));
    memset(&options, 0, sizeof(options));
    options.profile = &profile;
    options.firmware = &firmware;
    semu_error_clear(&error);
    machine = semu_machine_create(&options, &error);
    SEMU_TEST_ASSERT(context, machine != NULL);
    {
        semu_run_limits limits = { UINT64_C(100000000),
                                   UINT64_C(30000000000) };
        semu_stop_reason reason;
        semu_error_clear(&error);
        reason = semu_machine_run(machine, &limits, &error);
        /* E-SAP-0033: the armed one-second alarm wakes the startup
         * park; the boot then issues its footer-validation AIRCR reset
         * (lane-recorded native boundary, cf. the 2.35 resc reset
         * macro) and the second boot is under way at the budget.
         * Byte-identical to the two CLI bounded runs. The pass-2 reset
         * replay is exercised below but not pinned: a soft machine
         * reset does not detach the module seams (the ulsan_rtc
         * convention), so the warm pass legitimately differs. */
        SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_STOP_BUDGET,
                         (uint64_t)reason);
        SEMU_TEST_EQ_U64(context, UINT64_C(100000000),
                         semu_machine_instructions(machine));
        SEMU_TEST_EQ_U64(context, UINT64_C(0x000ccb1a),
                         semu_machine_program_counter(machine));
        SEMU_TEST_EQ_U64(context, UINT64_C(1033322780),
                         semu_machine_virtual_time(machine));
    }
    for (pass = 0u; pass < 2u; ++pass) {
        semu_run_limits limits = { UINT64_C(100000000),
                                   UINT64_C(30000000000) };
        semu_stop_reason reason;
        semu_error_clear(&error);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_machine_reset(machine, &error));
        semu_error_clear(&error);
        reason = semu_machine_run(machine, &limits, &error);
        SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_STOP_BUDGET,
                         (uint64_t)reason);
        SEMU_TEST_EQ_U64(context, UINT64_C(100000000),
                         semu_machine_instructions(machine));
    }
    semu_machine_destroy(machine);
}

int main(void)
{
    static const semu_test_case cases[] = {
        { "test_stub_profiles_stay_inert", test_stub_profiles_stay_inert },
        { "test_stores_and_counter", test_stores_and_counter },
        { "test_unobserved_accesses_refuse",
          test_unobserved_accesses_refuse },
        { "test_alarm_pulse_and_repeat", test_alarm_pulse_and_repeat },
        { "test_unobserved_values_do_not_arm",
          test_unobserved_values_do_not_arm },
        { "test_reset_cancels_pending_alarm",
          test_reset_cancels_pending_alarm },
        { "test_boot_recorded_stop", test_boot_recorded_stop }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
