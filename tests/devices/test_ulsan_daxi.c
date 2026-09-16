/*
 * Ulsan cpu-complex DAXI and SilenceRange tests (ticket 730, E-ULS-0011).
 *
 * Values come from the lane repl script (0x4 at +0x54, 0 elsewhere across
 * 0x48000000) and from the probe-confirmed SilenceRange zero reads;
 * writes are discarded there. The boot case is manifest-gated.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semu/bus.h"
#include "semu/machine.h"
#include "semu/manifest.h"
#include "test.h"
#include "../../src/boards/machine_internal.h"
#include "../../src/boards/ulsan_board.h"
#include "../../src/devices/ulsan_daxi.h"

static void test_script_values_served(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* request.Value = 0x4 if Offset == 0x54 else 0, whole 0x1000 block. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x48000054u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x4), value);
    value = 0xdeadbeefu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x48000050u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x48000FFCu, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    /* The boot's store to 0x54 is accepted and discarded: the script
     * never stores, so 0x54 keeps answering 0x4 afterwards. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x48000054u, 4u, UINT32_C(0),
                                    &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x48000054u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x4), value);
    /* SilenceRange declarations: SYNC_READ and MCUCTRL read 0 (probe
     * confirmed on the lane) and ignore writes. */
    {
        const uint32_t silence_addrs[3] = {0x40020000u, 0x47FF0000u,
                                           0x47FF0004u};
        size_t index;
        for (index = 0u; index < 3u; ++index) {
            value = 0xdeadbeefu;
            semu_error_clear(&error);
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                             semu_bus_read(bus, silence_addrs[index], 4u,
                                           &value, &error));
            SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
            semu_error_clear(&error);
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                             semu_bus_write(bus, silence_addrs[index], 4u,
                                            UINT32_C(0xA5A5), &error));
        }
    }
    semu_bus_destroy(bus);
}

static void test_narrow_or_misaligned_refused(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* The lane peripherals see 32-bit requests only. */
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, 0x48000054u, 2u, &value, &error) !=
                         SEMU_OK);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_write(bus, 0x48000054u, 1u, UINT32_C(4),
                                    &error) != SEMU_OK);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, 0x47FF0002u, 4u, &value, &error) !=
                         SEMU_OK);
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
static void test_boot_passes_daxi_probe(semu_test_context *context)
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
        { "test_script_values_served", test_script_values_served },
        { "test_narrow_or_misaligned_refused",
          test_narrow_or_misaligned_refused },
        { "test_boot_passes_daxi_probe", test_boot_passes_daxi_probe }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
