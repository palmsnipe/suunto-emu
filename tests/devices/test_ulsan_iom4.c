/*
 * Ulsan IOM4 tests (ticket 730, E-ULS-0023).
 *
 * The boot-touched registers reproduce the lane register-collection
 * semantics: masked stores answer reads, +0x11C carries its read-only
 * submodule-type enum bits, +0x280 keeps the 0x00200000 reset constant,
 * and fully tagged registers discard writes and read 0. The lane probe
 * of the live device proves the observed bytes. All other IOM4
 * addresses and widths refuse. The boot case is manifest-gated.
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
#include "../../src/devices/ulsan_iom4.h"

#define IOM4 0x40054000u

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

    /* The lane reports 0xE20 at +0x11C (read-only SMOD1TYPE=1 and
     * SMOD2TYPE=7 bits); stores keep bit 0/bit 4 only. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, IOM4 + 0x11Cu, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x00000E20), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, IOM4 + 0x11Cu, 4u, UINT32_C(0x1),
                                    &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, IOM4 + 0x11Cu, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x00000E21), value);
    /* Enabling both SPI and I2C clears both (lane write callback). */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, IOM4 + 0x11Cu, 4u, UINT32_C(0xFF),
                                    &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, IOM4 + 0x11Cu, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x00000E20), value);

    /* Boot write values store exactly as the lane register masks say. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, IOM4 + 0x104u, 4u,
                                    UINT32_C(0x00001010), &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, IOM4 + 0x104u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x00001010), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, IOM4 + 0x118u, 4u,
                                    UINT32_C(0xFFFFFFFF), &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, IOM4 + 0x118u, 4u, &value, &error));
    /* Only reserved bits [1,8) and [13,16) drop out; DIV3 stores. */
    SEMU_TEST_EQ_U64(context, UINT32_C(0xFFFF1F01), value);

    /* The configuration write 0x0103F270 leaves the lane at 0xF270. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, IOM4 + 0x2C0u, 4u,
                                    UINT32_C(0x0103F270), &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, IOM4 + 0x2C0u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x0000F270), value);

    /* +0x210 keeps only bits [1:0]; +0x200 keeps 15 bits. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, IOM4 + 0x210u, 4u,
                                    UINT32_C(0x00000002), &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, IOM4 + 0x210u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x00000002), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, IOM4 + 0x200u, 4u, UINT32_C(0x8000),
                                    &error));
    value = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, IOM4 + 0x200u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0), value);

    /* Write-discard registers (tagged-only or W1C over zero status):
 * writes discard, reads answer 0; +0x280
     * reads its reset constant and stores nothing. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, IOM4 + 0x2C0u, 4u, 0u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, IOM4 + 0x280u, 4u, UINT32_C(0x1),
                                    &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, IOM4 + 0x280u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x00200000), value);
    /* +0x248: IDLEST reset bit 0x4; the tagged ERR bit 0 stores. */
    value = 9u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, IOM4 + 0x248u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x00000004), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, IOM4 + 0x248u, 4u, UINT32_C(0x55),
                                    &error));
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, IOM4 + 0x248u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x00000005), value);
    /* Transaction block (E-ULS-0026): observed stores and the
     * upstream field masks the lane applies on each of them. */
    {
        static const struct {
            uint32_t offset; uint32_t written; uint32_t read_back;
        } transaction[] = {
            { 0x120u, UINT32_C(0xFFFFFFFF), UINT32_C(0xFF3FFFFF) },
            { 0x124u, UINT32_C(0x0000001F), UINT32_C(0x00000010) },
            { 0x128u, UINT32_C(0xDEADBEEF), UINT32_C(0xDEADBEEF) },
            { 0x218u, UINT32_C(0xFFFFFFFF), UINT32_C(0x00000303) },
            { 0x21Cu, UINT32_C(0xFFFFFFFF), UINT32_C(0x00000FFF) },
            { 0x220u, UINT32_C(0xFFFFFFFF), UINT32_C(0x1FFFFFFF) },
            { 0x2C4u, UINT32_C(0x000003FF), UINT32_C(0x0000007F) }
        };
        size_t index;
        for (index = 0u; index < sizeof(transaction) / sizeof(
                 transaction[0]); ++index) {
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(
                bus, IOM4 + transaction[index].offset, 4u,
                transaction[index].written, &error));
            value = 0u;
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
                bus, IOM4 + transaction[index].offset, 4u, &value,
                &error));
            SEMU_TEST_EQ_U64(context, transaction[index].read_back,
                             value);
        }
    }
    {
        static const uint32_t zero_offsets[] = {
            0x208u, 0x228u, 0x22Cu, 0x234u, 0x23Cu, 0x240u, 0x244u
        };
        size_t index;
        for (index = 0u; index < sizeof(zero_offsets) / sizeof(
                 zero_offsets[0]); ++index) {
            value = 9u;
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
                bus, IOM4 + zero_offsets[index], 4u, &value, &error));
            SEMU_TEST_EQ_U64(context, UINT32_C(0), value);
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(
                bus, IOM4 + zero_offsets[index], 4u, UINT32_C(0x55),
                &error));
        }
    }
    semu_bus_destroy(bus);
}

