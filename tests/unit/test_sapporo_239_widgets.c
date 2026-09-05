#include "test.h"
#include "../../src/compat/sapporo_239.h"
#include <stdio.h>
#include <string.h>

#define BASE UINT32_C(0x10000000)
#define CLIENT (BASE + UINT32_C(0x25634))
#define CONTEXT (BASE + UINT32_C(0x21c))
#define WIDGETS (BASE + UINT32_C(0x2b0))

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

static void setup(semu_test_context *context, fixture *f, unsigned command)
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
            semu_bus_write(f->bus, CONTEXT + (uint32_t)i * 4u, 4u,
                           words[i], &f->error));
    f->cpu.r[0] = CLIENT;
    f->cpu.r[3] = 200u;
    f->cpu.r[15] = 0x124844u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(f->bus, CLIENT + 12u, 4u, 0x1c0ec8u, &f->error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, invoke(f));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(f->bus, CLIENT + 12u, 4u, 0x1c0ed8u, &f->error));
    f->cpu.r[3] = 500u;
    if (command == 1u) SEMU_TEST_EQ_U64(context, SEMU_OK, invoke(f));
    f->cpu.r[3] = 500u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(f->bus, CLIENT + 16u, 1u, command, &f->error));
}

static void destroy(fixture *f)
{
    semu_bus_destroy(f->bus);
    (void)fclose(f->log);
}

static void refuses_unchanged(semu_test_context *context, fixture *f)
{
    uint8_t before[0x300], after[0x300];
    semu_cpu_state cpu = f->cpu;
    uint64_t hits = f->layer.hits;
    uint64_t counters[SEMU_SAPPORO_239_IV_COUNT];
    long log_size = ftell(f->log);
    size_t i;
    for (i = 0u; i < SEMU_ARRAY_LEN(counters); ++i)
        counters[i] = f->layer.descriptor->interventions[i].hits;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(f->bus, BASE, before, sizeof(before), &f->error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, invoke(f));
    SEMU_TEST_ASSERT(context, memcmp(&cpu, &f->cpu, sizeof(cpu)) == 0);
    SEMU_TEST_EQ_U64(context, hits, f->layer.hits);
    SEMU_TEST_ASSERT(context, ftell(f->log) == log_size);
    for (i = 0u; i < SEMU_ARRAY_LEN(counters); ++i)
        SEMU_TEST_EQ_U64(context, counters[i],
            f->layer.descriptor->interventions[i].hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(f->bus, BASE, after, sizeof(after), &f->error));
    SEMU_TEST_ASSERT(context, memcmp(before, after, sizeof(before)) == 0);
}

static void test_widgets_corruption_and_legacy_json(semu_test_context *context)
{
    static const uint8_t legacy[] = "{\"arrayData\":[]}";
    unsigned command, variant;
    for (command = 0u; command <= 1u; ++command) {
        for (variant = 0u; variant < 19u; ++variant) {
            fixture f;
            setup(context, &f, command);
            if (variant < 16u) {
                SEMU_TEST_EQ_U64(context, SEMU_OK,
                    semu_bus_write(f.bus, WIDGETS + variant, 1u, 1u, &f.error));
            } else if (variant < 18u) {
                SEMU_TEST_EQ_U64(context, SEMU_OK,
                    semu_bus_write(f.bus, BASE + 0x38u +
                        (variant - 16u) * 2u, 2u, 16u, &f.error));
            } else {
                SEMU_TEST_EQ_U64(context, SEMU_OK,
                    semu_bus_load(f.bus, WIDGETS, legacy, sizeof(legacy) - 1u,
                                   &f.error));
                SEMU_TEST_EQ_U64(context, SEMU_OK,
                    semu_bus_write(f.bus, BASE + 0x38u, 4u, 0x00100010u,
                                    &f.error));
            }
            refuses_unchanged(context, &f);
            destroy(&f);
        }
    }
}

static void test_widgets_preloads_and_provenance(semu_test_context *context)
{
    unsigned command;
    for (command = 0u; command <= 1u; ++command) {
        fixture f;
        uint8_t before[0x300], after[0x300];
        char line[512];
        unsigned found = 0u;
        setup(context, &f, command);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_copy_out(f.bus, BASE, before, sizeof(before), &f.error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, invoke(&f));
        SEMU_TEST_EQ_U64(context, 200u, f.cpu.r[3]);
        SEMU_TEST_EQ_U64(context, 2u + command, f.layer.hits);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_copy_out(f.bus, BASE, after, sizeof(after), &f.error));
        SEMU_TEST_ASSERT(context, memcmp(before, after, sizeof(before)) == 0);
        rewind(f.log);
        while (fgets(line, sizeof(line), f.log) != NULL)
            if (strstr(line, "trigger=wbsto-session-cache ordinal=1") &&
                strstr(line, "provenance=E-SAP-COMPAT-WIDGETS-NATIVE-239-001"))
                ++found;
        SEMU_TEST_EQ_U64(context, 1u, found);
        destroy(&f);
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_widgets_corruption_and_legacy_json),
        SEMU_TEST_CASE(test_widgets_preloads_and_provenance)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
