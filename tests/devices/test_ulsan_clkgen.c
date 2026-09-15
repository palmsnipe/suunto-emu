/*
 * Ulsan clock generator dictionary tests (ticket 730, E-ULS-0010).
 *
 * The dictionary behavior (store on write, stored value or 0 on read) is
 * the exact reference-lane script; the observed values (0x00FC0000,
 * 0x00FC0040 at offset 0x44) come from lane probes 8/9. The boot case is
 * manifest-gated like the other Ulsan boot tests.
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
#include "../../src/devices/ulsan_clkgen.h"

#define CLKGEN_ADDR 0x40004000u

static void test_dictionary_stores_and_returns(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0xDEADBEEFu;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* Never-written offsets read 0 (lane dict default). */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, CLKGEN_ADDR + 0x44u, 4u, &value,
                                   &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    /* The observed boot sequence on 0x44: store, read back, store again. */
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, CLKGEN_ADDR + 0x44u, 4u,
                                    UINT32_C(0x00FC0000), &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, CLKGEN_ADDR + 0x44u, 4u, &value,
                                   &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x00FC0000), value);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, CLKGEN_ADDR + 0x44u, 4u,
                                    UINT64_C(0x00FC0040), &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, CLKGEN_ADDR + 0x44u, 4u, &value,
                                   &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x00FC0040), value);
    /* Other offsets keep the dict default until written. */
    value = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, CLKGEN_ADDR + 0x84u, 4u, &value,
                                   &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, CLKGEN_ADDR + 0x84u, 4u,
                                    UINT32_C(0x12345678), &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, CLKGEN_ADDR + 0x84u, 4u, &value,
                                   &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x12345678), value);
    semu_bus_destroy(bus);
}

static void test_invalid_access_widths_refused(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* The lane peripheral serves 32-bit requests; narrower or misaligned
     * accesses refuse rather than invent lane behavior. */
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, CLKGEN_ADDR + 0x44u, 2u, &value,
                                   &error) != SEMU_OK);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_write(bus, CLKGEN_ADDR + 0x44u, 1u,
                                    UINT32_C(0x40), &error) != SEMU_OK);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, CLKGEN_ADDR + 0x46u, 4u, &value,
                                   &error) != SEMU_OK);
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
static void test_boot_passes_hfclk_control(semu_test_context *context)
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
        { "test_dictionary_stores_and_returns",
          test_dictionary_stores_and_returns },
        { "test_invalid_access_widths_refused",
          test_invalid_access_widths_refused },
        { "test_boot_passes_hfclk_control", test_boot_passes_hfclk_control }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
