#include "test.h"
#include "../../src/compat/sapporo_239.h"
#include <stdio.h>
#include <string.h>

#define BASE UINT32_C(0x10000000)
#define CLIENT (BASE + UINT32_C(0x25634))
#define CACHE (BASE + UINT32_C(0x21c))

typedef struct fixture {
    semu_bus *bus;
    semu_cpu_state cpu;
    semu_layer_state layer;
    semu_logger logger;
    semu_error error;
    FILE *log;
} fixture;

static semu_status invoke(fixture *f)
{
    return semu_sapporo_239_apply_wbsto_hook(f->bus, &f->cpu, &f->layer,
                                           &f->logger, &f->error);
}

static void setup(semu_test_context *context, fixture *f, unsigned stage)
{
    static const uint32_t words[] = {
        0x192dc0u, 61u, BASE + 4u, 0x1f4u, BASE + 4u, 0u,
        BASE + 0x2a0u, 0x700u, BASE + 0x2a0u, 0u
    };
    size_t i;
    memset(f, 0, sizeof(*f));
    f->bus = semu_bus_create(&f->error);
    f->log = tmpfile();
    SEMU_TEST_ASSERT(context, f->bus != NULL && f->log != NULL);
    semu_log_init(&f->logger, f->log, SEMU_LOG_INFO);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_map_ram(f->bus, "sram", BASE, 0x30000u, &f->error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_layer_enable_checked(&f->layer, &semu_sapporo_239_wbsto_layer,
            "sapporo-2.39.20", semu_sapporo_239_wbsto_layer.component_hashes,
            3u, &f->error));
    for (i = 0u; i < SEMU_ARRAY_LEN(words); ++i)
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_write(f->bus, CACHE + (uint32_t)i * 4u, 4u,
                           words[i], &f->error));
    f->cpu.r[0] = CLIENT;
    f->cpu.r[3] = 200u;
    f->cpu.r[15] = 0x124844u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(f->bus, CLIENT + 12u, 4u, 0x1c0ec8u, &f->error));
    if (stage >= 1u) SEMU_TEST_EQ_U64(context, SEMU_OK, invoke(f));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(f->bus, CLIENT + 12u, 4u, 0x1c0ed8u, &f->error));
    f->cpu.r[3] = 500u;
    if (stage >= 2u) SEMU_TEST_EQ_U64(context, SEMU_OK, invoke(f));
    f->cpu.r[3] = 500u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(f->bus, CLIENT + 16u, 1u, 1u, &f->error));
}

static void destroy(fixture *f)
{
    semu_bus_destroy(f->bus);
    (void)fclose(f->log);
}

static void unchanged(semu_test_context *context, fixture *f,
                       semu_status expected)
{
    uint8_t before[0x300u], after[0x300u];
    semu_cpu_state cpu = f->cpu;
    uint64_t hits = f->layer.hits;
    uint64_t counters[8];
    size_t i;
    SEMU_TEST_ASSERT(context, f->layer.descriptor->intervention_count <= 8u);
    for (i = 0u; i < f->layer.descriptor->intervention_count; ++i)
        counters[i] = f->layer.descriptor->interventions[i].hits;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(f->bus, BASE, before, sizeof(before), &f->error));
    SEMU_TEST_EQ_U64(context, expected, invoke(f));
    SEMU_TEST_EQ_U64(context, hits, f->layer.hits);
    SEMU_TEST_ASSERT(context, memcmp(&cpu, &f->cpu, sizeof(cpu)) == 0);
    for (i = 0u; i < f->layer.descriptor->intervention_count; ++i)
        SEMU_TEST_EQ_U64(context, counters[i],
            f->layer.descriptor->interventions[i].hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(f->bus, BASE, after, sizeof(after), &f->error));
    SEMU_TEST_ASSERT(context, memcmp(before, after, sizeof(before)) == 0);
}

static void test_preload1_success_and_repeat(semu_test_context *context)
{
    fixture f;
    char line[512];
    unsigned found = 0u;
    uint8_t before[0x300u], after[0x300u];
    semu_cpu_state cpu;
    setup(context, &f, 2u);
    cpu = f.cpu;
    cpu.r[3] = 200u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(f.bus, BASE, before, sizeof(before), &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, invoke(&f));
    SEMU_TEST_EQ_U64(context, 200u, f.cpu.r[3]);
    SEMU_TEST_EQ_U64(context, 3u, f.layer.hits);
    SEMU_TEST_ASSERT(context, memcmp(&cpu, &f.cpu, sizeof(cpu)) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(f.bus, BASE, after, sizeof(after), &f.error));
    SEMU_TEST_ASSERT(context, memcmp(before, after, sizeof(before)) == 0);
    f.cpu.r[3] = 500u;
    unchanged(context, &f, SEMU_ERR_STATE);
    rewind(f.log);
    while (fgets(line, sizeof(line), f.log) != NULL)
        if (strstr(line, "trigger=wbsto-preload1-result ordinal=1") != NULL &&
            strstr(line, "provenance=E-SAP-COMPAT-PRELOAD1-239-001") != NULL)
            ++found;
    SEMU_TEST_EQ_U64(context, 1u, found);
    destroy(&f);
}

static void test_preload1_prerequisites_and_corruption(semu_test_context *context)
{
    fixture f;
    unsigned stage;
    uint32_t offsets[] = { 0x21cu, 4u, 0x2a0u };
    size_t i;
    for (stage = 0u; stage < 2u; ++stage) {
        setup(context, &f, stage);
        unchanged(context, &f, SEMU_ERR_STATE);
        destroy(&f);
    }
    for (i = 0u; i < SEMU_ARRAY_LEN(offsets); ++i) {
        setup(context, &f, 2u);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_write(f.bus, BASE + offsets[i], 1u, 0xffu, &f.error));
        unchanged(context, &f, SEMU_ERR_STATE);
        destroy(&f);
    }
    setup(context, &f, 2u);
    f.layer.enabled = 0;
    unchanged(context, &f, SEMU_ERR_STATE);
    destroy(&f);
}

static void test_preload1_unrelated_callbacks(semu_test_context *context)
{
    fixture f;
    unsigned variant;
    for (variant = 0u; variant < 5u; ++variant) {
        setup(context, &f, 2u);
        if (variant == 0u) f.cpu.r[15] += 2u;
        if (variant == 1u) f.cpu.r[0] += 4u;
        if (variant == 2u)
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_bus_write(f.bus, CLIENT + 12u, 4u, 0x1c3864u, &f.error));
        if (variant == 3u)
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_bus_write(f.bus, CLIENT + 16u, 1u, 2u, &f.error));
        if (variant == 4u) f.cpu.r[3] = 204u;
        unchanged(context, &f, SEMU_OK);
        destroy(&f);
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_preload1_success_and_repeat),
        SEMU_TEST_CASE(test_preload1_prerequisites_and_corruption),
        SEMU_TEST_CASE(test_preload1_unrelated_callbacks)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
