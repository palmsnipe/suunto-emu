#include "test.h"

#include <stdio.h>
#include <string.h>

#include "../../src/compat/sapporo_239.h"

#define CLIENT UINT32_C(0x10025634)
#define CONTEXT UINT32_C(0x1000021c)
#define TREE UINT32_C(0x10000004)
#define DATA UINT32_C(0x100002a0)

static const char *const correct_hashes[] = {
    "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5",
    "85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89",
    "49a3936f4c9d61dbceee12324f41412334c42aa98b900f6f2d3fc5633ae43aea"
};

static void write_context(semu_bus *bus, semu_error *error)
{
    static const uint32_t values[] = {
        UINT32_C(0x00192dc0), 61u, TREE, UINT32_C(0x1f4), TREE,
        0u, DATA, UINT32_C(0x700), DATA, 0u
    };
    size_t i;
    for (i = 0u; i < SEMU_ARRAY_LEN(values); ++i) {
        (void)semu_bus_write(bus, CONTEXT + (uint32_t)i * 4u, 4u,
                             values[i], error);
    }
    (void)semu_bus_write(bus, CLIENT + 12u, 4u, UINT32_C(0x001c0ec8), error);
    (void)semu_bus_write(bus, CLIENT + 16u, 1u, 0u, error);
}

static semu_bus *make_bus(semu_test_context *context, semu_error *error)
{
    semu_bus *bus = semu_bus_create(error);
    (void)context;
    if (bus == NULL) return NULL;
    if (semu_bus_map_ram(bus, "sram", UINT32_C(0x10000000),
            UINT32_C(0x00180000), error) != SEMU_OK) {
        semu_bus_destroy(bus);
        return NULL;
    }
    write_context(bus, error);
    return bus;
}

static void set_trigger(semu_cpu_state *cpu, uint32_t status)
{
    memset(cpu, 0, sizeof(*cpu));
    cpu->r[0] = CLIENT;
    cpu->r[3] = status;
    cpu->r[15] = UINT32_C(0x00124844);
}

static void enable_layer(semu_test_context *context, semu_layer_state *state,
                         semu_error *error)
{
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_layer_enable_checked(state, &semu_sapporo_239_wbsto_layer,
            "sapporo-2.39.20", correct_hashes, 3u, error));
}

static uint32_t read_value(semu_test_context *context, semu_bus *bus,
                           uint32_t address, unsigned width,
                           semu_error *error)
{
    uint32_t value = 0u;
    (void)context;
    (void)semu_bus_read(bus, address, width, &value, error);
    return value;
}

static void test_descriptor_and_hash_pins(semu_test_context *context)
{
    const char *wrong_hashes[3] = {
        correct_hashes[0],
        "0000000000000000000000000000000000000000000000000000000000000000",
        correct_hashes[2]
    };
    semu_layer_state state;
    semu_error error;
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
        strcmp(semu_sapporo_239_wbsto_layer.id,
               "sapporo-2.39-synthetic-wbsto") == 0);
    SEMU_TEST_EQ_U64(context, 2674u,
                     semu_sapporo_239_wbsto_layer.maximum_hits);
    SEMU_TEST_EQ_U64(context, 4u, semu_sapporo_239_wbsto_layer.intervention_count);
    SEMU_TEST_EQ_U64(context, 1u, semu_sapporo_239_wbsto_layer.interventions[0].max_hits);
    SEMU_TEST_EQ_U64(context, 1u, semu_sapporo_239_wbsto_layer.interventions[1].max_hits);
    SEMU_TEST_EQ_U64(context, 2671u, semu_sapporo_239_wbsto_layer.interventions[2].max_hits);
    SEMU_TEST_EQ_U64(context, 1u, semu_sapporo_239_wbsto_layer.interventions[3].max_hits);
    enable_layer(context, &state, &error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
        semu_layer_enable_checked(&state, &semu_sapporo_239_wbsto_layer,
            "sapporo-2.39.20", wrong_hashes, 3u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
        semu_layer_enable_checked(&state, &semu_sapporo_239_wbsto_layer,
            "sapporo-2.22.60", correct_hashes, 3u, &error));
}

