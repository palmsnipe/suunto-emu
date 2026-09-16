/*
 * Ulsan RTC tests (ticket 730, E-ULS-0019, E-ULS-0036, E-ULS-0048).
 *
 * The four registers boot touches store writes and answer reads from the
 * store (lane silent = framework-handled); the sleep-path counter window
 * answers BCD scheduler hundredths at +0x20 and the observed store at
 * +0x24; the observed service stores (E-ULS-0048: +0x20=0x100 and
 * +0x24=20230101 from pc 0x0009be70/0x0009beae) are accepted, the +0x20
 * store kept but out of the live-counter read. The observed alarm pair
 * 0x200=1 and 0x208=1 pulses IRQ line 2 for 61,035 ns once a second
 * (repeat scheduled at the occurrence); unobserved alarm values store
 * without arming. All other RTC addresses and widths refuse. The boot
 * case is manifest-gated.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semu/bus.h"
#include "semu/machine.h"
#include "semu/scheduler.h"
#include "semu/manifest.h"
#include "test.h"
#include "../../src/boards/machine_internal.h"
#include "../../src/boards/ulsan_board.h"
#include "../../src/devices/ulsan_rtc.h"

static void test_observed_registers_store(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0xdeadbeefu;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* First reads answer 0 like the trace, stores stick afterwards. */
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
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004800u, 4u, UINT32_C(0),
                                    &error));
    {
        const uint32_t addrs[3] = {0x40004830u, 0x40004A00u, 0x40004A08u};
        size_t index;
        for (index = 0u; index < 3u; ++index) {
            value = 0xdeadbeefu;
            semu_error_clear(&error);
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                             semu_bus_read(bus, addrs[index], 4u, &value,
                                           &error));
            SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
            semu_error_clear(&error);
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                             semu_bus_write(bus, addrs[index], 4u,
                                            UINT32_C(1), &error));
            value = 0u;
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                             semu_bus_read(bus, addrs[index], 4u, &value,
                                           &error));
            SEMU_TEST_EQ_U64(context, UINT64_C(1), value);
        }
    }
    semu_bus_destroy(bus);
}

static void test_unobserved_rtc_accesses_refused(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;
    const uint32_t offsets[] = {0x40004804u, 0x40004808u, 0x4000482Cu,
                                0x40004834u, 0x400049F4u, 0x40004A04u,
                                0x40004A0Cu, 0x40004FFCu};
    size_t index;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

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
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, 0x40004800u, 2u, &value, &error) !=
                         SEMU_OK);
    semu_bus_destroy(bus);
}

/* Counter-window success/refusal cases for E-ULS-0036 (lane lp40). */
static void test_counter_window_reads(semu_test_context *context)
{
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_error error;
    uint32_t value = 0xdeadbeefu;
    uint32_t again = 0u;

    semu_error_clear(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* Device maps detach: before attach the live counter refuses
     * fail-closed while +0x24 already answers its observed constant
     * (lane samples at 0.1 s through 11.0 s all read 0x14700101). */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, 0x40004820u, 4u, &value, &error) !=
                         SEMU_OK);
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004824u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x14700101), value);

    semu_ulsan_rtc_attach(scheduler, NULL, NULL);
    /* BCD hundredths of scheduler virtual time: lane calibration lp40
     * (0.1 s -> 0x10, 0.25 s -> 0x25, 2.5 s -> 0x250, 10.0 s -> 0x1000). */
    value = 0xdeadbeefu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004820u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(250000000),
                                            &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004820u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x25), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(2250000000),
                                            &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004820u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x250), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(7500000000),
                                            &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004820u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x1000), value);
    /* The guest seqlock reads +0x20 twice and retries until stable:
     * consecutive reads at one virtual instant are equal. */
    again = value ^ 1u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004820u, 4u, &again, &error));
    SEMU_TEST_EQ_U64(context, value, again);
    /* E-ULS-0048 supersedes the write refusal: the probe77 v2 service
     * trace (twice x two passes byte-identical) shows the guest stores
     * +0x20=0x100 and +0x24=20230101 at every RTC service and the lane
     * survives those stores every second in its steady era. */
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004820u, 4u, UINT32_C(0x100),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004824u, 4u,
                                    UINT32_C(0x20230101), &error));
    /* The kept +0x20 store does not enter the read: it keeps answering
     * the live scheduler counter (E-ULS-0036 calibration). */
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004820u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x1000), value);
    /* The +0x24 store answers its read. */
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004824u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x20230101), value);
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}

