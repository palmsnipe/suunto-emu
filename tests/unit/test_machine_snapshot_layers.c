#include "test.h"
#include "semu/hash.h"
#include "../../src/boards/machine_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put64(uint8_t *data, uint64_t value)
{
    unsigned i;
    for (i = 0u; i < 8u; ++i) data[i] = (uint8_t)(value >> (8u * i));
}

static semu_machine *make_machine(char path[128], semu_error *error)
{
    static const uint8_t program[36] = {
        0u, 1u, 0u, 0x10u, 0x21u, 0u, 0u, 0u,
        [32] = 0u, 0xbfu, 0u, 0xbeu
    };
    semu_profile profile = {0};
    semu_firmware_manifest firmware = {0};
    semu_machine_options options = {0};
    semu_component *c = &firmware.components[0];
    FILE *stream;
    int written;
    if (!semu_test_temp_path(path, 128u, "snapshot-layers.bin")) return NULL;
    stream = fopen(path, "wb");
    if (stream == NULL) return NULL;
    written = fwrite(program, 1u, sizeof(program), stream) == sizeof(program);
    if (fclose(stream) != 0 || !written) return NULL;
    profile.format = firmware.format = 1u;
    strcpy(profile.id, "sapporo-2.22.60");
    strcpy(profile.board, "sapporo");
    strcpy(profile.product, "Synthetic"); strcpy(firmware.product, "Synthetic");
    strcpy(profile.version, "test"); strcpy(firmware.version, "test");
    profile.required_count = firmware.component_count = 1u;
    strcpy(c->id, "test"); strcpy(c->role, "application");
    (void)snprintf(c->path, sizeof(c->path), "%s", path);
    if (semu_sha256_file(path, c->sha256, &c->size, error) != SEMU_OK) return NULL;
    profile.required[0] = *c;
    profile.required[0].path[0] = '\0';
    options.profile = &profile; options.firmware = &firmware;
    return semu_machine_create(&options, error);
}

static void equal_snapshot(semu_test_context *context, const semu_snapshot *a,
                            const semu_snapshot *b)
{
    uint32_t id;
    for (id = 0u; id <= SEMU_SNAPSHOT_SECTION_MACHINE; ++id) {
        const uint8_t *left, *right;
        size_t left_size, right_size;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_snapshot_read_section(a, id, &left, &left_size));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_snapshot_read_section(b, id, &right, &right_size));
        SEMU_TEST_ASSERT(context, left_size == right_size &&
            memcmp(left, right, left_size) == 0);
    }
}

static void test_machine_snapshot_layer_counter_refusal(semu_test_context *context)
{
    semu_layer_intervention iv[] = {
        {"first", "synthetic", "test", 1u, 0u},
        {"second", "synthetic", "test", 1u, 0u}
    };
    semu_layer_descriptor desc = {"test-counts", SEMU_LAYER_DEVICE_FIXTURE,
        "sapporo-2.22.60", "test", NULL, 0u, iv, 2u, 2u};
    semu_error error;
    char path[128];
    semu_machine *m = make_machine(path, &error);
    semu_snapshot *source = semu_snapshot_create(&error);
    semu_snapshot *after = semu_snapshot_create(&error);
    semu_snapshot *before = semu_snapshot_create(&error);
    const uint8_t *data;
    uint8_t *valid, *bad;
    size_t size, aggregate, first;
    unsigned mode;
    SEMU_TEST_ASSERT(context, m != NULL && source != NULL && after != NULL && before != NULL);
    m->layer_count = 1u;
    m->layers[0] = (semu_layer_state){&desc, 2u, 1};
    iv[0].hits = iv[1].hits = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, source, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_snapshot_read_section(source, SEMU_SNAPSHOT_SECTION_MACHINE, &data, &size));
    valid = malloc(size); bad = malloc(size);
    SEMU_TEST_ASSERT(context, valid != NULL && bad != NULL);
    memcpy(valid, data, size);
    aggregate = 29u + strlen(desc.id); first = aggregate + 12u;
    SEMU_TEST_ASSERT(context, first + 16u == size);
    m->layers[0].hits = 0u; iv[0].hits = iv[1].hits = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_load(m, source, &error));
    SEMU_TEST_EQ_U64(context, 2u, m->layers[0].hits);
    SEMU_TEST_EQ_U64(context, 1u, iv[0].hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, before, &error));
    for (mode = 0u; mode < 4u; ++mode) {
        memcpy(bad, valid, size);
        if (mode == 0u) put64(bad + aggregate, 3u); /* Aggregate excess. */
        if (mode == 1u) put64(bad + first, 2u); /* Individual excess. */
        if (mode == 2u) put64(bad + aggregate, 1u); /* Sum exceeds total. */
        if (mode == 3u) {
            /* A huge valid bound must not let the sum wrap to one. */
            desc.maximum_hits = iv[0].max_hits = UINT64_MAX;
            put64(bad + aggregate, UINT64_MAX);
            put64(bad + first, UINT64_MAX);
        }
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_snapshot_write_section(source, SEMU_SNAPSHOT_SECTION_MACHINE, bad, size, &error));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
            semu_machine_snapshot_load(m, source, &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, after, &error));
        equal_snapshot(context, before, after);
        SEMU_TEST_EQ_U64(context, 2u, m->layers[0].hits);
        SEMU_TEST_EQ_U64(context, 1u, iv[0].hits);
        desc.maximum_hits = 2u; iv[0].max_hits = 1u;
    }
    free(valid); free(bad);
    semu_snapshot_destroy(source); semu_snapshot_destroy(before); semu_snapshot_destroy(after);
    semu_machine_destroy(m);
    (void)remove(path);
}

