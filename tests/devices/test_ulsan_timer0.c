/*
 * Ulsan TIMER block tests (ticket 730, E-ULS-0021).
 *
 * The registers boot touches store writes; the four with observed reads
 * (0x10, 0x60, 0x64, 0x220) answer from the store or the observed-zero
 * dictionary (lane silent = framework-handled; E-ULS-0035). Reads of
 * write-only-observed registers, other addresses and widths refuse.
 * Comparator wake edges live in test_ulsan_timer0wake.c. The boot case
 * is manifest-gated.
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
#include "../../src/devices/ulsan_timer0.h"

static void test_observed_registers_store(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0xdeadbeefu;
    const uint32_t read_offsets[3] = {0x40008010u, 0x40008060u, 0x40008220u};
    const uint32_t write_values[3] = {0x2u, 0x4u, 0xA20u};
    size_t index;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* First reads answer 0 like the trace, stores stick afterwards. */
    for (index = 0u; index < 3u; ++index) {
        value = 0xdeadbeefu;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_bus_read(bus, read_offsets[index], 4u, &value,
                                       &error));
        SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
        semu_error_clear(&error);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_bus_write(bus, read_offsets[index], 4u,
                                        write_values[index], &error));
        value = 0u;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_bus_read(bus, read_offsets[index], 4u, &value,
                                       &error));
        SEMU_TEST_EQ_U64(context, write_values[index], value);
    }
    /* Write-only-observed registers accept their stores. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40008068u, 4u, UINT32_C(4),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40008228u, 4u, UINT32_C(0x20),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x4000822cu, 4u, UINT32_C(0),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40008230u, 4u, UINT32_C(0),
                                    &error));
    /* A fresh map clears the shared stores (machine-create semantics). */
    semu_bus_destroy(bus);
    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));
    value = 0xdeadbeefu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40008220u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    semu_bus_destroy(bus);
}

static void test_unobserved_timer_accesses_refused(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;
    const uint32_t offsets[] = {0x40008000u, 0x40008014u, 0x4000805Cu,
                                0x4000806Cu, 0x400080F0u,
                                0x40008224u, 0x40008300u, 0x400087FCu};
    const uint32_t write_only_reads[4] = {0x40008068u, 0x40008228u,
                                          0x4000822cu, 0x40008230u};
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
    /* Reads of write-only-observed registers refuse. */
    for (index = 0u; index < 4u; ++index) {
        semu_error_clear(&error);
        SEMU_TEST_ASSERT(context,
                         semu_bus_read(bus, write_only_reads[index], 4u,
                                       &value, &error) != SEMU_OK);
    }
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, 0x40008010u, 2u, &value, &error) !=
                         SEMU_OK);
    /* E-ULS-0035: the IRQ 68 service routine reads 0x64 (lane answers 0
     * at 1.0 s and 1.003 s, run twice). The lane never faults writes to
     * its registered interrupt registers, so 0x64 is a plain store like
     * its neighbours rather than a write refusal. */
    value = 0xdeadbeefu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40008064u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40008064u, 4u, UINT32_C(0x5A5),
                                    &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40008064u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x5A5), value);
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
static void test_boot_passes_timer0(semu_test_context *context)
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
        { "test_observed_registers_store", test_observed_registers_store },
        { "test_unobserved_timer_accesses_refused",
          test_unobserved_timer_accesses_refused },
        { "test_boot_passes_timer0", test_boot_passes_timer0 }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
