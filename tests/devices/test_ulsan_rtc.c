/*
 * Ulsan RTC tests (ticket 730, E-ULS-0019).
 *
 * The four registers boot touches store writes and answer reads from the
 * store (lane silent = framework-handled). All other RTC addresses and
 * widths refuse. The boot case is manifest-gated.
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

/*
 * Boot frontier: with the RTC registers answering, the observed boot
 * instruction at 13,000,000 is PC 0x001d0f22, SP 0x1005ffa8
 * (reproducing twice across a reset). The next strict refusal is the
 * SystemTimer comparator word at 0x40008854 (E-ULS-0019 boundary).
 */static void test_boot_passes_rtc(semu_test_context *context)
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
        SEMU_TEST_EQ_U64(context, UINT64_C(0x001d0f22),
                         semu_machine_program_counter(machine));
        SEMU_TEST_EQ_U64(context, UINT64_C(0x1005ffa8), state->r[13]);
    }
    semu_machine_destroy(machine);
}

int main(void)
{
    static const semu_test_case cases[] = {
        { "test_observed_registers_store", test_observed_registers_store },
        { "test_unobserved_rtc_accesses_refused",
          test_unobserved_rtc_accesses_refused },
        { "test_boot_passes_rtc", test_boot_passes_rtc }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
