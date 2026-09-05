#include "test.h"
#include "semu/hash.h"
#include "../../src/boards/machine_internal.h"
#include "../../src/compat/sapporo_239.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put32(uint8_t *p, uint32_t n)
{
    unsigned i;
    for (i = 0u; i < 4u; ++i) p[i] = (uint8_t)(n >> (8u * i));
}

static void make_machine(semu_test_context *context, semu_machine **machine,
                          char path[128], semu_error *error)
{
    static const uint8_t program[36] = {
        0x00u, 0x01u, 0x00u, 0x10u, 0x21u, 0x00u, 0x00u, 0x00u,
        [32] = 0x00u, 0xbfu, 0x00u, 0xbeu
    };
    semu_profile profile = {0};
    semu_firmware_manifest firmware = {0};
    semu_machine_options options = {0};
    semu_component *component = &firmware.components[0];
    FILE *stream;
    uint64_t size;
    SEMU_TEST_ASSERT(context,
        semu_test_temp_path(path, 128u, "preload1-snapshot.bin"));
    stream = fopen(path, "wb");
    SEMU_TEST_ASSERT(context, stream != NULL);
    SEMU_TEST_EQ_U64(context, sizeof(program),
        fwrite(program, 1u, sizeof(program), stream));
    SEMU_TEST_EQ_U64(context, 0u, fclose(stream));
    profile.format = firmware.format = 1u;
    (void)snprintf(profile.id, sizeof(profile.id), "sapporo-2.22.60");
    (void)snprintf(profile.board, sizeof(profile.board), "sapporo");
    (void)snprintf(profile.product, sizeof(profile.product), "Synthetic Sapporo");
    (void)snprintf(profile.version, sizeof(profile.version), "snapshot-test");
    (void)snprintf(firmware.product, sizeof(firmware.product), "%s", profile.product);
    (void)snprintf(firmware.version, sizeof(firmware.version), "%s", profile.version);
    profile.display_width = profile.display_height = 240u;
    profile.required_count = firmware.component_count = 1u;
    (void)snprintf(component->id, sizeof(component->id), "synthetic-reset");
    (void)snprintf(component->role, sizeof(component->role), "application");
    (void)snprintf(component->path, sizeof(component->path), "%s", path);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sha256_file(path, component->sha256, &size, error));
    component->size = size;
    profile.required[0] = *component;
    profile.required[0].path[0] = '\0';
    options.profile = &profile;
    options.firmware = &firmware;
    *machine = semu_machine_create(&options, error);
    SEMU_TEST_ASSERT(context, *machine != NULL);
    (*machine)->layer_count = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_layer_enable(&(*machine)->layers[0], &semu_sapporo_239_wbsto_layer,
                           "sapporo-2.39.20", error));
}

static void replace_count(semu_test_context *context, semu_snapshot *snapshot,
                           const uint8_t *original, size_t size,
                           unsigned count, semu_error *error)
{
    size_t count_offset = 37u + strlen(semu_sapporo_239_wbsto_layer.id);
    size_t prefix = count_offset + 4u;
    SEMU_TEST_ASSERT(context, size >= prefix + 32u);
    size_t tail = size - prefix - 32u;
    size_t total = prefix + (size_t)count * 8u + (count == 2u ? 0u : tail);
    uint8_t *replacement = calloc(total, 1u);
    unsigned i;
    SEMU_TEST_ASSERT(context, replacement != NULL && size >= prefix + 32u);
    memcpy(replacement, original, prefix);
    put32(replacement + count_offset, count);
    memset(replacement + count_offset - 8u, 0, 8u);
    put32(replacement + count_offset - 8u, count);
    for (i = 0u; i < count; ++i) replacement[prefix + i * 8u] = 1u;
    if (count != 2u)
        memcpy(replacement + prefix + count * 8u, original + prefix + 32u, tail);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_snapshot_write_section(snapshot, SEMU_SNAPSHOT_SECTION_MACHINE,
                                     replacement, total, error));
    free(replacement);
}

static void test_preload1_snapshot_counters_and_legacy(semu_test_context *context)
{
    semu_machine *machine = NULL;
    semu_snapshot *snapshot, *after;
    semu_error error;
    semu_logger logger;
    char path[128];
    const uint8_t *section;
    uint8_t *original, *before_bytes, *after_bytes;
    size_t size, before_size, after_size, capacity = 5u * 1024u * 1024u;
    unsigned count, i;
    semu_error_clear(&error);
    make_machine(context, &machine, path, &error);
    SEMU_TEST_ASSERT(context, machine != NULL);
    snapshot = semu_snapshot_create(&error);
    after = semu_snapshot_create(&error);
    SEMU_TEST_ASSERT(context, snapshot != NULL && after != NULL);
    semu_log_init(&logger, NULL, SEMU_LOG_ERROR);
    SEMU_TEST_EQ_U64(context, 4u, semu_sapporo_239_wbsto_layer.intervention_count);
    for (i = 0u; i < 4u; ++i)
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_layer_intervention_hit(&machine->layers[0], &logger, i, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_machine_snapshot_save(machine, snapshot, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_snapshot_read_section(snapshot, SEMU_SNAPSHOT_SECTION_MACHINE,
                                   &section, &size));
    original = malloc(size);
    before_bytes = malloc(capacity);
    after_bytes = malloc(capacity);
    SEMU_TEST_ASSERT(context,
        original != NULL && before_bytes != NULL && after_bytes != NULL);
    memcpy(original, section, size);
    for (count = 2u; count <= 4u; ++count) {
        replace_count(context, snapshot, original, size, count, &error);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_machine_snapshot_load(machine, snapshot, &error));
        SEMU_TEST_EQ_U64(context, count, machine->layers[0].hits);
        for (i = 0u; i < 4u; ++i)
            SEMU_TEST_EQ_U64(context, i < count ? 1u : 0u,
                semu_sapporo_239_wbsto_layer.interventions[i].hits);
    }
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_layer_intervention_hit(&machine->layers[0], &logger,
            SEMU_SAPPORO_239_IV_WBSTO_PRELOAD1_RESULT, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_machine_snapshot_save(machine, after, &error));
    before_size = semu_snapshot_serialize(after, before_bytes, capacity);
    SEMU_TEST_ASSERT(context, before_size != 0u);
    for (count = 0u; count <= 5u; ++count) {
        if (count >= 2u && count <= 4u) continue;
        replace_count(context, snapshot, original, size, count, &error);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
            semu_machine_snapshot_load(machine, snapshot, &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_machine_snapshot_save(machine, after, &error));
        after_size = semu_snapshot_serialize(after, after_bytes, capacity);
        SEMU_TEST_EQ_U64(context, before_size, after_size);
        SEMU_TEST_ASSERT(context,
            memcmp(before_bytes, after_bytes, before_size) == 0);
    }
    free(original);
    free(before_bytes);
    free(after_bytes);
    semu_snapshot_destroy(snapshot);
    semu_snapshot_destroy(after);
    semu_machine_destroy(machine);
    (void)remove(path);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_preload1_snapshot_counters_and_legacy)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
