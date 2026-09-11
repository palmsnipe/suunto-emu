/*
 * Ulsan 2.35.36 synthetic board map, fail-closed stop, and input refusal
 * (ticket 725, E-ULS-0005/E-ULS-0006). No firmware content is required.
 */

#include "semu/cpu.h"
#include "semu/hash.h"
#include "semu/input.h"
#include "semu/machine.h"
#include "semu/manifest.h"
#include "test.h"
#include "../../src/boards/machine_internal.h"
#include "../../src/boards/ulsan_board.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_text(char *destination, size_t capacity, const char *source)
{
    strncpy(destination, source, capacity - 1u);
    destination[capacity - 1u] = '\0';
}

static void put32(uint8_t *bytes, uint32_t value)
{
    unsigned shift;
    for (shift = 0u; shift < 32u; shift += 8u) {
        bytes[shift / 8u] = (uint8_t)((value >> shift) & 0xffu);
    }
}

static int write_bytes(const char *path, const uint8_t *bytes, size_t size)
{
    FILE *stream = fopen(path, "wb");
    size_t written;
    if (stream == NULL) {
        return 0;
    }
    written = size != 0u ? fwrite(bytes, 1u, size, stream) : 0u;
    return fclose(stream) == 0 && (size == 0u || written == size);
}

typedef struct {
    const char *path;
    const uint8_t *data;
    uint64_t size;
} component_source;

static int make_contract(const component_source sources[3],
                         semu_profile *profile,
                         semu_firmware_manifest *firmware,
                         semu_error *error)
{
    static const char *ids[3] = { "resident", "application", "resources" };
    static const uint32_t loads[3] = { 0x00010000u, 0x00030000u,
                                       0x18040000u };
    size_t index;
    memset(profile, 0, sizeof(*profile));
    memset(firmware, 0, sizeof(*firmware));
    profile->format = 1u;
    copy_text(profile->id, sizeof(profile->id), "ulsan-2.35.36");
    copy_text(profile->board, sizeof(profile->board), "ulsan");
    copy_text(profile->product, sizeof(profile->product), "Ulsan");
    copy_text(profile->version, sizeof(profile->version), "2.35.36.10731-R");
    profile->vector_table = 0x00030000u;
    profile->display_width = 466u;
    profile->display_height = 466u;
    profile->required_count = 3u;
    firmware->format = 1u;
    copy_text(firmware->product, sizeof(firmware->product), "Ulsan");
    copy_text(firmware->version, sizeof(firmware->version), "2.35.36.10731-R");
    firmware->component_count = 3u;
    for (index = 0u; index < 3u; ++index) {
        semu_component *component = &firmware->components[index];
        uint64_t actual_size = 0u;
        copy_text(component->id, sizeof(component->id), ids[index]);
        copy_text(component->role, sizeof(component->role), ids[index]);
        copy_text(component->path, sizeof(component->path), sources[index].path);
        component->load_address = loads[index];
        component->size = sources[index].size;
        if (semu_sha256_file(sources[index].path, component->sha256,
                             &actual_size, error) != SEMU_OK ||
            actual_size != component->size) {
            return 0;
        }
        profile->required[index] = *component;
        profile->required[index].path[0] = '\0';
    }
    return 1;
}

/* The application entry executes WFI with no modeled event source, so the
 * bounded run stops in the deadlocked-WFI state (SEMU_STOP_WFI_DEADLOCK),
 * the same bounded-halt class as the reference lane's boundary stop. */
