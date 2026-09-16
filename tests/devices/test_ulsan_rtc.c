/*
 * Ulsan RTC tests (ticket 730, E-ULS-0019, E-ULS-0036).
 *
 * The four registers boot touches store writes and answer reads from the
 * store (lane silent = framework-handled); the sleep-path counter window
 * answers BCD scheduler hundredths at +0x20 and the observed constant at
 * +0x24. All other RTC addresses and widths refuse, and the counter
 * words refuse writes. The boot case is manifest-gated.
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

    semu_ulsan_rtc_attach(scheduler);
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
    /* Neither counter word was ever written in the lane. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_write(bus, 0x40004820u, 4u, UINT32_C(1),
                                    &error) != SEMU_OK);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_write(bus, 0x40004824u, 4u, UINT32_C(1),
                                    &error) != SEMU_OK);
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}

/*
 * Boot frontier (ticket 730, re-pinned by E-ULS-0041): BKPT #0 now
 * retires as a no-op without a debug session (E-ULS-0040 lane probe:
 * Renode 1.16.1 silent continuation plus the lp34b post-assert
 * interrupt census), so both epochs walk past the AM_DEBUG_LOG_ERROR
 * assert at 0x0006bda8 into the scheduler era. Both passes now run to
 * budget: a wake-overflow WFI jump parks virtual time at
 * 262143351559124 ns (about 2^32 timer ticks) past the 4e9 ns cap
 * after 16,667,327 instructions (frontier re-pinned by E-ULS-0046
 * with the read-pure virtual-time STTMR; both passes converge on PC
 * 0x000dabcc, SP 0x10029e40, LR 0x0009760b, XPSR 0x61000000;
 * reproduced twice; dump sha256
 * c5a90475530e4890693ce7bf89871208ef9e2f0c664b4fd379c23130867648b3;
 * old E-ULS-0041 pins PC 0x000b359c/0x000b3598, SP 0x1002a7ec, LR
 * 0x0009c65b measured the read-advance counter). The
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
        SEMU_TEST_EQ_U64(context, UINT64_C(16667327),
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
        { "test_boot_passes_rtc", test_boot_passes_rtc }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
