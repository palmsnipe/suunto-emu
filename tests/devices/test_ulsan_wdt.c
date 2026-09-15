/*
 * Ulsan watchdog tests (tickets 730, E-ULS-0015/E-ULS-0017).
 *
 * Control writes store minus the untagged bit 1 and reads return the
 * store (lane probe pinned 0x33C3D04 after the 0x33C3D06 write);
 * InterruptEnable answers its logged read/write pair; the reload store
 * is lane-silent. Everything else refuses. The boot case is
 * manifest-gated.
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

static void test_register_store_sequence(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* Boot's pair sequence: the logged 0x33C3D06 store, then the
     * read-back | 1 write; the store keeps neither cycle's bit 1 and
     * answers 0x33C3D04 like the lane probe. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40024000u, 4u,
                                    UINT32_C(0x033C3D06), &error));
    {
        uint32_t value = 0u;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_bus_read(bus, 0x40024000u, 4u, &value,
                                       &error));
        SEMU_TEST_EQ_U64(context, UINT64_C(0x033C3D04), value);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_bus_write(bus, 0x40024000u, 4u,
                                        value | UINT32_C(1), &error));
        value = 0u;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_bus_read(bus, 0x40024000u, 4u, &value,
                                       &error));
        SEMU_TEST_EQ_U64(context, UINT64_C(0x033C3D05), value);
    }
    /* InterruptEnable: logged read (0 before the write), logged write. */
    {
        uint32_t value = 0xdeadbeefu;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_bus_read(bus, 0x40024200u, 4u, &value,
                                       &error));
        SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40024200u, 4u, UINT32_C(1),
                                    &error));
    /* Reload store (lane-silent, no read logged). */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40024004u, 4u, UINT32_C(0xB2),
                                    &error));
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

    /* Reads other than control and InterruptEnable refuse. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, 0x40024004u, 4u, &value, &error) !=
                         SEMU_OK);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, 0x40024008u, 4u, &value, &error) !=
                         SEMU_OK);
    /* Narrow accesses refuse: boot only uses 32-bit. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_write(bus, 0x40024000u, 2u, UINT32_C(0x3D06),
                                    &error) != SEMU_OK);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, 0x40024000u, 1u, &value, &error) !=
                         SEMU_OK);
    semu_bus_destroy(bus);
}

/*
 * Boot frontier: the first boot run now ends in the machine reset applied
 * after the guest's HardFault handler stores SYSRESETREQ: the TIMER0
 * comparator wake releases the WFI at instruction 12,650,485, the RTC
 * counter window answers the sleep-deepening path, which now proceeds
 * fault-free through the enable-window read-modify-write storm at
 * PC 0x00096a30 (E-ULS-0037 stored-word plane) into the guest's
 * own system-reset helper at PC 0x000c399c, and the reset takes
 * effect; a budget of 14,756,458 stops at the first post-reset boot instruction
 * PC 0x001e1b4c, SP 0x1005ffc0 (reproduced twice). Ticket 730 frontier;
 * later instances extend this pin. */
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
        semu_run_limits limits = { UINT64_C(14756458), UINT64_C(4000000000) };
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
        SEMU_TEST_EQ_U64(context, UINT64_C(14756458),
                         semu_machine_instructions(machine));
        SEMU_TEST_EQ_U64(context, UINT64_C(0x001e1b4c),
                         semu_machine_program_counter(machine));
        SEMU_TEST_EQ_U64(context, UINT64_C(0x1005ffc0), state->r[13]);
    }
    semu_machine_destroy(machine);
}

int main(void)
{
    static const semu_test_case cases[] = {
        { "test_register_store_sequence", test_register_store_sequence },
        { "test_unobserved_transactions_refused",
          test_unobserved_transactions_refused },
        { "test_boot_passes_wdt", test_boot_passes_wdt }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
