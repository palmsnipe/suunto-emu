/*
 * Wismar 2.46.14 synthetic board map, NVIC 0xf0 mask, warn+0 gap law,
 * fail-closed stop, and input refusal (ticket 755, E-WIS-0001). No
 * firmware content is required.
 */

#include "semu/cpu.h"
#include "semu/hash.h"
#include "semu/input.h"
#include "semu/machine.h"
#include "semu/manifest.h"
#include "test.h"
#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "../../src/boards/machine_internal.h"
#include "../../src/cpu/armv7m/armv7m_internal.h"

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
    static const uint32_t loads[3] = { 0x00414000u, 0x00450000u,
                                       0x18040000u };
    size_t index;
    memset(profile, 0, sizeof(*profile));
    memset(firmware, 0, sizeof(*firmware));
    profile->format = 1u;
    copy_text(profile->id, sizeof(profile->id), "wismar-2.46.14");
    copy_text(profile->board, sizeof(profile->board), "wismar");
    copy_text(profile->product, sizeof(profile->product), "Wismar");
    copy_text(profile->version, sizeof(profile->version), "2.46.14.26212-U");
    profile->vector_table = 0x00450000u;
    profile->display_width = 466u;
    profile->display_height = 466u;
    profile->required_count = 3u;
    firmware->format = 1u;
    copy_text(firmware->product, sizeof(firmware->product), "Wismar");
    copy_text(firmware->version, sizeof(firmware->version), "2.46.14.26212-U");
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
 * the same bounded-halt class as the reference lane's boundary stop. The
 * entry sets MSPLIM/PSPLIM (v8-M stack limits, E-WIS-0001) first, so the
 * WFI also exercises the accepted MSR path. */
