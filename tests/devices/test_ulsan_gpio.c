/*
 * Ulsan 2.35.36 GPIO block attachment (ticket 730, E-ULS-0007).
 *
 * The Ulsan application runs a table of boot steps; one step performs the
 * Ambiq pad-setup sequence through the GPIO block: a PADKEY store of 0x73
 * at 0x40010200 followed by PINCFG stores at 0x40010000 + 4 * pin. Before
 * this ticket the Ulsan board map registered no peripherals, so the PADKEY
 * store took a precise bus fault (CFSR 0x00008200, BFAR 0x40010200, first
 * at instruction 12578512) that the shared fault handler absorbed, the boot
 * step never completed, and the table re-ran forever. The reference lane
 * resolves the same stores through AmbiqApollo4_GPIO at 0x40010000 with
 * bank IRQs 56..63 and never refuses them.
 *
 * Success case: keyed PINCFG path through the board map. Refusal case:
 * PINCFG before PADKEY and an unobserved GPIO offset. Boot case: bounded
 * run reaches the step and continues past the former fault point.
 */

#include "semu/bus.h"
#include "semu/cpu.h"
#include "semu/machine.h"
#include "semu/manifest.h"
#include "test.h"
#include "../../src/boards/machine_internal.h"
#include "../../src/boards/ulsan_board.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GPIO_BASE 0x40010000u
#define PADKEY_OFFSET 0x200u
#define PADKEY_VALUE 0x73u
#define PIN1_PACFG (GPIO_BASE + 0x4u)
#define UNOBSERVED_OFFSET (GPIO_BASE + 0x258u)

static void test_pin_configuration_locked_until_padkey(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));
    /* Fail closed before the pad key: the controller refuses the write. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, SEMU_OK != semu_bus_write(
        bus, PIN1_PACFG, 4u, UINT32_C(0x00000100), &error));
    SEMU_TEST_ASSERT(context, error.code != SEMU_OK);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, SEMU_OK != semu_bus_read(
        bus, UNOBSERVED_OFFSET, 4u, &value, &error));
    SEMU_TEST_ASSERT(context, error.code != SEMU_OK);
    semu_bus_destroy(bus);
}

static void test_padkey_then_pin_configuration_accepted(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(
        bus, GPIO_BASE + PADKEY_OFFSET, 4u, PADKEY_VALUE, &error));
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(
        bus, PIN1_PACFG, 4u, UINT32_C(0x00000100), &error));
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
        bus, PIN1_PACFG, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x00000100), value);
    /* Refusal still holds after unlock for offsets with no Ulsan evidence. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, SEMU_OK != semu_bus_write(
        bus, UNOBSERVED_OFFSET, 4u, UINT32_C(0x00000001), &error));
    SEMU_TEST_ASSERT(context, error.code != SEMU_OK);
    semu_bus_destroy(bus);
}

/*
 * Boot frontier (ticket 730, re-pinned by E-ULS-0041): BKPT #0 now
 * retires as a no-op without a debug session (E-ULS-0040 lane probe:
 * Renode 1.16.1 silent continuation plus the lp34b post-assert
 * interrupt census), so both epochs walk past the AM_DEBUG_LOG_ERROR
 * assert at 0x0006bda8 into the scheduler era. Both passes now run to
 * the instruction budget: 200,000,000 instructions each, PC
 * 0x000b359c (pass zero) / 0x000b3598 (pass one, after the harness
 * reset with retained STIMER NVRAM and MSPI1 planes), SP 0x1002a7ec,
 * LR 0x0009c65b, XPSR 0x61000000 in both (reproduced twice). The
 * lane's final 440 ms show 141 IRQ30/IRQ84 pairs, 49 deep-sleep/wake
 * cycles, and two more IRQ18 acknowledgements with no IRQ26/37/45 in
 * this era; matching the lane's steady-era census (panel era, MSPI2,
 * SDIO planes, and the logger values that feed the assert) is the
 * next gap. */
static void test_boot_passes_pad_setup_step(semu_test_context *context)
{
    const char *manifest_path = getenv("SEMU_ULSAN_FIRMWARE_MANIFEST");
    FILE *probe;
    semu_firmware_manifest firmware;
    semu_machine_options options;
    semu_machine *machine;
    semu_profile profile;
    semu_error error;
    semu_stop_reason reason;
    const semu_cpu_state *state;
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
        semu_error_clear(&error);
        if (pass != 0u) {
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                             semu_machine_reset(machine, &error));
        }
        semu_error_clear(&error);
        reason = semu_machine_run(machine, &limits, &error);
        state = semu_cpu_get_state(machine->cpu);
        SEMU_TEST_ASSERT(context, machine != NULL);
        SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_STOP_BUDGET,
                         (uint64_t)reason);
        SEMU_TEST_EQ_U64(context, UINT64_C(200000000),
                         semu_machine_instructions(machine));
        SEMU_TEST_EQ_U64(context, pass == 0u ? UINT64_C(0x000b359c)
                                              : UINT64_C(0x000b3598),
                         semu_machine_program_counter(machine));
        SEMU_TEST_EQ_U64(context, UINT64_C(0x1002a7ec), state->r[13]);
        SEMU_TEST_EQ_U64(context, UINT64_C(0x0009c65b), state->r[14]);
        SEMU_TEST_EQ_U64(context, UINT64_C(0x61000000), state->xpsr);
    }
    semu_machine_destroy(machine);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_pin_configuration_locked_until_padkey),
        SEMU_TEST_CASE(test_padkey_then_pin_configuration_accepted),
        SEMU_TEST_CASE(test_boot_passes_pad_setup_step)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
