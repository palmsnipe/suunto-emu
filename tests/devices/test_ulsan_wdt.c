/*
 * Ulsan watchdog write-table tests (ticket 730, E-ULS-0015).
 *
 * The single lane-logged control write is accepted; every read and every
 * other write refuses. The boot case is manifest-gated.
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
#include "../../src/devices/ulsan_wdt.h"

static void test_logged_control_write_accepted(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* The exact lane-logged pair, repeated like boot's delta passes. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40024000u, 4u,
                                    UINT32_C(0x033C3D06), &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40024000u, 4u,
                                    UINT32_C(0x033C3D06), &error));
    semu_bus_destroy(bus);
}

static void test_unobserved_transactions_refused(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* Reads refuse: the lane never logged a WDT read inside boot cycles. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, 0x40024000u, 4u, &value, &error) !=
                         SEMU_OK);
    /* A different control value refuses: only the logged value matches. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_write(bus, 0x40024000u, 4u, UINT32_C(0),
                                    &error) != SEMU_OK);
    /* InterruptEnable (lane line belongs to an unreached phase). */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_write(bus, 0x40024200u, 4u, UINT32_C(1),
                                    &error) != SEMU_OK);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_write(bus, 0x40024000u, 2u, UINT32_C(0x3D06),
                                    &error) != SEMU_OK);
    semu_bus_destroy(bus);
}

/*
 * Boot frontier: with the watchdog control write accepted, the observed
 * boot instruction at 13,000,000 is PC 0x001d0f26, SP 0x1005ffa8
 * (reproducing twice across a reset). The next strict refusal is the
 * UART0 write at 0x40000000 from PC 0x000da834 (E-ULS-0015 boundary).
 */
static void test_boot_passes_wdt(semu_test_context *context)
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
        semu_run_limits limits = { UINT64_C(13000000), UINT64_C(4000000000) };
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
        SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_STOP_BUDGET, (uint64_t)reason);
        SEMU_TEST_EQ_U64(context, UINT64_C(13000000),
                         semu_machine_instructions(machine));
        SEMU_TEST_EQ_U64(context, UINT64_C(0x001d0f26),
                         semu_machine_program_counter(machine));
        SEMU_TEST_EQ_U64(context, UINT64_C(0x1005ffa8), state->r[13]);
    }
    semu_machine_destroy(machine);
}

int main(void)
{
    static const semu_test_case cases[] = {
        { "test_logged_control_write_accepted", test_logged_control_write_accepted },
        { "test_unobserved_transactions_refused",
          test_unobserved_transactions_refused },
        { "test_boot_passes_wdt", test_boot_passes_wdt }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
