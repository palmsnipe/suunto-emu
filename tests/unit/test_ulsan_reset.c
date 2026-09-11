/*
 * Ulsan 2.35.36 reset profile contract and private bounded run (ticket 725).
 * Evidence: E-ULS-0001 (component identities and vector tables), E-ULS-0002
 * (bounded reference reset trace), E-ULS-0005 (cross-version comparison),
 * E-ULS-0006 (boot tuple and memory map).
 * See tests/unit/test_ulsan_reset_map.c for the synthetic map/input tests.
 */

#include "semu/cpu.h"
#include "semu/hash.h"
#include "semu/machine.h"
#include "semu/manifest.h"
#include "test.h"
#include "../../src/boards/machine_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expected_hash(const semu_component *component, const char *hex,
                         uint64_t size, uint32_t load, const char *role)
{
    uint8_t digest[SEMU_SHA256_SIZE];
    return component != NULL && component->load_address == load &&
           component->size == size && strcmp(component->role, role) == 0 &&
           semu_sha256_parse(hex, digest) &&
           memcmp(component->sha256, digest, SEMU_SHA256_SIZE) == 0;
}

static const semu_component *find_required(const semu_profile *profile,
                                           const char *id)
{
    size_t index;
    for (index = 0u; index < profile->required_count; ++index) {
        if (strcmp(profile->required[index].id, id) == 0) {
            return &profile->required[index];
        }
    }
    return NULL;
}

static void test_profile_file_contract(semu_test_context *context)
{
    semu_profile profile;
    semu_error error;
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_profile_load(
        "profiles/ulsan/2.35.36/profile.semu", &profile, &error));
    SEMU_TEST_ASSERT(context, strcmp(profile.id, "ulsan-2.35.36") == 0);
    SEMU_TEST_ASSERT(context, strcmp(profile.board, "ulsan") == 0);
    SEMU_TEST_ASSERT(context, strcmp(profile.product, "Ulsan") == 0);
    SEMU_TEST_ASSERT(context,
        strcmp(profile.version, "2.35.36.10731-R") == 0);
    SEMU_TEST_EQ_U64(context, UINT64_C(0x00030000), profile.vector_table);
    SEMU_TEST_EQ_U64(context, 466u, profile.display_width);
    SEMU_TEST_EQ_U64(context, 466u, profile.display_height);
    SEMU_TEST_EQ_U64(context, 3u, profile.required_count);
    SEMU_TEST_ASSERT(context, expected_hash(
        find_required(&profile, "resident"),
        "8de61914e7de7b73e7a8b5a35adfd53332548a3b5611b4667f0fe41f7158d419",
        UINT64_C(124123), 0x00010000u, "resident"));
    SEMU_TEST_ASSERT(context, expected_hash(
        find_required(&profile, "application"),
        "5e229bb3893ee0ac0748d701bcd339aedb6dfd928250e69504e7d089d6cb5bc3",
        UINT64_C(1779802), 0x00030000u, "application"));
    /* The resources load address is the physical XIP placement of the
     * recovered tail: window base 0x18000000 + logical 0x00040000
     * (E-ULS-0006), ending exactly at the 32 MiB aperture edge. */
    SEMU_TEST_ASSERT(context, expected_hash(
        find_required(&profile, "resources"),
        "ef2a1358fff0c9d2eb1df31b65296b6e99eea729dc3d4921d0f7901e247ef906",
        UINT64_C(33292288), 0x18040000u, "resources"));
}

/* Private-firmware bounded run (skips when the bundle is absent; the
 * reference-lane stop for this version is E-ULS-0002, PC 0xdabcc, which the
 * in-tree model reaches only after device behavior from later tickets). */
static void test_real_firmware_stop_reproduced(semu_test_context *context)
{
    const char *manifest_path = getenv("SEMU_ULSAN_FIRMWARE_MANIFEST");
    FILE *probe;
    semu_firmware_manifest firmware;
    semu_machine_options options;
    semu_machine *machine;
    semu_profile profile;
    semu_run_limits limits = { UINT64_C(10000000), UINT64_C(4000000000) };
    semu_error error;
    semu_stop_reason first_reason;
    semu_stop_reason second_reason;
    uint32_t first_pc;
    uint32_t second_pc;
    uint64_t first_instructions;
    uint64_t second_instructions;
    uint64_t first_time;
    uint64_t second_time;

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
    /* E-ULS-0006 boot tuple: SP and reset vector fetched from the
     * application vector table at 0x00030000. */
    SEMU_TEST_EQ_U64(context, 0x001e1b4cu,
                     semu_machine_program_counter(machine));
    SEMU_TEST_EQ_U64(context, 0x1005ffc0u,
                     semu_cpu_get_state(machine->cpu)->r[13]);
    semu_error_clear(&error);
    first_reason = semu_machine_run(machine, &limits, &error);
    first_pc = semu_machine_program_counter(machine);
    first_instructions = semu_machine_instructions(machine);
    first_time = semu_machine_virtual_time(machine);
    printf("ULSAN 2.35.36 stop=%s pc=0x%08x instr=%llu time=%llu\n",
           semu_stop_reason_name(first_reason), first_pc,
           (unsigned long long)first_instructions,
           (unsigned long long)first_time);
    /* Recorded bounded stop (ticket 725 frontier): the guest executes the
     * pristine 2.35.36 application 10,000,000 instructions and hits the run
     * budget. Current CPU semantics set CFSR status bits for unmapped data
     * accesses but only stop on fault-in-handler/lockup paths, so no device
     * transaction has refused yet; virtual time does not advance because no
     * Ulsan event source is attached. This golden advances when device
     * behavior (tickets 730+) lands and must not be weakened. */
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_STOP_BUDGET, (uint64_t)first_reason);
    SEMU_TEST_EQ_U64(context, UINT64_C(0x000c258c), first_pc);
    SEMU_TEST_EQ_U64(context, UINT64_C(10000000), first_instructions);
    SEMU_TEST_EQ_U64(context, UINT64_C(10000000), first_time);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_reset(machine, &error));
    semu_error_clear(&error);
    second_reason = semu_machine_run(machine, &limits, &error);
    second_pc = semu_machine_program_counter(machine);
    second_instructions = semu_machine_instructions(machine);
    second_time = semu_machine_virtual_time(machine);
    SEMU_TEST_EQ_U64(context, (uint64_t)first_reason, (uint64_t)second_reason);
    SEMU_TEST_EQ_U64(context, first_pc, second_pc);
    SEMU_TEST_EQ_U64(context, first_instructions, second_instructions);
    SEMU_TEST_EQ_U64(context, first_time, second_time);
    semu_machine_destroy(machine);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_profile_file_contract),
        SEMU_TEST_CASE(test_real_firmware_stop_reproduced)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