/*
 * E-ULS-0048 alarm line: the observed stores 0x200=1 and 0x208=1 arm
 * one second ahead; each occurrence raises IRQ 2 and drops it 61,035 ns
 * later (lane lp34b/lp50: momentary True->False across the NVIC
 * acknowledge, no RTC MMIO involved), and the next occurrence sits
 * exactly one second after this one (lane "next alarm: t+1 s"). The
 * guest rewrites 0x208=1 inside every service (pc 0x0009bf1e) without
 * shifting that schedule, and unobserved alarm values never arm.
 */
static unsigned g_alarm_irq[24];
static unsigned g_alarm_level[24];
static size_t g_alarm_count;

static void alarm_sink(void *context, unsigned irq, int level)
{
    (void)context;
    if (g_alarm_count < 24u) {
        g_alarm_irq[g_alarm_count] = irq;
        g_alarm_level[g_alarm_count] = level != 0 ? 1u : 0u;
    }
    ++g_alarm_count;
}

static void test_alarm_line_pulse_and_repeat(semu_test_context *context)
{
    semu_scheduler *scheduler;
    semu_bus *bus;
    semu_error error;
    const uint64_t period_ns = UINT64_C(1000000000);
    const uint64_t fall_ns = UINT64_C(61035);

    g_alarm_count = 0u;
    semu_error_clear(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));
    /* Device maps reset detach: arming before attach stores without
     * events (no scheduler). */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A00u, 4u, UINT32_C(1),
                                    &error));
    semu_ulsan_rtc_attach(scheduler, alarm_sink, NULL);
    /* The second half of the observed pair arms one second ahead. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A08u, 4u, UINT32_C(1),
                                    &error));
    /* No edge before the occurrence. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, period_ns - 1u,
                                            &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), g_alarm_count);
    /* The occurrence raises line 2... */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(1), &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(1), g_alarm_count);
    SEMU_TEST_EQ_U64(context, UINT64_C(2), g_alarm_irq[0]);
    SEMU_TEST_EQ_U64(context, UINT64_C(1), g_alarm_level[0]);
    /* ...the lane's acknowledge-side fall drops it one 61,035 ns tick
     * later... */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, fall_ns - 1u,
                                            &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(1), g_alarm_count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(1), &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(2), g_alarm_count);
    SEMU_TEST_EQ_U64(context, UINT64_C(2), g_alarm_irq[1]);
    SEMU_TEST_EQ_U64(context, UINT64_C(0), g_alarm_level[1]);
    /* The service-time rewrite of 0x208=1 (pc 0x0009bf1e) does not
     * reschedule: the lane repeats on its own "next alarm: t+1 s"
     * stamps. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A08u, 4u, UINT32_C(1),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler,
                                            period_ns - fall_ns - 2u,
                                            &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(2), g_alarm_count);
    /* The second occurrence lands exactly one second after the first. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(2), &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(3), g_alarm_count);
    SEMU_TEST_EQ_U64(context, UINT64_C(1), g_alarm_level[2]);
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}

static void test_alarm_unobserved_values_do_not_arm(semu_test_context *context)
{
    semu_scheduler *scheduler;
    semu_bus *bus;
    semu_error error;

    g_alarm_count = 0u;
    semu_error_clear(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));
    semu_ulsan_rtc_attach(scheduler, alarm_sink, NULL);
    /* Only 0x200=1 with 0x208=1 was ever observed on the lane; other
     * alarm values store (reads answer them) without an alarm. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A00u, 4u, UINT32_C(2),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A08u, 4u, UINT32_C(2),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler,
                                            UINT64_C(5000000000), &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), g_alarm_count);
    {
        uint32_t value = 0u;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_bus_read(bus, 0x40004A00u, 4u, &value, &error));
        SEMU_TEST_EQ_U64(context, UINT64_C(2), value);
    }
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}

/*
 * Boot frontier (ticket 730, re-pinned by E-ULS-0041): BKPT #0 now
 * retires as a no-op without a debug session (E-ULS-0040 lane probe:
 * Renode 1.16.1 silent continuation plus the lp34b post-assert
 * interrupt census), so both epochs walk past the AM_DEBUG_LOG_ERROR
 * assert at 0x0006bda8 into the scheduler era. Both passes now run to
 * budget: with the RTC alarm adopted (E-ULS-0048) the guest parks in
 * WFI from one one-second RTC alarm (IRQ 2) to the next and stops at
 * the fourth occurrence, vt 4012595271 ns (anchor 12595271 ns = the
 * boot alarm-pair store) past the 4e9 ns cap, after 16,853,480
 * instructions (frontier re-pinned by E-ULS-0048; both passes converge
 * on PC 0x000dabcc, SP 0x10029e40, LR 0x0009760b, XPSR 0x61000000 -
 * same pins as the E-ULS-0046 wake-overflow park except inst/vt;
 * reproduced twice; dump sha256
 * 56f4b2b8f752ce555af266d9d816948c95a30fcb0dc0d7442cc765e41ddc892e;
 * old E-ULS-0046 pins inst 16667327 vt 262143351559124 - the
 * wake-overflow park about 2^32 timer ticks past the cap on the
 * pre-alarm engine; the old E-ULS-0041 pins PC 0x000b359c/0x000b3598,
 * SP 0x1002a7ec, LR 0x0009c65b measured the read-advance counter). The
 * lane's final 440 ms show 141 IRQ30/IRQ84 pairs, 49 deep-sleep/wake
 * cycles, and two more IRQ18 acknowledgements with no IRQ26/37/45 in
 * this era; matching the lane's steady-era census (panel era, MSPI2,
 * SDIO planes, and the logger values that feed the assert) is the
 * next gap. */