static int synth_sources(char paths[3][128], component_source sources[3],
                         uint8_t resident[64], uint8_t app[0x50],
                         uint8_t resources[256])
{
    size_t index;
    for (index = 0u; index < 64u; ++index) {
        resident[index] = 0xa5u;
    }
    memset(app, 0, 0x50u);
    put32(app + 0x00u, 0x20004000u);
    put32(app + 0x04u, 0x00450041u);
    /* msr MSPLIM, r0 (f3 80 88 0a); msr PSPLIM, r0 (f3 80 88 0b) */
    app[0x40] = 0x4fu; app[0x41] = 0xf3u;  /* placeholder; replaced below */
    /* Real sequence: ldr r0,[pc,#8]; msr 0x0a,r0; msr 0x0b,r0; wfi. */
    app[0x40] = 0x02u; app[0x41] = 0x48u;  /* ldr r0, [pc, #8] -> 0x4c */
    app[0x42] = 0x80u; app[0x43] = 0xf3u;
    app[0x44] = 0x0au; app[0x45] = 0x88u;  /* msr 0x0a, r0 (MSPLIM) */
    app[0x46] = 0x80u; app[0x47] = 0xf3u;
    app[0x48] = 0x0bu; app[0x49] = 0x88u;  /* msr 0x0b, r0 (PSPLIM) */
    app[0x4a] = 0x30u; app[0x4b] = 0xbfu;  /* wfi */
    put32(app + 0x4cu, 0x20004000u);       /* literal pool */
    memset(resources, 0x5c, 256u);
    if (!semu_test_temp_path(paths[0], sizeof(paths[0]), "wismar-res.bin") ||
        !semu_test_temp_path(paths[1], sizeof(paths[1]), "wismar-app.bin") ||
        !semu_test_temp_path(paths[2], sizeof(paths[2]), "wismar-resc.bin")) {
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

static int write4(semu_machine *machine, uint32_t address, uint32_t value)
{
    semu_error error;
    semu_error_clear(&error);
    return semu_bus_write(machine->bus, address, 4u, value, &error);
}

static void test_map_and_fail_closed_stop(semu_test_context *context)
{
    static const uint32_t mapped[] = { 0x00400000u, 0x006ffffcu,
        0x20000000u, 0x203ffffcu, 0x10160000u, 0x101bfffcu,
        0x101c0000u, 0x10266ffcu, 0x18000000u, 0x19fffffcu,
        0x40008800u, 0x40010400u, 0x4001047cu };
    /* 0x40008000 is mapped but its CTIMER control reads refuse by the
     * shared Apollo4 timer law (E-WIS-0001 records only the Timer14
     * control write); a refused read is the correct bounded answer. This
     * check runs after machine creation below. */
    static const uint32_t unmapped[] = { 0x00700000u, 0x20400000u,
        0x10267000u, 0x1a000000u };
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
    SEMU_TEST_EQ_U64(context, SEMU_OK, read4(machine, 0x00414000u, &value));
    SEMU_TEST_EQ_U64(context, 0xa5a5a5a5u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read4(machine, 0x00450000u, &value));
    SEMU_TEST_EQ_U64(context, 0x20004000u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read4(machine, 0x18040000u, &value));
    SEMU_TEST_EQ_U64(context, 0x5c5c5c5cu, value);
    SEMU_TEST_ASSERT(context,
                     semu_wismar_board_accepted("wismar", "wismar-2.46.14") != 0);
    SEMU_TEST_ASSERT(context,
                     semu_wismar_board_accepted("ulsan", "wismar-2.46.14") == 0);
    SEMU_TEST_ASSERT(context,
                     semu_wismar_board_accepted("wismar", "ulsan-2.44.52") == 0);

    /* Region boundaries from E-WIS-0001 plus the Wismar gap blocks are
     * mapped exactly as recorded. */
    for (index = 0u; index < sizeof(mapped) / sizeof(mapped[0]); ++index) {
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         read4(machine, mapped[index], &value));
    }
    /* Fail-closed edges: aperture overrun and every block outside the
     * recorded map stay unmapped. */
    for (index = 0u; index < sizeof(unmapped) / sizeof(unmapped[0]); ++index) {
        SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
                         read4(machine, unmapped[index], &value));
    }
    /* 0x40008000 is mapped but its CTIMER control reads refuse by the
     * shared Apollo4 timer law (E-WIS-0001 records only the Timer14
     * control write); a refused read is the correct bounded answer. */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     read4(machine, 0x40008000u, &value));

    /* E-WIS-0001 NVIC probe: with the 0xf0 mask bound, writing 0xffffffff
     * to IPR0 reads back 0xF0F0F0F0 and 0x12345678 to IPR1 reads back
     * 0x10305070 — the twice-observed lane readback. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write4(machine, 0xe000e400u, 0xffffffffu));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     read4(machine, 0xe000e400u, &value));
    SEMU_TEST_EQ_U64(context, 0xf0f0f0f0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write4(machine, 0xe000e404u, 0x12345678u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     read4(machine, 0xe000e404u, &value));
    SEMU_TEST_EQ_U64(context, 0x10305070u, value);

    /* Boot tuple (E-WIS-0001): SP/PC fetched from the vector table at
     * 0x00450000. */
    SEMU_TEST_EQ_U64(context, 0x00450040u,
                     semu_machine_program_counter(machine));
    SEMU_TEST_EQ_U64(context, 0x20004000u,
                     semu_cpu_get_state(machine->cpu)->r[13]);

    /* The v8-M stack-limit MSRs at the entry are accepted (E-WIS-0001) and
     * the stored values are observable. */
    first_reason = semu_machine_run(machine, &limits, &error);
    first_pc = semu_machine_program_counter(machine);
    first_instructions = semu_machine_instructions(machine);
    SEMU_TEST_EQ_U64(context, SEMU_STOP_WFI_DEADLOCK, first_reason);
    SEMU_TEST_EQ_U64(context, 0x20004000u,
                     semu_cpu_get_state(machine->cpu)->msplim);
    SEMU_TEST_EQ_U64(context, 0x20004000u,
                     semu_cpu_get_state(machine->cpu)->psplim);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_reset(machine, &error));
    /* Reset restores the 0xe0 default mask; the profile seam re-binds
     * 0xf0 only on the next select-profile seam, so the raw reset state
     * keeps the mask semantics deterministic. */
    SEMU_TEST_EQ_U64(context, (uint64_t)first_reason,
                     (uint64_t)semu_machine_run(machine, &limits, &error));
    second_pc = semu_machine_program_counter(machine);
    second_instructions = semu_machine_instructions(machine);
    SEMU_TEST_EQ_U64(context, first_pc, second_pc);
    SEMU_TEST_EQ_U64(context, first_instructions, second_instructions);

    /* E-WIS-0001 names no Wismar input pin, so every semantic input
     * refuses fail-closed. */
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
    copy_text(profile.board, sizeof(profile.board), "rostock");
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

static void test_priority_mask_refusal(semu_test_context *context)
{
    semu_cpu_fixture fixture;
    semu_error error;
    static const uint8_t program[] = { 0x00u, 0xbfu }; /* nop */
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, program,
                                                    sizeof(program)));
    /* Only 0xe0 and 0xf0 bind; anything else refuses and leaves the
     * previous selector in place. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, semu_cpu_bind_priority_mask(fixture.cpu, 0xf0u,
                                                          &error));
    SEMU_TEST_EQ_U64(context, 0xf0u, semu_cpu_priority_mask(fixture.cpu));
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, 0, semu_cpu_bind_priority_mask(fixture.cpu,
                                                             0xf8u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT, error.code);
    SEMU_TEST_EQ_U64(context, 0xf0u, semu_cpu_priority_mask(fixture.cpu));
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, semu_cpu_bind_priority_mask(fixture.cpu, 0xe0u,
                                                          &error));
    SEMU_TEST_EQ_U64(context, 0xe0u, semu_cpu_priority_mask(fixture.cpu));
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_map_and_fail_closed_stop),
        SEMU_TEST_CASE(test_metadata_refused_before_mapping),
        SEMU_TEST_CASE(test_priority_mask_refusal)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
