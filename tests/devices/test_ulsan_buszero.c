/*
 * Ulsan lane-recorded bus-zero window tests (ticket 730, E-ULS-0013).
 *
 * The served CRYPTO offset returns the 0 the lane sysbus log records for
 * the 2.35.36 boot read at 0x400c0fe0; every other access in the declared
 * tag range refuses. The boot case is manifest-gated.
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
#include "../../src/devices/ulsan_buszero.h"

static void test_logged_offset_reads_zero(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0xdeadbeefu;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x400c0fe0u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    /* Guest logger port: probe-pinned lane read value 0 and the
     * lane-discarded logged write 0x2 (E-ULS-0016). */
    value = 0xdeadbeefu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40000000u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40000000u, 4u, UINT32_C(2),
                                    &error));
    semu_bus_destroy(bus);
}

static void test_unobserved_offsets_refused(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;
    const uint32_t offsets[] = {0x400c0000u, 0x400c0fdcu, 0x400c0fe4u,
                                0x400c2000u, 0x400c3ffcu};
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
    }
    /* No write to the CRYPTO range is observed in the lane. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_write(bus, 0x400c0fe0u, 4u, UINT32_C(1),
                                    &error) != SEMU_OK);
    /* Logger port: only the logged 0x2 write is accepted. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_write(bus, 0x40000000u, 4u, UINT32_C(3),
                                    &error) != SEMU_OK);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, 0x400c0fe0u, 2u, &value, &error) !=
                         SEMU_OK);
    semu_bus_destroy(bus);
}

/*
 * Boot frontier: with the bus-zero CRYPTO word answering, the observed
 * boot instruction at 13,000,000 is PC 0x001d0f22, SP 0x1005ffa8
 * (reproducing twice across a reset). The strict refusal chain moved to
 * the power-control zero write (added with this gap) and then to the
 * TIMER block at 0x40008800 (E-ULS-0013 boundary).
 */
static void test_boot_passes_crypto_read(semu_test_context *context)
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
        { "test_logged_offset_reads_zero", test_logged_offset_reads_zero },
        { "test_unobserved_offsets_refused",
          test_unobserved_offsets_refused },
        { "test_boot_passes_crypto_read", test_boot_passes_crypto_read }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