static int synth_sources(char paths[3][128], component_source sources[3],
                        uint8_t resident[64], uint8_t app[0x50],
                        uint8_t resources[256])
{
    size_t index;
    for (index = 0u; index < 64u; ++index) {
        resident[index] = 0xa5u;
    }
    memset(app, 0, 0x50u);
    put32(app + 0x00u, 0x1005ffc0u);
    put32(app + 0x04u, 0x00030041u);
    app[0x40] = 0x30u; app[0x41] = 0xbfu; /* wfi */
    app[0x42] = 0xfeu; app[0x43] = 0xe7u; /* b . */
    memset(resources, 0x5c, 256u);
    if (!semu_test_temp_path(paths[0], sizeof(paths[0]), "ulsan-res.bin") ||
        !semu_test_temp_path(paths[1], sizeof(paths[1]), "ulsan-app.bin") ||
        !semu_test_temp_path(paths[2], sizeof(paths[2]), "ulsan-resc.bin")) {
        return 0;
    }
    if (!write_bytes(paths[0], resident, 64u) ||
        !write_bytes(paths[1], app, 0x50u) ||
        !write_bytes(paths[2], resources, 256u)) {
        return 0;
    }
    sources[0].path = paths[0]; sources[0].data = resident; sources[0].size = 64u;
    sources[1].path = paths[1]; sources[1].data = app; sources[1].size = 0x50u;
    sources[2].path = paths[2]; sources[2].data = resources; sources[2].size = 256u;
    return 1;
}

static semu_machine *create_synthetic(const component_source sources[3],
                                      semu_profile *profile,
                                      semu_firmware_manifest *firmware,
                                      semu_error *error)
{
    semu_machine_options options;
    memset(&options, 0, sizeof(options));
    if (!make_contract(sources, profile, firmware, error)) {
        return NULL;
    }
    options.profile = profile;
    options.firmware = firmware;
    return semu_machine_create(&options, error);
}

static int read4(semu_machine *machine, uint32_t address, uint32_t *value)
{
    semu_error error;
    semu_error_clear(&error);
    return semu_bus_read(machine->bus, address, 4u, value, &error);
}

static void test_map_and_fail_closed_stop(semu_test_context *context)
{
    static const uint32_t mapped[] = { 0x00000000u, 0x001ffffcu, 0x10000000u,
        0x1005fffcu, 0x10060000u, 0x1015fffcu, 0x10160000u, 0x101bfffcu,
        0x101c0000u, 0x10266ffcu, 0x18040000u, 0x19fffffcu };
    static const uint32_t unmapped[] = { 0x00200000u, 0x10267000u,
        0x18000000u, 0x1803fff8u, 0x1a000000u, 0x07fffffcu, 0x08000000u,
        0x40004000u, 0x40061000u, 0x400a0000u };
    char paths[3][128];
    component_source sources[3];
    uint8_t resident[64];
    uint8_t app[0x50];
    uint8_t resources[256];
    semu_firmware_manifest firmware;
    semu_machine *machine;
    semu_profile profile;
    semu_run_limits limits = { 100u, UINT64_C(1000000) };
    semu_error error;
    uint32_t value = 0u;
    semu_stop_reason first_reason;
    uint32_t first_pc;
    uint32_t second_pc;
    uint64_t first_instructions;
    uint64_t second_instructions;
    size_t index;
    semu_input_event event;
    SEMU_TEST_ASSERT(context,
        synth_sources(paths, sources, resident, app, resources));
    semu_error_clear(&error);
    machine = create_synthetic(sources, &profile, &firmware, &error);
    SEMU_TEST_ASSERT(context, machine != NULL);

    /* Loaded content is visible at the exact proven placements. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, read4(machine, 0x00010000u, &value));
    SEMU_TEST_EQ_U64(context, 0xa5a5a5a5u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read4(machine, 0x00030000u, &value));
    SEMU_TEST_EQ_U64(context, 0x1005ffc0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read4(machine, 0x18040000u, &value));
    SEMU_TEST_EQ_U64(context, 0x5c5c5c5cu, value);
    SEMU_TEST_ASSERT(context,
                     semu_ulsan_board_accepted("ulsan", "ulsan-2.35.36") != 0);
    SEMU_TEST_ASSERT(context,
                     semu_ulsan_board_accepted("sapporo", "ulsan-2.35.36") == 0);

    /* Region boundaries from E-ULS-0006 are mapped exactly as recorded. */
    for (index = 0u; index < sizeof(mapped) / sizeof(mapped[0]); ++index) {
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         read4(machine, mapped[index], &value));
    }
    /* Fail-closed edges: personalization hole, bootrom, aperture overrun,
     * and every observed-but-unimplemented SoC block stay unmapped. */
    for (index = 0u; index < sizeof(unmapped) / sizeof(unmapped[0]); ++index) {
        SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
                         read4(machine, unmapped[index], &value));
    }

    /* Boot tuple (E-ULS-0006): SP/PC fetched from the vector table. */
    SEMU_TEST_EQ_U64(context, 0x00030040u,
                     semu_machine_program_counter(machine));
    SEMU_TEST_EQ_U64(context, 0x1005ffc0u,
                     semu_cpu_get_state(machine->cpu)->r[13]);

    /* Fail-closed bounded stop, reproduced identically after reset. */
    first_reason = semu_machine_run(machine, &limits, &error);
    first_pc = semu_machine_program_counter(machine);
    first_instructions = semu_machine_instructions(machine);
    SEMU_TEST_EQ_U64(context, SEMU_STOP_WFI_DEADLOCK, first_reason);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_reset(machine, &error));
    SEMU_TEST_EQ_U64(context, (uint64_t)first_reason,
                     (uint64_t)semu_machine_run(machine, &limits, &error));
    second_pc = semu_machine_program_counter(machine);
    second_instructions = semu_machine_instructions(machine);
    SEMU_TEST_EQ_U64(context, first_pc, second_pc);
    SEMU_TEST_EQ_U64(context, first_instructions, second_instructions);

    /* No Ulsan evidence names an input pin, so every semantic input
     * refuses fail-closed (E-ULS-0005/E-ULS-0006). */
    memset(&event, 0, sizeof(event));
    event.kind = SEMU_INPUT_BUTTON;
    event.value = 1;
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_machine_input(machine, &event, &error));
    semu_machine_destroy(machine);
}

