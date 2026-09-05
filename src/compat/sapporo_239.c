#include "sapporo_239.h"

#include <string.h>

#define STARTUP_CALLBACK UINT32_C(0x00124844)
#define STARTUP_CLIENT UINT32_C(0x10025634)
#define WBSTO_MANAGER_PROVIDER UINT32_C(0x001c0ec8)
#define WBSTO_PRELOAD_PROVIDER UINT32_C(0x001c0ed8)
#define CACHE_CONTEXT UINT32_C(0x1000021c)
#define CACHE_TREE UINT32_C(0x10000004)
#define CACHE_DATA UINT32_C(0x100002a0)
#define ENTRY_SIZE 20u
#define ENTRY_COUNT 4u
#define TREE_SIZE (ENTRY_SIZE * ENTRY_COUNT)
#define DATA_SIZE 40u

static const char *const sapporo_239_hashes[] = {
    "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5",
    "85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89",
    "49a3936f4c9d61dbceee12324f41412334c42aa98b900f6f2d3fc5633ae43aea"
};

static semu_layer_intervention sapporo_239_interventions[] = {
    { "wbsto-session-cache",
      "install four synthetic values in the native WbStorage session cache",
      "E-SAP-COMPAT-WBSTO-239-001", 1u, 0u },
    { "wbsto-preload-result",
      "translate exact WbStoPreload command-zero result from 500 to 200",
      "E-SAP-COMPAT-WBSTO-239-001", 1u, 0u },
    { "logical-file",
      "retain one native public-file operation in session-local memory",
      "E-SAP-COMPAT-FILES-239-001", 2671u, 0u },
    { "wbsto-preload1-result",
      "translate exact WbStoPreload command-one result from 500 to 200",
      "E-SAP-COMPAT-PRELOAD1-239-001", 1u, 0u }
};

const semu_layer_descriptor semu_sapporo_239_wbsto_layer = {
    .id = "sapporo-2.39-synthetic-wbsto",
    .kind = SEMU_LAYER_SYNTHETIC_STATE,
    .profile_id = "sapporo-2.39.20",
    .evidence = "E-SAP-COMPAT-WBSTO-239-001",
    .component_hashes = sapporo_239_hashes,
    .component_hash_count = sizeof(sapporo_239_hashes) /
                            sizeof(sapporo_239_hashes[0]),
    .interventions = sapporo_239_interventions,
    .intervention_count = sizeof(sapporo_239_interventions) /
                          sizeof(sapporo_239_interventions[0]),
    .maximum_hits = 2674u
};

typedef struct cache_record {
    uint16_t local_id;
    uint16_t data_offset;
    const uint8_t *payload;
    uint16_t size;
} cache_record;

static const uint8_t watchface_id[] = "zwwatc01";
static const uint8_t watchface_number[] = "0";
static const uint8_t widgets[] = "{\"arrayData\":[]}";
static const uint8_t daily_theme[] = "yellow";

static const cache_record records[] = {
    { UINT16_C(0xa431), 0u, watchface_id, 8u },
    { UINT16_C(0xa42a), 8u, watchface_number, 1u },
    { UINT16_C(0xa432), 16u, widgets, 16u },
    { UINT16_C(0xa427), 32u, daily_theme, 6u }
};

static uint32_t get_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | (uint32_t)data[1] << 8u |
           (uint32_t)data[2] << 16u | (uint32_t)data[3] << 24u;
}

static void put_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
}

static void put_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static void make_empty_context(uint8_t context[40])
{
    static const uint32_t values[] = {
        UINT32_C(0x00192dc0), 61u, CACHE_TREE, UINT32_C(0x1f4),
        CACHE_TREE, 0u, CACHE_DATA, UINT32_C(0x700), CACHE_DATA, 0u
    };
    size_t i;
    for (i = 0u; i < sizeof(values) / sizeof(values[0]); ++i) {
        put_u32(context + i * 4u, values[i]);
    }
}

