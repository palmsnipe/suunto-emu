/*
 * Ulsan SystemTimer tests (ticket 730, E-ULS-0014/0020/0038 law
 * superseded in part by E-ULS-0045/E-ULS-0046).
 *
 * CONFIG is a plain store, reset 0x80000000 (lane lp48); STTMR is
 * virtual-time derived at the CLKSEL rate (3 = 32000 Hz, lp47:
 * +0xA0 per 5 ms), reads equal at equal virtual time (the guest's
 * PRIMASK-scoped triple-read at 0xc2bc8 must see the stable branch),
 * and holds while gated. The +0x50..+0x5c NVRAM plane stores, reads
 * back, and survives the machine reset (lane Apollo4RetainedSystem-
 * Timer). Without a scheduler attach the virtual-time registers
 * refuse. The boot case is manifest-gated.
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
#include "../../src/devices/ulsan_stimer.h"

#define STIMER_CONFIG 0x40008800u
#define STIMER_COUNT  0x40008804u
#define STIMER_CTL    0x40008900u

static void test_registers_hold_lane_behavior(semu_test_context *context)
{
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_error error;
    uint32_t value = 0xdeadbeefu;
    uint32_t triple[3];
    uint32_t last;
    unsigned i;
    unsigned ticks_seen = 0u;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));
    semu_ulsan_stimer_attach(scheduler);

    /* CONFIG resets to FREEZE with CLKSEL=NOCLK (lp48 at vt=0) and the
     * gated counter answers 0. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, STIMER_CONFIG, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x80000000), value);
    value = 0xdeadbeefu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, STIMER_COUNT, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);

    /* Boot sequence as static RE pins it (0xc2b52/0xc2b60 via 0x9bf38):
     * store 0x80000000, then RMW (old & 0x7FFF00F0) | 0x303. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, STIMER_CONFIG, 4u,
                                    UINT32_C(0x80000000), &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, STIMER_CONFIG, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x80000000), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, STIMER_CONFIG, 4u,
                                    UINT32_C(0x303), &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, STIMER_CONFIG, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x303), value);

    /* CLKSEL=3 enables 32000 Hz: three consecutive reads at the same
     * virtual time are equal (the getter's stable branch), then exact
     * lp47 rates - one tick per 31250 ns and +0xA0 per 5 ms. */
    for (i = 0u; i < 3u; ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_bus_read(bus, STIMER_COUNT, 4u, &triple[i],
                                       &error));
    }
    SEMU_TEST_EQ_U64(context, triple[0], triple[1]);
    SEMU_TEST_EQ_U64(context, triple[1], triple[2]);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(31250),
                                            &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, STIMER_COUNT, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(1), value);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(5000000),
                                            &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, STIMER_COUNT, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(161), value);
    /* The lane models XTAL_32KHZ as exactly 32000, not silicon 32768:
     * one whole second of counting lands on 32000 ticks. */
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler,
                                            UINT64_C(994968750), &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, STIMER_COUNT, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(32000), value);

    /* FREEZE holds the counter across further time (upstream Enabled
     * gate; no lane evidence of clear-on-regate), and re-enable
     * resumes from the held value. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, STIMER_CONFIG, 4u,
                                    UINT32_C(0x80000000), &error));
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(1000000),
                                            &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, STIMER_COUNT, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(32000), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, STIMER_CONFIG, 4u,
                                    UINT32_C(0x303), &error));
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(62500),
                                            &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, STIMER_COUNT, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(32002), value);

    /* Freeze regression (found by the E-ULS-0046 in-era probe): the
     * wake loop calls the counter getter every ~170 ns of virtual time.
     * A fold that re-anchored on every read truncated each sub-tick
     * remainder and pinned STTMR forever (the probe froze at 184).
     * Upstream derives Value from a fixed origin, so dense monotone
     * reads must keep crossing ticks: 400 reads spaced 100 ns apart
     * span 40000 ns = exactly one tick plus an 8750 ns remainder. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, STIMER_CONFIG, 4u,
                                    UINT32_C(0x80000000), &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, STIMER_CONFIG, 4u,
                                    UINT32_C(0x303), &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, STIMER_COUNT, 4u, &value, &error));
    last = value;
    for (i = 0u; i < 400u; ++i) {
        uint32_t seen = 0u;
        semu_error_clear(&error);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_scheduler_advance(scheduler, UINT64_C(100),
                                                &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_bus_read(bus, STIMER_COUNT, 4u, &seen,
                                       &error));
        SEMU_TEST_ASSERT(context, seen >= last);
        SEMU_TEST_ASSERT(context, seen - last <= 1u);
        ticks_seen += (unsigned)(seen - last);
        last = seen;
    }
    SEMU_TEST_EQ_U64(context, UINT64_C(1), ticks_seen);

    /* CTL stores; boot's clear lands on 0. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, STIMER_CTL, 4u, UINT32_C(7),
                                    &error));
    value = 0xdeadbeefu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, STIMER_CTL, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(7), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, STIMER_CTL, 4u, UINT32_C(0),
                                    &error));

    /* NVRAM plane (E-ULS-0038): stores read back, survive the reset;
     * CONFIG resets to FREEZE and the counter restarts gated at 0. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40008850u, 4u,
                                    UINT32_C(0x13579bdf), &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40008858u, 4u,
                                    UINT32_C(0x2468ace0), &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x4000885cu, 4u,
                                    UINT32_C(0x3579bdf1), &error));
    semu_bus_reset(bus);
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, STIMER_CONFIG, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x80000000), value);
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40008850u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x13579bdf), value);
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40008858u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x2468ace0), value);
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x4000885cu, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x3579bdf1), value);
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, STIMER_CTL, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    value = 0xdeadbeefu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, STIMER_COUNT, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    semu_scheduler_destroy(scheduler);
    semu_bus_destroy(bus);
}

static void test_unobserved_registers_refused(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;
    const uint32_t offsets[] = {0x40008808u, 0x4000880cu,
                                0x40008904u, 0x40008908u, 0x400089f4u};
    size_t index;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));
    /* The map detaches the scheduler: the virtual-time registers
     * (CONFIG write, STTMR read) refuse instead of guessing a clock. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_write(bus, STIMER_CONFIG, 4u, UINT32_C(0x303),
                                    &error) != SEMU_OK);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, STIMER_COUNT, 4u, &value, &error) !=
                         SEMU_OK);

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
                     semu_bus_read(bus, 0x40008802u, 4u, &value, &error) !=
                         SEMU_OK);
    /* STTMR is read-only: the lane drops guest writes there and the
     * era never sends one (E-ULS-0045 census); the engine refuses. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_write(bus, STIMER_COUNT, 4u, UINT32_C(1),
                                    &error) != SEMU_OK);
    /* The NVRAM plane is doubleword-only like the lane wrapper: sub-word
     * and unaligned accesses refuse. */
    {
        const uint32_t narrow[2] = {0x40008852u, 0x40008851u};
        size_t j;
        for (j = 0u; j < 2u; ++j) {
            semu_error_clear(&error);
            SEMU_TEST_ASSERT(context,
                             semu_bus_write(bus, narrow[j], 2u,
                                            UINT32_C(1), &error) != SEMU_OK);
            semu_error_clear(&error);
            SEMU_TEST_ASSERT(context,
                             semu_bus_read(bus, narrow[j], 2u, &value,
                                           &error) != SEMU_OK);
        }
    }
    semu_bus_destroy(bus);
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
 * instructions; PC 0x000dabcc, SP 0x10029e40, LR 0x0009760b,
 * XPSR 0x61000000, identical in both passes (reproduced twice; dump
 * sha256
 * 56f4b2b8f752ce555af266d9d816948c95a30fcb0dc0d7442cc765e41ddc892e).
 * The old E-ULS-0046 pins (inst 16667327, vt 262143351559124 - the
 * wake-overflow park about 2^32 timer ticks past the cap) measured the
 * pre-alarm engine; the old pins (PC 0x000b359c/0x000b3598,
 * SP 0x1002a7ec, LR
 * 0x0009c65b) were the E-ULS-0041 frontier of the read-advance
 * counter; the interim 200M frontier (PC 0x0009c3ae at 203844874 ns,
 * dump sha256
 * ae279d6361c523e68d4c9e6f665e223ee352d18e7920ebe3b3d16f1991bc9a26)
 * measured the first fold, which froze dense getter reads. The
 * lane's final 440 ms show 141 IRQ30/IRQ84 pairs, 49 deep-sleep/wake
 * cycles, and two more IRQ18 acknowledgements with no IRQ26/37/45 in
 * this era; matching the lane's steady-era census (panel era, MSPI2,
 * SDIO planes, and the logger values that feed the assert) is the
 * next gap. */
static void test_boot_passes_stimer(semu_test_context *context)
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
        { "test_registers_hold_lane_behavior", test_registers_hold_lane_behavior },
        { "test_unobserved_registers_refused",
          test_unobserved_registers_refused },
        { "test_boot_passes_stimer", test_boot_passes_stimer }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