static void test_metadata_refused_before_mapping(semu_test_context *context)
{
    char paths[3][128];
    component_source sources[3];
    uint8_t resident[64];
    uint8_t app[0x50];
    uint8_t resources[256];
    semu_firmware_manifest firmware;
    semu_machine_options options;
    semu_machine *machine;
    semu_profile profile;
    semu_error error;
    uint8_t damaged[0x50];

    SEMU_TEST_ASSERT(context,
        synth_sources(paths, sources, resident, app, resources));

    /* Unknown board identities are refused before any bus work. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
        make_contract(sources, &profile, &firmware, &error));
    copy_text(profile.board, sizeof(profile.board), "tulsa");
    memset(&options, 0, sizeof(options));
    options.profile = &profile;
    options.firmware = &firmware;
    machine = semu_machine_create(&options, &error);
    SEMU_TEST_ASSERT(context, machine == NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, error.code);

    /* Profile/manifest metadata disagreement is refused. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
        make_contract(sources, &profile, &firmware, &error));
    firmware.components[1].sha256[0] ^= 0x01u;
    options.profile = &profile;
    options.firmware = &firmware;
    machine = semu_machine_create(&options, &error);
    SEMU_TEST_ASSERT(context, machine == NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT, error.code);

    /* On-disk bytes that differ from the pinned hash are refused even when
     * size and structure are intact. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
        make_contract(sources, &profile, &firmware, &error));
    memcpy(damaged, app, sizeof(damaged));
    damaged[0x48] ^= 0xffu;
    SEMU_TEST_ASSERT(context, write_bytes(paths[1], damaged, sizeof(damaged)));
    options.profile = &profile;
    options.firmware = &firmware;
    machine = semu_machine_create(&options, &error);
    SEMU_TEST_ASSERT(context, machine == NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT, error.code);
    /* Restore so repeated test runs see pristine synthetic sources. */
    SEMU_TEST_ASSERT(context, write_bytes(paths[1], app, 0x50u));
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_map_and_fail_closed_stop),
        SEMU_TEST_CASE(test_metadata_refused_before_mapping)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
