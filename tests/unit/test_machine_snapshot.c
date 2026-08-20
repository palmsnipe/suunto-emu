#include "semu/hash.h"
#include "semu/machine.h"
#include "test.h"
#include "../../src/boards/machine_internal.h"
#include "../../src/core/scheduler_internal.h"
#include "../../src/soc/apollo4/apollo4_internal.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int write_bytes(const char *path, const uint8_t *program, size_t size)
{
    FILE *stream = fopen(path, "wb");
    if (stream == NULL) return 0;
    if (fwrite(program, 1u, size, stream) != size) {
        (void)fclose(stream); return 0;
    }
    return fclose(stream) == 0;
}

static void copy_text(char *destination, size_t capacity, const char *source)
{
    (void)snprintf(destination, capacity, "%s", source);
}

static uint32_t get_u32le(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u);
}

static uint64_t get_u64le(const uint8_t *data)
{
    return (uint64_t)get_u32le(data) |
           ((uint64_t)get_u32le(data + 4u) << 32u);
}

static void put_u32le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static void put_u64le(uint8_t *data, uint64_t value)
{
    put_u32le(data, (uint32_t)value);
    put_u32le(data + 4u, (uint32_t)(value >> 32u));
}

static void no_op_event(void *context, uint64_t now_ns)
{
    (void)context;
    (void)now_ns;
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
    if (semu_sha256_file(path, component->sha256, &size, error) != SEMU_OK)
        return 0;
    component->size = size;
    required = &profile->required[0];
    *required = *component;
    required->path[0] = '\0';
    return 1;
}

