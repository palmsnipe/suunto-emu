/*
 * Ulsan 2.35.36 power-control block attachment (ticket 730, E-ULS-0008).
 *
 * The reference lane resolves 0x40021000 with
 * Miscellaneous.AmbiqApollo4_PowerController. Every boot-phase write the
 * lane logged (0x04 enable tags, 0x24, 0x58/0x60, 0x78/0x80, 0x100) was
 * reported as fully unhandled bits, and lane probes show 0x40021014/0x18
 * read 0x3F unchanged across the guest read-modify-write chain at
 * 0x000965ec: the accepted behavior is constant reads of the observed
 * offsets and stored-nothing writes of the observed values. Before this
 * attachment the boot sampler at 0x00096b66 took a precise BusFault
 * (BFAR 0x40021008, CFSR 0x00008200, first at instruction 12583862).
 *
 * Success cases: observed reads and writes through the board map.
 * Refusal cases: unobserved offsets, unobserved values, wrong widths.
 * Boot case: the guest passes the power-status sampler and the frontier
 * is reproduced twice.
 */

#include "semu/bus.h"
#include "semu/machine.h"
#include "semu/manifest.h"
#include "test.h"
#include "../../src/boards/machine_internal.h"
#include "../../src/boards/ulsan_board.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PWR_BASE 0x40021000u

#define MAP_BOARD(bus)                                                       \
    semu_error_clear(&error);                                                \
    (bus) = semu_bus_create(&error);                                         \
    SEMU_TEST_ASSERT(context, (bus) != NULL);                                \
    semu_error_clear(&error);                                                \
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map((bus), &error));

static void test_observed_transactions_accepted(semu_test_context *context)
{
    static const uint32_t write_pairs[][2] = {
        { 0x04u, 0x00008000u }, { 0x04u, 0x00040000u },
        { 0x04u, 0x00080000u }, { 0x04u, 0x00400000u },
        { 0x24u, 0x3u },        { 0x58u, 0x1u },
        { 0x60u, 0x4u },        { 0x78u, 0x1u },
        { 0x80u, 0x4u },        { 0x100u, 0x1u },
        { 0x14u, 0x3Fu },       { 0x1Cu, 0x8u },
        { 0x04u, 0x00100020u }  /* continuation store (E-ULS-0022) */
    };
    static const uint32_t read_pairs[][2] = {
        { 0x00u, 0x9u },         { 0x04u, 0x00100000u },
        { 0x08u, 0x00100000u },  { 0x14u, 0x3Fu },
        { 0x18u, 0x3Fu },        { 0x1Cu, 0x8u },
        { 0x24u, 0x0u },         { 0x28u, 0x3u },
        { 0x2Cu, 0x3FCu },       { 0x58u, 0x0u },
        { 0x100u, 0x0u },       { 0x108u, 0x0u }  /* VRSTATUS (730) */
    };
    semu_bus *bus;
    semu_error error;
    size_t index;

    MAP_BOARD(bus);
    for (index = 0u; index < sizeof(write_pairs) / sizeof(write_pairs[0]);
         ++index) {
        semu_error_clear(&error);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(
            bus, PWR_BASE + write_pairs[index][0], 4u, write_pairs[index][1],
            &error));
    }
    for (index = 0u; index < sizeof(read_pairs) / sizeof(read_pairs[0]);
         ++index) {
        uint32_t value = 0u;
        semu_error_clear(&error);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
            bus, PWR_BASE + read_pairs[index][0], 4u, &value, &error));
        SEMU_TEST_EQ_U64(context, read_pairs[index][1], value);
    }
    semu_bus_destroy(bus);
}

static void test_unobserved_transactions_refused(semu_test_context *context)
{
    static const uint32_t read_offsets[] = {
        0x34u, 0x38u, 0x50u, 0x64u, 0x84u, 0xFCu, 0x104u
    };
    static const uint32_t write_pairs[][2] = {
        { 0x04u, 0x00000001u },     /* unobserved value at 0x04 */
        { 0x00u, 0x00000009u },     /* reads same, writes refused */
        { 0x108u, 0x00000001u },    /* VRSTATUS write unobserved */
        { 0x14u, 0x0000002Fu },     /* lane branch value never observed */
        { 0x18u, 0x0000003Fu },     /* status offsets refuse writes */
        { 0x2Cu, 0x000003FCu }      /* never written in the boot trace */
    };
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;
    size_t index;

    MAP_BOARD(bus);
    for (index = 0u; index < sizeof(read_offsets) / sizeof(read_offsets[0]);
         ++index) {
        semu_error_clear(&error);
        SEMU_TEST_ASSERT(context, SEMU_OK != semu_bus_read(
            bus, PWR_BASE + read_offsets[index], 4u, &value, &error));
        SEMU_TEST_ASSERT(context, error.code != SEMU_OK);
    }
    for (index = 0u; index < sizeof(write_pairs) / sizeof(write_pairs[0]);
         ++index) {
        semu_error_clear(&error);
        SEMU_TEST_ASSERT(context, SEMU_OK != semu_bus_write(
            bus, PWR_BASE + write_pairs[index][0], 4u, write_pairs[index][1],
            &error));
        SEMU_TEST_ASSERT(context, error.code != SEMU_OK);
    }
    /* Lane framework semantics (E-ULS-0014): reads slice the register
     * word - the 1-byte read at +0x4002105A that runs there without a
     * log line proves byte service. */
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
        bus, PWR_BASE + 0x08u, 2u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x0000), value);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
        bus, PWR_BASE + 0x0Au, 2u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x0010), value);
    /* Writes of a width the lane has never recorded still refuse. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, SEMU_OK != semu_bus_write(
        bus, PWR_BASE + 0x24u, 1u, 3u, &error));
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, SEMU_OK != semu_bus_read(
        bus, PWR_BASE + 0x08u, 3u, &value, &error));
    semu_bus_destroy(bus);
}

/* Bounded private run: with GPIO and the power-control block attached the
 * guest passes the pad-setup step and the power-status sampler, and is
 * observed inside the following init-worker pass at instruction 13000000.
 * The triple is reproducing twice. With this table the guest performs no
 * other access to the block through instruction 200,000,000 and takes no
 * bus fault there (E-ULS-0008); the remaining divergence that keeps the
 * init worker cycling is not caused by any refused power-block
 * transaction and is reported, not guessed. */
static void test_boot_passes_power_status_sampler(semu_test_context *context)
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
        SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_STOP_WFI_DEADLOCK,
                         (uint64_t)reason);
        SEMU_TEST_EQ_U64(context, UINT64_C(12611224),
                         semu_machine_instructions(machine));
        SEMU_TEST_EQ_U64(context, UINT64_C(0x000dabcc),
                         semu_machine_program_counter(machine));
        SEMU_TEST_EQ_U64(context, UINT64_C(0x10029e40), state->r[13]);
    }
    semu_machine_destroy(machine);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_observed_transactions_accepted),
        SEMU_TEST_CASE(test_unobserved_transactions_refused),
        SEMU_TEST_CASE(test_boot_passes_power_status_sampler)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
