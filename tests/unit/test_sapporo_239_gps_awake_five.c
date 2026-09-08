/* Reuse the unchanged synthetic lifecycle/refusal fixture, not a runtime API. */
#define main legacy_awake_test_main
#include "test_sapporo_239_gps_awake.c"
#undef main

static void test_five_activation_and_exclusion(semu_test_context *context)
{
    fixture f; semu_error e; semu_sapporo_devices before;
    semu_layer_state state, layers[4];
    const semu_layer_descriptor *d = &semu_sapporo_239_gps_awake_five_layer;
    const char *hashes[3];
    for (unsigned i = 0u; i < 3u; ++i) hashes[i] = d->component_hashes[i];
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_layer_enable_checked(&state, d, "sapporo-2.39.20", hashes, 3u, &e));
    for (unsigned i = 0u; i < 3u; ++i) {
        hashes[i] = "wrong";
        SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
            semu_layer_enable_checked(&state, d, "sapporo-2.39.20", hashes, 3u, &e));
        SEMU_TEST_ASSERT(context, !state.enabled); hashes[i] = d->component_hashes[i];
    }
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
        semu_layer_enable_checked(&state, d, "sapporo-2.22.60", hashes, 3u, &e));
    for (unsigned order = 0u; order < 2u; ++order) {
        SEMU_TEST_ASSERT(context, init_layer(&f, order ? d : &semu_sapporo_239_gps_awake_layer, &e));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_layer_enable(&f.layers[3],
            order ? &semu_sapporo_239_gps_awake_layer : d, "sapporo-2.39.20", &e));
        before = f.d; memcpy(layers, f.layers, sizeof(layers));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
            semu_sapporo_devices_bind_gps_layers(&f.d, f.layers, 4u, &f.logger, &e));
        SEMU_TEST_ASSERT(context, !memcmp(&before, &f.d, sizeof(before)) &&
            !memcmp(layers, f.layers, sizeof(layers)));
        destroy(&f);
    }
}

static void test_five_pulses_and_instance_ownership(semu_test_context *context)
{
    fixture f, other; semu_error e; semu_cpu_state cpu;
    char log[2048]; size_t n;
    SEMU_TEST_ASSERT(context, init_layer(&f, &semu_sapporo_239_gps_awake_five_layer, &e));
    SEMU_TEST_ASSERT(context, init(&other, &e)); cpu = f.cpu;
    for (unsigned i = 0u; i < 5u; ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_OK, poll_awake(&f, &e));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(f.scheduler, 99999999u, &e));
        SEMU_TEST_EQ_U64(context, i, f.highs);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(f.scheduler, 1u, &e));
        SEMU_TEST_EQ_U64(context, i + 1u, f.highs);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(f.scheduler, 1000000u, &e));
    }
    SEMU_TEST_EQ_U64(context, 5u, f.layers[2].hits);
    SEMU_TEST_EQ_U64(context, 0u, other.layers[2].hits);
    SEMU_TEST_ASSERT(context, !memcmp(&cpu, &f.cpu, sizeof(cpu)));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, poll_awake(&f, &e));
    rewind(f.log); n = fread(log, 1u, sizeof(log) - 1u, f.log); log[n] = '\0';
    SEMU_TEST_ASSERT(context, strstr(log, "layer=sapporo-2.39-gps-awake-five") &&
        strstr(log, "ordinal=5") && strstr(log, "provenance=E-SAP-GPS-FIFTH-239-002"));
    /* Identity, not caller-controlled maximum, fixes the allowable budget. */
    f.d.gps_awake_context.descriptor.maximum_hits = 6u;
    f.d.gps_awake_context.intervention.max_hits = 6u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, poll_awake(&f, &e));
    destroy(&f); destroy(&other);
}

static void test_five_atomic_refusals(semu_test_context *context)
{
    refusals(context, &semu_sapporo_239_gps_awake_five_layer);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_five_activation_and_exclusion),
        SEMU_TEST_CASE(test_five_pulses_and_instance_ownership),
        SEMU_TEST_CASE(test_five_atomic_refusals)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
