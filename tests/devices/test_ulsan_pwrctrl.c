/*
 * Ulsan 2.35.36 power-control block attachment (ticket 730, E-ULS-0008).
 *
 * The reference lane resolves 0x40021000 with
 * Miscellaneous.AmbiqApollo4_PowerController. The boot-phase writes at
 * 0x24, 0x58/0x60, 0x78/0x80 and 0x100 were reported with tag-only
 * (discarded) bits, and lane probes show 0x40021014/0x18 read 0x3F
 * unchanged across the guest read-modify-write chain at 0x000965ec: the
 * accepted behavior there is constant reads of the observed offsets and
 * stored-nothing writes of the observed values. +0x04/+0x08 (E-ULS-0037)
 * follow the upstream field storage: the enable word starts at
 * 0x00100000, every write replaces it with value & 0x00103FFE, and the
 * status register mirrors it. Before the first attachment the boot
 * sampler at 0x00096b66 took a precise BusFault (BFAR 0x40021008,
 * CFSR 0x00008200, first at instruction 12583862).
 *
 * Success cases: observed reads and writes through the board map; the
 * +0x04 stored-word plane with its +0x08 mirror.
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
    /* Reads first: +0x04/+0x08 are stored-word backed, so their expected
     * values are the reset-state values asserted by a fresh map. */
    for (index = 0u; index < sizeof(read_pairs) / sizeof(read_pairs[0]);
         ++index) {
        uint32_t value = 0u;
        semu_error_clear(&error);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
            bus, PWR_BASE + read_pairs[index][0], 4u, &value, &error));
        SEMU_TEST_EQ_U64(context, read_pairs[index][1], value);
    }
    for (index = 0u; index < sizeof(write_pairs) / sizeof(write_pairs[0]);
         ++index) {
        semu_error_clear(&error);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(
            bus, PWR_BASE + write_pairs[index][0], 4u, write_pairs[index][1],
            &error));
    }
    semu_bus_destroy(bus);
}

static void test_unobserved_transactions_refused(semu_test_context *context)
{
    static const uint32_t read_offsets[] = {
        0x34u, 0x38u, 0x50u, 0x64u, 0x84u, 0xFCu, 0x104u
    };
    static const uint32_t write_pairs[][2] = {
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

/*
 * E-ULS-0037 stored-word plane. The guest RMW helper (PC 0x000969de/
 * 0x00096a30) reads +0x04, ORs a domain bit and stores the result back;
 * the lane's whole-value warnings (0x8000 etc.) and the lane probe
 * samples (0x00100000 at 0.13 s, then 0 to 11.0 s) prove the upstream
 * field storage reproduced below: word := value & 0x00103FFE, tag-only
 * writes leave it unchanged, and +0x08 mirrors the folded flags.
 */
static void test_device_power_store_plane(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;

    MAP_BOARD(bus);
    /* Reset state matches the boot-era observations. */
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
        bus, PWR_BASE + 0x04u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x00100000), value);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
        bus, PWR_BASE + 0x08u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x00100000), value);

    /* IOM4 enable read-modify-write store logged in-tree (bit 5 with the
     * reset read-back): handled bits stay, status folds group 5-8 to
     * 0x1E0 and copies the CRYPTO flag. */
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(
        bus, PWR_BASE + 0x04u, 4u, 0x00100020u, &error));
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
        bus, PWR_BASE + 0x04u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x00100020), value);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
        bus, PWR_BASE + 0x08u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x001001E0), value);
    /* Byte lanes slice the live word. */
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
        bus, PWR_BASE + 0x06u, 1u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x00000010), value);

    /* The guest's zero store clears the word: the mechanism behind the
     * lane reading 0 at every +0x04 RMW (whole-value warning 0x8000). */
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(
        bus, PWR_BASE + 0x04u, 4u, 0x00000000u, &error));
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
        bus, PWR_BASE + 0x04u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x00000000), value);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
        bus, PWR_BASE + 0x08u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x00000000), value);

    /* The logged whole values (bit 15, 18, 19, 22) are tag-only: each is
     * accepted and leaves the word at 0, matching the 0 samples after
     * 0.23 s. */
    {
        static const uint32_t tag_only[] = {
            0x00008000u, 0x00400000u, 0x00040000u, 0x00080000u
        };
        size_t index;
        for (index = 0u; index < sizeof(tag_only) / sizeof(tag_only[0]);
             ++index) {
            semu_error_clear(&error);
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(
                bus, PWR_BASE + 0x04u, 4u, tag_only[index], &error));
            semu_error_clear(&error);
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
                bus, PWR_BASE + 0x04u, 4u, &value, &error));
            SEMU_TEST_EQ_U64(context, UINT64_C(0x00000000), value);
        }
    }

    /* Handled-bit mask edge: bit 0 (tag-only) and bits 25-31 (reserved)
     * drop; the full-word write keeps exactly the field bits and the
     * status mirror folds all groups. */
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(
        bus, PWR_BASE + 0x04u, 4u, 0x8000001Fu, &error));
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
        bus, PWR_BASE + 0x04u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x0000001E), value);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(
        bus, PWR_BASE + 0x04u, 4u, 0xFFFFFFFFu, &error));
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
        bus, PWR_BASE + 0x04u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x00103FFE), value);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
        bus, PWR_BASE + 0x08u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x00103FFE), value);

    /* +0x08 writes hit FieldMode.Read and tag bits only: accepted with
     * no state change (the enable word keeps the previous value). */
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(
        bus, PWR_BASE + 0x08u, 4u, 0x12345678u, &error));
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
        bus, PWR_BASE + 0x04u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x00103FFE), value);

    /* Refusals: widths the lane never recorded, and the constant-plane
     * offsets around the live pair keep refusing unobserved accesses. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, SEMU_OK != semu_bus_write(
        bus, PWR_BASE + 0x04u, 2u, 0x20u, &error));
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, SEMU_OK != semu_bus_write(
        bus, PWR_BASE + 0x05u, 1u, 0x10u, &error));
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, SEMU_OK != semu_bus_read(
        bus, PWR_BASE + 0x04u, 3u, &value, &error));

    /* Machine-level bus reset re-applies the register reset value. */
    semu_bus_reset(bus);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
        bus, PWR_BASE + 0x04u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x00100000), value);
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
        SEMU_TEST_CASE(test_observed_transactions_accepted),
        SEMU_TEST_CASE(test_unobserved_transactions_refused),
        SEMU_TEST_CASE(test_device_power_store_plane),
        SEMU_TEST_CASE(test_boot_passes_power_status_sampler)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
