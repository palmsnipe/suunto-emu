#include "semu/hash.h"
#include "semu/machine.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static int write_program(const char *path, uint8_t program[34])
{
    FILE *stream = fopen(path, "wb");
    if (stream == NULL) {
        return 0;
    }
    if (fwrite(program, 1u, 34u, stream) != 34u) {
        (void)fclose(stream);
        return 0;
    }
    return fclose(stream) == 0;
}

static void copy_text(char *destination, size_t capacity, const char *source)
{
    (void)snprintf(destination, capacity, "%s", source);
}

static int make_contract(const char *path, semu_profile *profile,
                         semu_firmware_manifest *firmware, semu_error *error)
{
    semu_component *required;
    semu_component *component;
    uint64_t size;
    memset(profile, 0, sizeof(*profile));
    memset(firmware, 0, sizeof(*firmware));
    profile->format = 1u;
    copy_text(profile->id, sizeof(profile->id), "sapporo-2.22.60");
    copy_text(profile->board, sizeof(profile->board), "sapporo");
    copy_text(profile->product, sizeof(profile->product), "Synthetic Sapporo");
    copy_text(profile->version, sizeof(profile->version), "machine-test");
    profile->display_width = 240u;
    profile->display_height = 240u;
    profile->required_count = 1u;

    firmware->format = 1u;
    copy_text(firmware->product, sizeof(firmware->product), profile->product);
    copy_text(firmware->version, sizeof(firmware->version), profile->version);
    firmware->component_count = 1u;
    component = &firmware->components[0];
    copy_text(component->id, sizeof(component->id), "synthetic-reset");
    copy_text(component->role, sizeof(component->role), "application");
    copy_text(component->path, sizeof(component->path), path);
    component->load_address = 0u;
    if (semu_sha256_file(path, component->sha256, &size, error) != SEMU_OK) {
        return 0;
    }
    component->size = size;
    required = &profile->required[0];
    *required = *component;
    required->path[0] = '\0';
    return 1;
}

static void test_repeated_reset_and_source_guard(semu_test_context *context)
{
    uint8_t program[34] = {
        0x00u, 0x01u, 0x00u, 0x10u, /* MSP = 0x10000100 */
        0x21u, 0x00u, 0x00u, 0x00u, /* reset = 0x00000021 */
        [32] = 0x00u, [33] = 0xbeu  /* BKPT */
    };
    uint8_t source_before[SEMU_SHA256_SIZE];
    uint8_t source_after[SEMU_SHA256_SIZE];
    semu_firmware_manifest firmware;
    semu_machine_options options;
    semu_run_limits limits = {8u, 8u};
    semu_profile profile;
    semu_machine *machine;
    semu_error error;
    char path[128];
    uint64_t size;
    uint64_t first_instructions;
    uint64_t first_time;
    uint32_t first_pc;

    SEMU_TEST_ASSERT(context,
        semu_test_temp_path(path, sizeof(path), "machine.bin"));
    SEMU_TEST_ASSERT(context, write_program(path, program));
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
        make_contract(path, &profile, &firmware, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sha256_file(path, source_before, &size, &error));
    memset(&options, 0, sizeof(options));
    options.profile = &profile;
    options.firmware = &firmware;
    machine = semu_machine_create(&options, &error);
    SEMU_TEST_ASSERT(context, machine != NULL);

    SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
        semu_machine_run(machine, &limits, &error));
    first_instructions = semu_machine_instructions(machine);
    first_time = semu_machine_virtual_time(machine);
    first_pc = semu_machine_program_counter(machine);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_reset(machine, &error));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
        semu_machine_run(machine, &limits, &error));
    SEMU_TEST_EQ_U64(context, first_instructions,
                     semu_machine_instructions(machine));
    SEMU_TEST_EQ_U64(context, first_time, semu_machine_virtual_time(machine));
    SEMU_TEST_EQ_U64(context, first_pc, semu_machine_program_counter(machine));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sha256_file(path, source_after, &size, &error));
    SEMU_TEST_ASSERT(context,
        memcmp(source_before, source_after, sizeof(source_before)) == 0);

    program[33] = 0xbfu;
    SEMU_TEST_ASSERT(context, write_program(path, program));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
                     semu_machine_reset(machine, &error));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_DEVICE_REFUSED,
                     semu_machine_stop_reason(machine));
    semu_machine_destroy(machine);
    (void)remove(path);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_repeated_reset_and_source_guard)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