static void test_machine_snapshot_resume_and_atomic_refusal(
    semu_test_context *context)
{
    static const size_t buffer_size = 5u * 1024u * 1024u;
    uint8_t program[64] = {
        0x00u, 0x01u, 0x00u, 0x10u, 0x21u, 0x00u, 0x00u, 0x00u,
        [0x20] = 0x00u, 0xbfu, [0x22] = 0x00u, 0xbfu,
        [0x24] = 0x00u, 0xbfu, [0x26] = 0x00u, 0xbeu
    };
    static const uint8_t rx_bytes[] = { 0x5au };
    char path[128];
    semu_profile profile;
    semu_firmware_manifest firmware;
    semu_machine_options options;
    semu_machine *first;
    semu_machine *second;
    semu_snapshot *source;
    semu_snapshot *loaded;
    semu_snapshot *missing;
    semu_error error;
    semu_run_limits limits = { 2u, UINT64_C(1000000) };
    uint8_t *buffer;
    size_t length;
    uint64_t saved_instructions, saved_time;
    uint32_t saved_pc;
    uint64_t refused_instructions, refused_time;
    uint32_t refused_pc;
    semu_event_id unsupported_event;
    semu_event_id invalid_link_event;
    size_t index, section_size;
    size_t layer_id_length, malformed_size;
    const uint8_t *section_data;
    uint8_t *malformed;
    static const uint8_t invalid_cpu[] = { 0u };

    SEMU_TEST_ASSERT(context,
        semu_test_temp_path(path, sizeof(path), "machine-snapshot.bin"));
    SEMU_TEST_ASSERT(context, write_bytes(path, program, sizeof(program)));
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, make_contract(path, &profile, &firmware, &error));
    memset(&options, 0, sizeof(options));
    options.profile = &profile;
    options.firmware = &firmware;
    first = semu_machine_create(&options, &error);
    second = semu_machine_create(&options, &error);
    source = semu_snapshot_create(&error);
    loaded = semu_snapshot_create(&error);
    buffer = (uint8_t *)malloc(buffer_size);
    SEMU_TEST_ASSERT(context, first != NULL && second != NULL && source != NULL &&
                     loaded != NULL && buffer != NULL);
    if (first != NULL && second != NULL && source != NULL &&
        loaded != NULL && buffer != NULL) {
        first->layer_count = second->layer_count = 1u;
        first->layers[0] = (semu_layer_state){
            &semu_sapporo_222_no_device_layer, 0u, 0
        };
        second->layers[0] = first->layers[0];
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_scheduler_schedule(first->scheduler, 0u,
                                                  no_op_event, NULL,
                                                  &unsupported_event, &error));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                         semu_machine_snapshot_save(first, source, &error));
        SEMU_TEST_EQ_U64(context, 1u,
                         semu_scheduler_cancel(first->scheduler,
                                                unsupported_event));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_scheduler_schedule_tagged(
                             first->scheduler, 0u, SEMU_SCHED_EVENT_UART_RX,
                             UINT32_C(99), no_op_event, NULL,
                             &invalid_link_event, &error));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                         semu_machine_snapshot_save(first, source, &error));
        SEMU_TEST_EQ_U64(context, 1u,
                         semu_scheduler_cancel(first->scheduler,
                                                invalid_link_event));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_apollo4_uart_schedule_rx(
                             first->soc->uart, 100u, rx_bytes,
                             sizeof(rx_bytes), &error));
        SEMU_TEST_EQ_U64(context, SEMU_STOP_BUDGET,
                         semu_machine_run(first, &limits, &error));
        saved_instructions = semu_machine_instructions(first);
        saved_time = semu_machine_virtual_time(first);
        saved_pc = semu_machine_program_counter(first);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_machine_snapshot_save(first, source, &error));
        length = semu_snapshot_serialize(source, buffer, buffer_size);
        SEMU_TEST_ASSERT(context, length > 0u);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_snapshot_deserialize(loaded, buffer, length, &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_machine_snapshot_load(second, loaded, &error));
        SEMU_TEST_EQ_U64(context, saved_instructions, semu_machine_instructions(second));
        SEMU_TEST_EQ_U64(context, saved_time, semu_machine_virtual_time(second));
        SEMU_TEST_EQ_U64(context, saved_pc, semu_machine_program_counter(second));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_snapshot_read_section(loaded,
                             SEMU_SNAPSHOT_SECTION_SCHEDULER,
                             &section_data, &section_size));
        SEMU_TEST_ASSERT(context, section_size >= 60u &&
                         get_u32le(section_data + 24u) == 1u);
        if (section_size >= 60u && get_u32le(section_data + 24u) == 1u) {
            uint8_t *mismatched_scheduler = (uint8_t *)malloc(section_size);
            uint64_t next_id = get_u64le(section_data + 16u);
            uint64_t event_id = get_u64le(section_data + 44u);
            SEMU_TEST_ASSERT(context, mismatched_scheduler != NULL &&
                             next_id < UINT64_MAX && event_id < UINT64_MAX);
            if (mismatched_scheduler != NULL && next_id < UINT64_MAX &&
                event_id < UINT64_MAX) {
                memcpy(mismatched_scheduler, section_data, section_size);
                put_u64le(mismatched_scheduler + 16u, next_id + 1u);
                put_u64le(mismatched_scheduler + 44u, event_id + 1u);
                SEMU_TEST_EQ_U64(context, SEMU_OK,
                                 semu_snapshot_write_section(loaded,
                                     SEMU_SNAPSHOT_SECTION_SCHEDULER,
                                     mismatched_scheduler, section_size,
                                     &error));
                SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                                 semu_machine_snapshot_load(second, loaded,
                                                            &error));
                SEMU_TEST_EQ_U64(context, saved_instructions,
                                 semu_machine_instructions(second));
                SEMU_TEST_EQ_U64(context, saved_time,
                                 semu_machine_virtual_time(second));
                SEMU_TEST_EQ_U64(context, saved_pc,
                                 semu_machine_program_counter(second));
            }
            free(mismatched_scheduler);
        }
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_snapshot_read_section(loaded,
                             SEMU_SNAPSHOT_SECTION_MACHINE,
                             &section_data, &section_size));
        layer_id_length = strlen(semu_sapporo_222_no_device_layer.id);
        SEMU_TEST_ASSERT(context, section_size >= 28u + layer_id_length &&
                         get_u32le(section_data + 24u) == layer_id_length);
        malformed_size = section_size + 4u;
        malformed = (uint8_t *)malloc(malformed_size);
        SEMU_TEST_ASSERT(context, malformed != NULL);
        memcpy(malformed, section_data, 24u);
        put_u32le(malformed + 24u, (uint32_t)layer_id_length + 4u);
        memcpy(malformed + 28u, section_data + 28u, layer_id_length);
        memset(malformed + 28u + layer_id_length, 0, 4u);
        malformed[29u + layer_id_length] = 'x';
        memcpy(malformed + 28u + layer_id_length + 4u,
               section_data + 28u + layer_id_length,
               section_size - 28u - layer_id_length);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_snapshot_write_section(loaded,
                             SEMU_SNAPSHOT_SECTION_MACHINE, malformed,
                             malformed_size, &error));
        free(malformed);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                         semu_machine_snapshot_load(second, loaded, &error));
        SEMU_TEST_EQ_U64(context, saved_instructions, semu_machine_instructions(second));
        SEMU_TEST_EQ_U64(context, saved_time, semu_machine_virtual_time(second));
        SEMU_TEST_EQ_U64(context, saved_pc, semu_machine_program_counter(second));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_snapshot_deserialize(loaded, buffer, length, &error));
        limits.max_instructions = 4u;
        SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
                         semu_machine_run(first, &limits, &error));
        SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
                         semu_machine_run(second, &limits, &error));
        SEMU_TEST_EQ_U64(context, semu_machine_instructions(first), semu_machine_instructions(second));
        SEMU_TEST_EQ_U64(context, semu_machine_virtual_time(first), semu_machine_virtual_time(second));
        SEMU_TEST_EQ_U64(context, semu_machine_program_counter(first), semu_machine_program_counter(second));
        refused_instructions = semu_machine_instructions(second);
        refused_time = semu_machine_virtual_time(second);
        refused_pc = semu_machine_program_counter(second);
        missing = semu_snapshot_create(&error);
        SEMU_TEST_ASSERT(context, missing != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_snapshot_set_identity(missing,
            semu_snapshot_profile_id(source), semu_snapshot_firmware_hash(source), &error));
        for (index = 0u; index <= SEMU_SNAPSHOT_SECTION_MACHINE; ++index) {
            if (index == SEMU_SNAPSHOT_SECTION_VIRTUAL_TIME) continue;
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_snapshot_read_section(source, (uint32_t)index,
                                            &section_data, &section_size));
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_snapshot_write_section(missing, (uint32_t)index,
                                             section_data, section_size, &error));
        }
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                         semu_machine_snapshot_load(second, missing, &error));
        SEMU_TEST_EQ_U64(context, refused_instructions, semu_machine_instructions(second));
        SEMU_TEST_EQ_U64(context, refused_time, semu_machine_virtual_time(second));
        SEMU_TEST_EQ_U64(context, refused_pc, semu_machine_program_counter(second));
        semu_snapshot_destroy(missing);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_snapshot_write_section(loaded,
                             SEMU_SNAPSHOT_SECTION_CPU_STATE, invalid_cpu,
                             sizeof(invalid_cpu), &error));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                         semu_machine_snapshot_load(second, loaded, &error));
        SEMU_TEST_EQ_U64(context, refused_instructions, semu_machine_instructions(second));
        SEMU_TEST_EQ_U64(context, refused_time, semu_machine_virtual_time(second));
        SEMU_TEST_EQ_U64(context, refused_pc, semu_machine_program_counter(second));
    }
    free(buffer);
    semu_snapshot_destroy(source);
    semu_snapshot_destroy(loaded);
    semu_machine_destroy(first);
    semu_machine_destroy(second);
    (void)remove(path);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_machine_snapshot_resume_and_atomic_refusal)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