static void test_boot_passes_rtc(semu_test_context *context)
{
    const char *manifest_path = getenv("SEMU_ULSAN_FIRMWARE_MANIFEST");
    FILE *probe;
    semu_firmware_manifest firmware;
    semu_machine_options options;
    semu_machine *machine;
    semu_profile profile;
    semu_error error;
    unsigned pass;

    if (manifest_path == NULL || manifest_path[0] == '\0') {
        manifest_path = "tests/private/ulsan-2.35.36/firmware.semu";
    }
    probe = fopen(manifest_path, "rb");
    if (probe == NULL) {
        printf("SKIP ulsan 2.35.36 private manifest absent: %s\n",
               manifest_path);
        return;
    }
    fclose(probe);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_profile_load(
        "profiles/ulsan/2.35.36/profile.semu", &profile, &error));
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_manifest_load(manifest_path, &firmware, &error));
    memset(&options, 0, sizeof(options));
    options.profile = &profile;
    options.firmware = &firmware;
    semu_error_clear(&error);
    machine = semu_machine_create(&options, &error);
    SEMU_TEST_ASSERT(context, machine != NULL);
    for (pass = 0u; pass < 2u; ++pass) {
        semu_run_limits limits = { UINT64_C(200000000), UINT64_C(4000000000) };
        semu_stop_reason reason;
        const semu_cpu_state *state;
        semu_error_clear(&error);
        if (pass != 0u) {
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                             semu_machine_reset(machine, &error));
        }
        semu_error_clear(&error);
        reason = semu_machine_run(machine, &limits, &error);
        state = semu_cpu_get_state(machine->cpu);
        SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_STOP_BUDGET,
                         (uint64_t)reason);
        SEMU_TEST_EQ_U64(context, UINT64_C(16853480),
                         semu_machine_instructions(machine));
        SEMU_TEST_EQ_U64(context, UINT64_C(0x000dabcc),
                         semu_machine_program_counter(machine));
        SEMU_TEST_EQ_U64(context, UINT64_C(0x10029e40), state->r[13]);
        SEMU_TEST_EQ_U64(context, UINT64_C(0x0009760b), state->r[14]);
        SEMU_TEST_EQ_U64(context, UINT64_C(0x61000000), state->xpsr);
    }
    semu_machine_destroy(machine);
}

int main(void)
{
    static const semu_test_case cases[] = {
        { "test_observed_registers_store", test_observed_registers_store },
        { "test_unobserved_rtc_accesses_refused",
          test_unobserved_rtc_accesses_refused },
        { "test_counter_window_reads", test_counter_window_reads },
        { "test_alarm_line_pulse_and_repeat",
          test_alarm_line_pulse_and_repeat },
        { "test_alarm_unobserved_values_do_not_arm",
          test_alarm_unobserved_values_do_not_arm },
        { "test_boot_passes_rtc", test_boot_passes_rtc }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
