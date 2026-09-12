/*
 * Ulsan SystemTimer tests (ticket 730, E-ULS-0014).
 *
 * LOAD/CTL hold the observed writes (bit-31 mask proven by the lane
 * read-back); CNT advances monotonically per read. The boot case is
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
#include "../../src/devices/ulsan_stimer.h"

static void test_registers_hold_lane_behavior(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0xdeadbeefu;
    uint32_t first;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* Boot's OR-constants accumulate in LOAD, but bit 31 is not stored
     * (the lane read 0x303 back after the 0x80000000 write). */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40008800u, 4u, UINT32_C(0x303),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40008800u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x303), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40008800u, 4u,
                                    UINT32_C(0x80000303), &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40008800u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x303), value);
    /* CTL stores; boot's clear lands on 0. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40008900u, 4u, UINT32_C(0),
                                    &error));
    value = 0xdeadbeefu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40008900u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    /* Comparator words: probe-pinned reads 0; +0x54 also stores. */
    value = 0xdeadbeefu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40008858u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    value = 0xdeadbeefu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x4000885cu, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40008854u, 4u, UINT32_C(0),
                                    &error));
    value = 0xdeadbeefu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40008854u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    /* CNT is monotonic and never repeats while being read. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40008804u, 4u, &first, &error));
    {
        unsigned i;
        for (i = 0u; i < 8u; ++i) {
            uint32_t next = 0u;
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                             semu_bus_read(bus, 0x40008804u, 4u, &next,
                                           &error));
            SEMU_TEST_ASSERT(context, next != first);
            first = next;
        }
    }
    semu_bus_destroy(bus);
}

static void test_unobserved_registers_refused(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;
    const uint32_t offsets[] = {0x40008808u, 0x4000880cu, 0x40008850u,
                                0x40008904u, 0x40008908u, 0x400089f4u};
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
                     semu_bus_read(bus, 0x40008802u, 4u, &value, &error) !=
                         SEMU_OK);
    /* Unobserved comparator writes refuse; only +0x54 has lane traffic. */
    {
        const uint32_t write_only[2] = {0x40008858u, 0x4000885cu};
        size_t j;
        for (j = 0u; j < 2u; ++j) {
            semu_error_clear(&error);
            SEMU_TEST_ASSERT(context,
                             semu_bus_write(bus, write_only[j], 4u,
                                            UINT32_C(1), &error) != SEMU_OK);
        }
    }
    semu_bus_destroy(bus);
}

/*
 * Boot frontier: with the SystemTimer registers and comparator word
 * answering, the observed
 * boot instruction at 13,000,000 is PC 0x001d0f24, SP 0x1005ffa8
 * (reproducing twice across a reset). The next strict refusal is the
 * watchdog control write at 0x40024000 (E-ULS-0014 boundary).
 */
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
        { "test_registers_hold_lane_behavior", test_registers_hold_lane_behavior },
        { "test_unobserved_registers_refused",
          test_unobserved_registers_refused },
        { "test_boot_passes_stimer", test_boot_passes_stimer }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