static void test_sapporo_222_budget_metadata(semu_test_context *context)
{
    const semu_layer_descriptor *d = &semu_sapporo_222_no_device_layer;
    semu_profile profile;
    semu_error error;
    size_t i;
    uint64_t sum = 0u;
    char line[256], expected[64];
    unsigned metadata_matches = 0u;
    FILE *stream;
    for (i = 0u; i < d->intervention_count; ++i) sum += d->interventions[i].max_hits;
    SEMU_TEST_EQ_U64(context, 20u, sum);
    SEMU_TEST_EQ_U64(context, sum, d->maximum_hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_profile_load("profiles/sapporo/2.22.60/profile.semu", &profile, &error));
    SEMU_TEST_EQ_U64(context, 1u, profile.layer_count);
    SEMU_TEST_ASSERT(context, strcmp(profile.layers[0], d->id) == 0);
    stream = fopen("profiles/sapporo/2.22.60/profile.semu", "rb");
    SEMU_TEST_ASSERT(context, stream != NULL);
    (void)snprintf(expected, sizeof(expected), "max_hits=%llu\n", (unsigned long long)sum);
    while (fgets(line, sizeof(line), stream) != NULL)
        if (strcmp(line, expected) == 0) ++metadata_matches;
    (void)fclose(stream);
    SEMU_TEST_EQ_U64(context, 1u, metadata_matches);
}

static void test_layer_aggregate_budget_refusal(semu_test_context *context)
{
    semu_layer_intervention iv = {"one", "test", "test", UINT64_MAX, 0u};
    semu_layer_descriptor d = {"test", SEMU_LAYER_DEVICE_FIXTURE, "test",
        "test", NULL, 0u, &iv, 1u, 2u};
    semu_layer_state state;
    semu_logger logger;
    semu_error error;
    FILE *stream = tmpfile();
    long end;
    SEMU_TEST_ASSERT(context, stream != NULL);
    semu_log_init(&logger, stream, SEMU_LOG_DEBUG);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_layer_enable(&state, &d, "test", &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_layer_hit(&state, &logger, "test", &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_layer_intervention_hit(&state, &logger, 0u, &error));
    end = ftell(stream);
    SEMU_TEST_ASSERT(context, end >= 0);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_layer_intervention_hit(&state, &logger, 0u, &error));
    SEMU_TEST_EQ_U64(context, 2u, state.hits);
    SEMU_TEST_EQ_U64(context, 1u, iv.hits);
    SEMU_TEST_EQ_U64(context, end, ftell(stream));
    d.maximum_hits = state.hits = UINT64_MAX;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_layer_intervention_hit(&state, &logger, 0u, &error));
    SEMU_TEST_EQ_U64(context, UINT64_MAX, state.hits);
    SEMU_TEST_EQ_U64(context, 1u, iv.hits);
    SEMU_TEST_EQ_U64(context, end, ftell(stream));
    (void)fclose(stream);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_machine_snapshot_layer_counter_refusal),
        SEMU_TEST_CASE(test_sapporo_222_budget_metadata),
        SEMU_TEST_CASE(test_layer_aggregate_budget_refusal)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