static void test_unobserved_iom4_accesses_refused(semu_test_context *context)
{
    /* +0x204 (INTSTAT) became an observed read via the vector-10
     * handler disassembly (E-ULS-0030) and is covered by the engine
     * test's status sequence. */
    static const uint32_t read_offsets[] = {
        0x0u, 0x100u, 0x108u, 0x110u, 0x114u, 0x12Cu,
        0x214u, 0x224u, 0x2C8u, 0x300u
    };
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;
    size_t index;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    for (index = 0u; index < sizeof(read_offsets) / sizeof(read_offsets[0]);
         ++index) {
        semu_error_clear(&error);
        SEMU_TEST_ASSERT(context, SEMU_OK != semu_bus_read(
            bus, IOM4 + read_offsets[index], 4u, &value, &error));
        SEMU_TEST_ASSERT(context, error.code != SEMU_OK);
        semu_error_clear(&error);
        SEMU_TEST_ASSERT(context, SEMU_OK != semu_bus_write(
            bus, IOM4 + read_offsets[index], 4u, UINT32_C(1), &error));
        SEMU_TEST_ASSERT(context, error.code != SEMU_OK);
    }
    /* Unobserved widths refuse. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, SEMU_OK != semu_bus_read(
        bus, IOM4 + 0x11Cu, 2u, &value, &error));
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, SEMU_OK != semu_bus_write(
        bus, IOM4 + 0x104u, 1u, UINT32_C(0x10), &error));
    semu_bus_destroy(bus);
}

/*
 * Boot frontier: the first boot run now ends in the machine reset applied
 * after the guest's HardFault handler stores SYSRESETREQ: the TIMER0
 * comparator wake releases the WFI at instruction 12,650,485, the RTC
 * counter window answers the sleep-deepening path, which then reads the
 * still-unmodelled second PowerController window word 0x40021004
 * (precise bus fault at PC 0x00096a32), and the reset takes effect; a
 * budget of 14,769,033 stops at the first post-reset boot instruction
 * PC 0x001e1b4c, SP 0x1005ffc0 (reproduced twice). Ticket 730 frontier;
 * later instances extend this pin. */
static void test_boot_passes_iom4(semu_test_context *context)
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
        semu_run_limits limits = { UINT64_C(14769033), UINT64_C(4000000000) };
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
        SEMU_TEST_EQ_U64(context, UINT64_C(14769033),
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
        { "test_unobserved_iom4_accesses_refused",
          test_unobserved_iom4_accesses_refused },
        { "test_boot_passes_iom4", test_boot_passes_iom4 }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