static semu_status validate_empty_cache(semu_bus *bus, uint8_t context[40],
                                        uint8_t tree[TREE_SIZE],
                                        uint8_t data[DATA_SIZE],
                                        semu_error *error)
{
    static const uint32_t expected[] = {
        UINT32_C(0x00192dc0), 61u, CACHE_TREE, UINT32_C(0x1f4),
        CACHE_TREE, 0u, CACHE_DATA, UINT32_C(0x700), CACHE_DATA, 0u
    };
    size_t i;

    if (semu_bus_copy_out(bus, CACHE_CONTEXT, context, 40u, error) != SEMU_OK ||
        semu_bus_copy_out(bus, CACHE_TREE, tree, TREE_SIZE, error) != SEMU_OK ||
        semu_bus_copy_out(bus, CACHE_DATA, data, DATA_SIZE, error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_RANGE;
    }
    for (i = 0u; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        if (get_u32(context + i * 4u) != expected[i]) {
            semu_error_set(error, SEMU_ERR_STATE,
                           "Sapporo 2.39 WbStorage cache context mismatch");
            return SEMU_ERR_STATE;
        }
    }
    for (i = 0u; i < TREE_SIZE; ++i) {
        if (tree[i] != 0u) {
            semu_error_set(error, SEMU_ERR_STATE,
                           "Sapporo 2.39 WbStorage cache index is not empty");
            return SEMU_ERR_STATE;
        }
    }
    for (i = 0u; i < DATA_SIZE; ++i) {
        if (data[i] != 0u) {
            semu_error_set(error, SEMU_ERR_STATE,
                           "Sapporo 2.39 WbStorage cache arena is not empty");
            return SEMU_ERR_STATE;
        }
    }
    return SEMU_OK;
}

static void build_cache(uint8_t context[40], uint8_t tree[TREE_SIZE],
                        uint8_t data[DATA_SIZE])
{
    size_t i;
    for (i = 0u; i < ENTRY_COUNT; ++i) {
        uint8_t *entry = tree + i * ENTRY_SIZE;
        put_u32(entry, UINT32_C(0x80000000));
        put_u32(entry + 4u, UINT32_C(0x80000000));
        put_u16(entry + 8u, records[i].local_id);
        put_u16(entry + 10u, records[i].data_offset);
        put_u16(entry + 12u, records[i].size);
        put_u16(entry + 14u, records[i].size);
        put_u32(entry + 16u, (uint32_t)i);
        memcpy(data + records[i].data_offset, records[i].payload,
               records[i].size);
    }
    put_u32(context + 20u, ENTRY_COUNT);
    put_u32(context + 32u, CACHE_DATA + DATA_SIZE);
    put_u32(context + 36u, ENTRY_COUNT);
}

static semu_status validate_installed_cache(semu_bus *bus, semu_error *error)
{
    uint8_t expected_context[40];
    uint8_t expected_tree[TREE_SIZE];
    uint8_t expected_data[DATA_SIZE];
    uint8_t actual_context[40];
    uint8_t actual_tree[TREE_SIZE];
    uint8_t actual_data[DATA_SIZE];

    memset(expected_tree, 0, sizeof(expected_tree));
    memset(expected_data, 0, sizeof(expected_data));
    make_empty_context(expected_context);
    build_cache(expected_context, expected_tree, expected_data);
    if (semu_bus_copy_out(bus, CACHE_CONTEXT, actual_context,
            sizeof(actual_context), error) != SEMU_OK ||
        semu_bus_copy_out(bus, CACHE_TREE, actual_tree,
            sizeof(actual_tree), error) != SEMU_OK ||
        semu_bus_copy_out(bus, CACHE_DATA, actual_data,
            sizeof(actual_data), error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_RANGE;
    }
    if (memcmp(actual_context, expected_context, sizeof(actual_context)) != 0 ||
        memcmp(actual_tree, expected_tree, sizeof(actual_tree)) != 0 ||
        memcmp(actual_data, expected_data, sizeof(actual_data)) != 0) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "Sapporo 2.39 installed WbStorage cache changed");
        return SEMU_ERR_STATE;
    }
    return SEMU_OK;
}