static void test_exact_native_cache_layout(semu_test_context *context)
{
    static const uint16_t ids[] = {
        UINT16_C(0xa431), UINT16_C(0xa42a),
        UINT16_C(0xa432), UINT16_C(0xa427)
    };
    static const uint16_t offsets[] = { 0u, 8u, 16u, 32u };
    static const uint16_t sizes[] = { 8u, 1u, 16u, 6u };
    static const uint8_t expected_data[40] = {
        'z','w','w','a','t','c','0','1',
        '0',0,0,0,0,0,0,0,
        '{','"','a','r','r','a','y','D','a','t','a','"',':','[',']','}',
        'y','e','l','l','o','w',0,0
    };
    semu_cpu_state cpu;
    semu_layer_state state;
    semu_logger logger;
    semu_error error;
    semu_bus *bus;
    uint8_t data[40];
    FILE *log;
    char line[512];
    int saw_event = 0;
    size_t i;

    semu_error_clear(&error);
    bus = make_bus(context, &error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    enable_layer(context, &state, &error);
    log = tmpfile();
    SEMU_TEST_ASSERT(context, log != NULL);
    semu_log_init(&logger, log, SEMU_LOG_INFO);
    set_trigger(&cpu, 200u);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_apply_wbsto_hook(
            bus, &cpu, &state, &logger, &error));
    SEMU_TEST_EQ_U64(context, 1u, state.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(bus, CLIENT + 12u, 4u, UINT32_C(0x001c0ed8), &error));
    set_trigger(&cpu, 500u);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_apply_wbsto_hook(
            bus, &cpu, &state, &logger, &error));
    SEMU_TEST_EQ_U64(context, 200u, cpu.r[3]);
    SEMU_TEST_EQ_U64(context, 2u, state.hits);
    cpu.r[3] = 500u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_sapporo_239_apply_wbsto_hook(
            bus, &cpu, &state, &logger, &error));
    SEMU_TEST_EQ_U64(context, 500u, cpu.r[3]);
    SEMU_TEST_EQ_U64(context, 4u,
        read_value(context, bus, CONTEXT + 20u, 4u, &error));
    SEMU_TEST_EQ_U64(context, DATA + 40u,
        read_value(context, bus, CONTEXT + 32u, 4u, &error));
    SEMU_TEST_EQ_U64(context, 4u,
        read_value(context, bus, CONTEXT + 36u, 4u, &error));
    for (i = 0u; i < SEMU_ARRAY_LEN(ids); ++i) {
        uint32_t entry = TREE + (uint32_t)i * 20u;
        SEMU_TEST_EQ_U64(context, UINT32_C(0x80000000),
            read_value(context, bus, entry, 4u, &error));
        SEMU_TEST_EQ_U64(context, UINT32_C(0x80000000),
            read_value(context, bus, entry + 4u, 4u, &error));
        SEMU_TEST_EQ_U64(context, ids[i],
            read_value(context, bus, entry + 8u, 2u, &error));
        SEMU_TEST_EQ_U64(context, offsets[i],
            read_value(context, bus, entry + 10u, 2u, &error));
        SEMU_TEST_EQ_U64(context, sizes[i],
            read_value(context, bus, entry + 12u, 2u, &error));
        SEMU_TEST_EQ_U64(context, sizes[i],
            read_value(context, bus, entry + 14u, 2u, &error));
        SEMU_TEST_EQ_U64(context, i,
            read_value(context, bus, entry + 16u, 4u, &error));
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(bus, DATA, data, sizeof(data), &error));
    SEMU_TEST_ASSERT(context, memcmp(data, expected_data, sizeof(data)) == 0);
    rewind(log);
    while (fgets(line, sizeof(line), log) != NULL) {
        if (strstr(line, "trigger=wbsto-session-cache ordinal=1") != NULL) {
            saw_event |= 1;
        }
        if (strstr(line, "trigger=wbsto-preload-result ordinal=1") != NULL) {
            saw_event |= 2;
        }
    }
    SEMU_TEST_EQ_U64(context, 3u, saw_event);
    (void)fclose(log);
    semu_bus_destroy(bus);
}

static void test_wrong_context_refuses_without_mutation(
    semu_test_context *context)
{
    semu_cpu_state cpu;
    semu_layer_state state;
    semu_logger logger;
    semu_error error;
    semu_bus *bus;
    uint8_t before[120], after[120];

    semu_error_clear(&error);
    bus = make_bus(context, &error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    enable_layer(context, &state, &error);
    semu_log_init(&logger, NULL, SEMU_LOG_ERROR);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(bus, CONTEXT + 28u, 4u, UINT32_C(0x701), &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(bus, TREE, before, 80u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(bus, DATA, before + 80u, 40u, &error));
    set_trigger(&cpu, 200u);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_sapporo_239_apply_wbsto_hook(
            bus, &cpu, &state, &logger, &error));
    SEMU_TEST_EQ_U64(context, 0u, state.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(bus, TREE, after, 80u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(bus, DATA, after + 80u, 40u, &error));
    SEMU_TEST_ASSERT(context, memcmp(before, after, sizeof(before)) == 0);
    semu_bus_destroy(bus);
}

static void test_trigger_refusals_and_unrelated_callback(
    semu_test_context *context)
{
    semu_cpu_state cpu;
    semu_layer_state state;
    semu_layer_state disabled;
    semu_logger logger;
    semu_error error;
    semu_bus *bus;

    memset(&disabled, 0, sizeof(disabled));
    semu_error_clear(&error);
    bus = make_bus(context, &error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    enable_layer(context, &state, &error);
    semu_log_init(&logger, NULL, SEMU_LOG_ERROR);
    set_trigger(&cpu, 500u);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_sapporo_239_apply_wbsto_hook(
            bus, &cpu, &state, &logger, &error));
    set_trigger(&cpu, 200u);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_sapporo_239_apply_wbsto_hook(
            bus, &cpu, &disabled, &logger, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(bus, CLIENT + 12u, 4u, UINT32_C(0x001c0ed8), &error));
    set_trigger(&cpu, 500u);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_sapporo_239_apply_wbsto_hook(
            bus, &cpu, &state, &logger, &error));
    SEMU_TEST_EQ_U64(context, 500u, cpu.r[3]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(bus, CLIENT + 12u, 4u, UINT32_C(0x001c3864), &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_apply_wbsto_hook(
            bus, &cpu, &state, &logger, &error));
    SEMU_TEST_EQ_U64(context, 0u, state.hits);
    cpu.r[15] += 2u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_239_apply_wbsto_hook(
            bus, &cpu, &state, &logger, &error));
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_descriptor_and_hash_pins),
        SEMU_TEST_CASE(test_exact_native_cache_layout),
        SEMU_TEST_CASE(test_wrong_context_refuses_without_mutation),
        SEMU_TEST_CASE(test_trigger_refusals_and_unrelated_callback)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
