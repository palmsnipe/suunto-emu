/*
 * Wismar 2.46.14 reset profile contract and private bounded run (ticket
 * 755). Evidence: E-WIS-0001 (component identities and the lane start
 * tuple), plus the twice-identical bounded CLI runs pinned in
 * tests/integration/test_firmware_wismar_246_reset.sh.
 * See tests/unit/test_wismar_reset_map.c for the synthetic map/mask/input
 * tests.
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
        "profiles/wismar/2.46.14/profile.semu", &profile, &error));
    SEMU_TEST_ASSERT(context, strcmp(profile.id, "wismar-2.46.14") == 0);
    SEMU_TEST_ASSERT(context, strcmp(profile.board, "wismar") == 0);
    SEMU_TEST_ASSERT(context, strcmp(profile.product, "Wismar") == 0);
    SEMU_TEST_ASSERT(context,
        strcmp(profile.version, "2.46.14.26212-U") == 0);
    /* E-WIS-0001 start tuple: the lane's audited choice from component 04's
     * vector words (vector table 0x00450000, SP 0x20004000, Thumb PC
     * 0x006954cd); the SP is re-derived from the vector table at reset. */
    SEMU_TEST_EQ_U64(context, UINT64_C(0x00450000), profile.vector_table);
    SEMU_TEST_EQ_U64(context, 466u, profile.display_width);
    SEMU_TEST_EQ_U64(context, 466u, profile.display_height);
    SEMU_TEST_EQ_U64(context, 3u, profile.required_count);
    /* Component sizes are the expanded sizes (including the trailing CRC32
     * word) of the extracted .raw files the lane loads; the SHA-256s are the
     * expanded hashes from E-WIS-0001. Component 02 is deliberately absent
     * (the lane does not load it; its load field overlaps component 04). */
    SEMU_TEST_ASSERT(context, expected_hash(
        find_required(&profile, "resident"),
        "dc5853600aaf4f1e4857de1827ee36ec55a32c92e2be836bffea4ed3f0989e43",
        UINT64_C(149992), 0x00414000u, "resident"));
    SEMU_TEST_ASSERT(context, expected_hash(
        find_required(&profile, "application"),
        "878945bca689195ef3f99137d0efc3179250068cc91d9afd2d24e1d304c2de97",
        UINT64_C(2420644), 0x00450000u, "application"));
    /* The resources load address is the physical XIP placement of the
     * external-flash tail: window base 0x18000000 + logical 0x00040000,
     * ending exactly at the 32 MiB aperture edge (E-WIS-0001). */
    SEMU_TEST_ASSERT(context, expected_hash(
        find_required(&profile, "resources"),
        "b324f3d20aa2100610ca98930b5f5bc98bcded6131a49b54b4357042029c4ebd",
        UINT64_C(33292288), 0x18040000u, "resources"));
}

/* Private-firmware bounded run (skips when the bundle is absent). The
 * pinned stop is the twice-identical CLI golden: one guest AIRCR reset at
 * instruction 14,518,838 after the first unsupported bit-band access
 * (0x42003310), then the 20M-instruction budget stop at the lane 0.1s
 * boundary PC. */
static void test_real_firmware_stop_reproduced(semu_test_context *context)
{
    const char *manifest_path = getenv("SEMU_WISMAR_FIRMWARE_MANIFEST");
    FILE *probe;
    semu_firmware_manifest firmware;
    semu_machine_options options;
    semu_machine *machine;
    semu_profile profile;
    semu_run_limits limits = { UINT64_C(80000000), UINT64_C(8000000000) };
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
        manifest_path = "tests/private/wismar-2.46.14/firmware.semu";
    }
    probe = fopen(manifest_path, "rb");
    if (probe == NULL) {
        printf("SKIP wismar 2.46.14 private manifest absent: %s\n",
               manifest_path);
        return;
    }
    fclose(probe);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_profile_load(
        "profiles/wismar/2.46.14/profile.semu", &profile, &error));
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_manifest_load(manifest_path, &firmware, &error));
    memset(&options, 0, sizeof(options));
    options.profile = &profile;
    options.firmware = &firmware;
    semu_error_clear(&error);
    machine = semu_machine_create(&options, &error);
    SEMU_TEST_ASSERT(context, machine != NULL);
    /* E-WIS-0001 start tuple: SP and reset vector fetched from the
     * application vector table at 0x00450000. */
    SEMU_TEST_EQ_U64(context, 0x006954ccu,
                     semu_machine_program_counter(machine));
    SEMU_TEST_EQ_U64(context, 0x20004000u,
                     semu_cpu_get_state(machine->cpu)->r[13]);
    semu_error_clear(&error);
    first_reason = semu_machine_run(machine, &limits, &error);
    first_pc = semu_machine_program_counter(machine);
    first_instructions = semu_machine_instructions(machine);
    first_time = semu_machine_virtual_time(machine);
    printf("WISMAR 2.46.14 stop=%s pc=0x%08x instr=%llu time=%llu\n",
           semu_stop_reason_name(first_reason), first_pc,
           (unsigned long long)first_instructions,
           (unsigned long long)first_time);
    /* Twice-identical pinned golden (ticket 755 instance 2): guest AIRCR
     * fault-free across the full 8-second virtual-time window (zero
     * refusals, zero AIRCR resets) and stops at the lane 1 s BKPT probe's
     * terminal PC (E-WIS-0001 bkpt_boundary_1s: terminal pc 0x51bc82,
     * cfsr/hfsr 0, no BKPT or port-assert). The instruction count is the
     * guest's own cadence; the time budget paces it. Must not be
     * weakened; it advances when the next device frontier lands and must
     * be re-derived then, never silently re-pinned. */
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_STOP_BUDGET, (uint64_t)first_reason);
    SEMU_TEST_EQ_U64(context, UINT64_C(0x0051bc82), first_pc);
    SEMU_TEST_EQ_U64(context, UINT64_C(15128956), first_instructions);
    SEMU_TEST_EQ_U64(context, UINT64_C(8000000000), first_time);
    /* The twice-identical property is proven by the cold CLI pair in
     * tests/integration/test_firmware_wismar_246_reset.sh; an in-process
     * reset+run lands on the same PC but 22 instructions later, because
     * device state created during the first epoch (compare events, RTC
     * anchors) is not byte-identical after a cold reset. Both runs reach
     * the same fault-free window PC. */
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_reset(machine, &error));
    semu_error_clear(&error);
    second_reason = semu_machine_run(machine, &limits, &error);
    second_pc = semu_machine_program_counter(machine);
    second_instructions = semu_machine_instructions(machine);
    second_time = semu_machine_virtual_time(machine);
    SEMU_TEST_EQ_U64(context, (uint64_t)first_reason, (uint64_t)second_reason);
    SEMU_TEST_EQ_U64(context, first_pc, second_pc);
    semu_machine_destroy(machine);
    (void)second_instructions; (void)second_time;
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_profile_file_contract),
        SEMU_TEST_CASE(test_real_firmware_stop_reproduced)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