semu_status semu_sapporo_239_apply_wbsto_hook(
    semu_bus *bus, semu_cpu_state *cpu_state, semu_layer_state *state,
    semu_logger *logger, semu_error *error)
{
    uint8_t context[40];
    uint8_t tree[TREE_SIZE];
    uint8_t data[DATA_SIZE];
    uint32_t provider = 0u;
    uint32_t command = 0u;

    if (bus == NULL || cpu_state == NULL || state == NULL || logger == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo 2.39 compatibility arguments are incomplete");
        return SEMU_ERR_ARGUMENT;
    }
    if (cpu_state->r[15] != STARTUP_CALLBACK ||
        cpu_state->r[0] != STARTUP_CLIENT) {
        return SEMU_OK;
    }
    if (semu_bus_read(bus, STARTUP_CLIENT + 12u, 4u, &provider, error) !=
            SEMU_OK ||
        semu_bus_read(bus, STARTUP_CLIENT + 16u, 1u, &command, error) !=
            SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_RANGE;
    }
    if (provider != WBSTO_MANAGER_PROVIDER &&
        provider != WBSTO_PRELOAD_PROVIDER) return SEMU_OK;
    if (state->descriptor == NULL || !state->enabled) {
        semu_error_set(error, SEMU_ERR_STATE, "disabled layer was invoked");
        return SEMU_ERR_STATE;
    }
    if (provider == WBSTO_PRELOAD_PROVIDER) {
        size_t intervention = command == 0u ?
            SEMU_SAPPORO_239_IV_WBSTO_PRELOAD_RESULT :
            SEMU_SAPPORO_239_IV_WBSTO_PRELOAD1_RESULT;
        if (command > 1u ||
            (cpu_state->r[3] & UINT32_C(0xffff)) != 500u) {
            return SEMU_OK;
        }
        if (state->descriptor->interventions[
                SEMU_SAPPORO_239_IV_WBSTO_SESSION_CACHE].hits != 1u) {
            semu_error_set(error, SEMU_ERR_STATE,
                           "Sapporo 2.39 WbStorage cache was not installed");
            return SEMU_ERR_STATE;
        }
        if (command == 1u && state->descriptor->interventions[
                SEMU_SAPPORO_239_IV_WBSTO_PRELOAD_RESULT].hits != 1u) {
            semu_error_set(error, SEMU_ERR_STATE,
                           "Sapporo 2.39 first preload was not translated");
            return SEMU_ERR_STATE;
        }
        if (validate_installed_cache(bus, error) != SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_STATE;
        }
        if (semu_layer_intervention_hit(state, logger,
                intervention, error) != SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_STATE;
        }
        cpu_state->r[3] = 200u;
        return SEMU_OK;
    }
    if (state->descriptor->interventions[
            SEMU_SAPPORO_239_IV_WBSTO_SESSION_CACHE].hits != 0u) {
        return SEMU_OK;
    }
    if (command != 0u || (cpu_state->r[3] & UINT32_C(0xffff)) != 200u) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "Sapporo 2.39 WbStoManager prerequisite mismatch");
        return SEMU_ERR_STATE;
    }
    memset(tree, 0, sizeof(tree));
    memset(data, 0, sizeof(data));
    if (validate_empty_cache(bus, context, tree, data, error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    build_cache(context, tree, data);
    if (semu_layer_intervention_hit(state, logger,
            SEMU_SAPPORO_239_IV_WBSTO_SESSION_CACHE, error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    if (semu_bus_load(bus, CACHE_TREE, tree, sizeof(tree), error) != SEMU_OK ||
        semu_bus_load(bus, CACHE_DATA, data, sizeof(data), error) != SEMU_OK ||
        semu_bus_load(bus, CACHE_CONTEXT, context, sizeof(context), error) !=
            SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_RANGE;
    }
    return SEMU_OK;
}
