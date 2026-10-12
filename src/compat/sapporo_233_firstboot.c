#include "sapporo_233_firstboot.h"

#include <stdio.h>
#include <string.h>

static const char *const hashes[] = {
    "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5",
    "ba286a7bce55dd2f7aba81436d3c5152401c28d86bef89b2c9dbbacd04b7e9e5",
    "b06492a982faa188a14d0a458e7165f9e0b13d0917ded3c3e37077f6ef68d5c5"
};

/* E-SAP-0068 v7 window: six translations in the 8 s staged boot, each once.
 * (WbStoPreload, 0) is the E-SAP-0066 v4 form of the same shared checker. */
typedef struct {
    const char *name;
    uint8_t command;
} firstboot_rule;

static const firstboot_rule rules[] = {
    { "WbStoPreload", 0u },
    { "sleepln", 0u },
    { "TrainingTss", 0u },
    { "PoiP", 1u },
    { "sleepln", 1u },
    { "actitmln", 1u },
    { "TrainingTss", 1u }
};

#define RULE_COUNT (sizeof(rules) / sizeof(rules[0]))

/* The layer budget is the rule count: each rule fires at most once and
 * semu_layer_hit's aggregate budget enforces the total. */
const semu_layer_descriptor semu_sapporo_233_firstboot_layer = {
    .id = "sapporo-2.33-firstboot",
    .kind = SEMU_LAYER_DEVICE_FIXTURE,
    .profile_id = "sapporo-2.33.16",
    .evidence = "E-SAP-0068",
    .component_hashes = hashes,
    .component_hash_count = 3u,
    .maximum_hits = RULE_COUNT
};

static int ready(const semu_sapporo_233_firstboot_context *c)
{
    return c != NULL && c->state != NULL && c->logger != NULL &&
        c->state->enabled &&
        c->state->descriptor == &semu_sapporo_233_firstboot_layer &&
        c->refusal.code == SEMU_OK && c->state->hits < RULE_COUNT;
}

static semu_status refuse(semu_sapporo_233_firstboot_context *c,
                          semu_error *error)
{
    semu_error local;
    semu_error_set(&local, SEMU_ERR_STATE,
        "2.33 firstboot fixture disabled, exhausted or unexpected state");
    if (c != NULL && c->state != NULL && c->state->enabled &&
        c->refusal.code == SEMU_OK) {
        c->refusal = local;
    }
    if (error != NULL) *error = local;
    return local.code;
}

semu_status semu_sapporo_233_firstboot_apply(
    semu_sapporo_233_firstboot_context *c, semu_bus *bus,
    semu_cpu_state *cpu, semu_error *error)
{
    char name[25];
    uint32_t name_ptr, value_tmp;
    uint8_t command;
    size_t i, k;
    char effect[96];
    semu_error failure;
    if (c == NULL || bus == NULL || cpu == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "2.33 firstboot hook arguments are incomplete");
        return SEMU_ERR_ARGUMENT;
    }
    if (cpu->r[15] != SEMU_SAPPORO_233_FIRSTBOOT_PC) return SEMU_OK;
    if ((cpu->r[3] & UINT32_C(0xffff)) != 500u) return SEMU_OK;
    /* The lane layer's client guard: one shared SRAM object inside the
     * staged window's span; anything else stays native. */
    if (cpu->r[0] < UINT32_C(0x10000000) ||
        cpu->r[0] > UINT32_C(0x10060000) - 20u) {
        return SEMU_OK;
    }
    if (!ready(c)) return refuse(c, error);
    /* Name pointer at +12, command byte at +16 (E-SAP-0066/0068). */
    if (semu_bus_copy_out(bus, cpu->r[0] + 12u, (uint8_t *)&name_ptr,
                          sizeof(name_ptr), error) != SEMU_OK) {
        return error->code;
    }
    if (name_ptr < UINT32_C(0x00040000) ||
        name_ptr > UINT32_C(0x001c0000) - 25u) {
        return SEMU_OK;
    }
    for (k = 0u; k < 24u; ++k) {
        if (semu_bus_read(bus, name_ptr + (uint32_t)k, 1u, &value_tmp,
                          error) != SEMU_OK) return error->code;
        if (value_tmp == 0u) break;
        name[k] = (value_tmp >= 32u && value_tmp < 127u) ? (char)value_tmp
                                                         : '?';
    }
    name[k] = '\0';
    if (k == 0u) return SEMU_OK;
    if (semu_bus_read(bus, cpu->r[0] + 16u, 1u, &value_tmp, error) !=
        SEMU_OK) {
        return error->code;
    }
    command = (uint8_t)value_tmp;
    for (i = 0u; i < RULE_COUNT; ++i) {
        if (rules[i].command == command &&
            strcmp(rules[i].name, name) == 0) {
            (void)snprintf(effect, sizeof(effect),
                "provider=%s command=%u native_status=500 translated_status=200",
                name, (unsigned)command);
            if (semu_layer_hit(c->state, c->logger, effect, &failure) !=
                SEMU_OK) {
                if (error != NULL) *error = failure;
                return failure.code;
            }
            cpu->r[3] = 200u;
            return SEMU_OK;
        }
    }
    return SEMU_OK;
}
