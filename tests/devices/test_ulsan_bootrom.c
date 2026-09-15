/*
 * Ulsan bootrom stub block and logger device tests (ticket 730, E-ULS-0009).
 *
 * Every pinned byte comes from the WriteWord/WriteDoubleWord list the
 * reference-lane platform description publishes for the block, or from the
 * zero-init fact for undelared offsets. The boot case is manifest-gated.
 */

#include "../../src/devices/ulsan_bootrom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semu/bus.h"
#include "semu/machine.h"
#include "semu/manifest.h"
#include "test.h"
#include "../../src/boards/machine_internal.h"
#include "../../src/boards/ulsan_board.h"

#define BOOTROM_ADDR 0x08000000u
#define LOGGER_ADDR 0x07FFFFFCu

static void test_declared_bytes_served(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* delay entry: adds r0, #15 ; subs r0, #1 ; cmp r0, #0 ; bne ; bx lr */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, BOOTROM_ADDR + 0x9Cu, 4u, &value,
                                   &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x3801300F), value);
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, BOOTROM_ADDR + 0xA0u, 4u, &value,
                                   &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0xD1FC2800), value);
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, BOOTROM_ADDR + 0x9Eu, 2u, &value,
                                   &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x3801), value);
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, BOOTROM_ADDR + 0x9Cu, 1u, &value,
                                   &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x0F), value);
    /* read_word: ldr r0, [r0] ; bx lr */
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, BOOTROM_ADDR + 0x74u, 4u, &value,
                                   &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x47706800), value);
    /* handler and its logger_address literal */
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, BOOTROM_ADDR + 0x30u, 4u, &value,
                                   &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x3014F8DF), value);
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, BOOTROM_ADDR + 0x48u, 4u, &value,
                                   &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x07FFFFFC), value);
    /* program_main2 (parm1 body): ldr r1, [r2, r0] ; str r1, [r3, r0] */
    value = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, BOOTROM_ADDR + 0x226u, 2u, &value,
                                   &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x5811), value);
    /* undeclared offsets are zero-initialized lane memory */
    value = 0xDEADBEEFu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, BOOTROM_ADDR + 0x10u, 4u, &value,
                                   &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    semu_bus_destroy(bus);
}

static void test_unobserved_transactions_refused(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;
    const uint32_t bad_reads[][2] = {
        { 0x00000001u, 2u }, /* odd offset, width 2 */
        { 0x00000002u, 4u }, /* unaligned width 4   */
        { 0x00000000u, 3u }  /* unsupported width   */
    };
    size_t index;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* The lane block is writable RAM, but no boot write to it is observed,
     * so every write to the bootrom and every BootromLogger access refuses.
     */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_write(bus, BOOTROM_ADDR + 0x9Cu, 4u,
                                    UINT32_C(0x47704770), &error) != SEMU_OK);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_write(bus, LOGGER_ADDR, 4u,
                                    UINT32_C(0x0009d20b), &error) != SEMU_OK);
    SEMU_TEST_ASSERT(context, strstr(error.text, "0x0009d20b") != NULL);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, LOGGER_ADDR, 4u, &value, &error) !=
                         SEMU_OK);
    for (index = 0u; index < sizeof(bad_reads) / sizeof(bad_reads[0]);
         ++index) {
        semu_error_clear(&error);
        SEMU_TEST_ASSERT(context,
                         semu_bus_read(bus, BOOTROM_ADDR + bad_reads[index][0],
                                       (unsigned)bad_reads[index][1], &value,
                                       &error) != SEMU_OK);
    }
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
static void test_boot_executes_bootrom_delay(semu_test_context *context)
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
        { "test_declared_bytes_served", test_declared_bytes_served },
        { "test_unobserved_transactions_refused",
          test_unobserved_transactions_refused },
        { "test_boot_executes_bootrom_delay", test_boot_executes_bootrom_delay }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
