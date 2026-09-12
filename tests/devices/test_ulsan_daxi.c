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
 * Boot frontier: with DAXI and the SilenceRange blocks answering, the
 * observed boot continues past the former panic frontier; PC at
 * instruction 13,000,000 is 0x001d0f24 with SP 0x1005ffa8
 * (reproducing twice across a reset). Ticket 730 frontier; later
 * instances extend this pin. */
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
        SEMU_TEST_EQ_U64(context, UINT64_C(0x001d0f24),
                         semu_machine_program_counter(machine));
        SEMU_TEST_EQ_U64(context, UINT64_C(0x1005ffa8), state->r[13]);
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
