#include "sapporo_233_sentinel_guard.h"

#include <stdio.h>

static const char *const hashes[] = {
    "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5",
    "ba286a7bce55dd2f7aba81436d3c5152401c28d86bef89b2c9dbbacd04b7e9e5",
    "b06492a982faa188a14d0a458e7165f9e0b13d0917ded3c3e37077f6ef68d5c5"
};

const semu_layer_descriptor semu_sapporo_233_sentinel_guard_layer = {
    .id = "sapporo-2.33-sentinel-guard",
    .kind = SEMU_LAYER_DEVICE_FIXTURE,
    .profile_id = "sapporo-2.33.16",
    .evidence = "E-SAP-0092..E-SAP-0107",
    .component_hashes = hashes,
    .component_hash_count = 3u,
    .maximum_hits = SEMU_SAPPORO_233_SENTINEL_MAX_HITS
};

static int ready(const semu_sapporo_233_sentinel_guard_context *c)
{
    return c != NULL && c->state != NULL && c->logger != NULL &&
        c->scheduler != NULL && c->state->enabled &&
        c->state->descriptor == &semu_sapporo_233_sentinel_guard_layer &&
        c->refusal.code == SEMU_OK &&
        c->state->hits < SEMU_SAPPORO_233_SENTINEL_MAX_HITS;
}

static semu_status refuse(semu_sapporo_233_sentinel_guard_context *c,
                          semu_error *error, const char *reason)
{
    semu_error local;
    semu_error_set(&local, SEMU_ERR_STATE,
        "2.33 sentinel guard refused: %s", reason);
    if (c != NULL && c->state != NULL && c->state->enabled &&
        c->refusal.code == SEMU_OK) {
        c->refusal = local;
    }
    if (error != NULL) *error = local;
    return local.code;
}

/* E-SAP-0117 budget: at most 3,000,000 skips per virtual second since the
 * last machine reset (the layer hit counter resets on the same reset), with
 * an absolute ceiling of rate x the supported 120 s census window. */
static uint64_t rate_allowed(uint64_t virtual_time_ns)
{
    uint64_t seconds = virtual_time_ns / UINT64_C(1000000000);
    uint64_t remainder = virtual_time_ns % UINT64_C(1000000000);
    uint64_t allowed =
        (uint64_t)SEMU_SAPPORO_233_SENTINEL_RATE_PER_SECOND * seconds +
        ((uint64_t)SEMU_SAPPORO_233_SENTINEL_RATE_PER_SECOND * remainder) /
            UINT64_C(1000000000);
    if (allowed > SEMU_SAPPORO_233_SENTINEL_MAX_HITS) {
        allowed = SEMU_SAPPORO_233_SENTINEL_MAX_HITS;
    }
    return allowed;
}

semu_status semu_sapporo_233_sentinel_guard_install(
    semu_sapporo_233_sentinel_guard_context *c, semu_layer_state *state,
    semu_logger *logger, semu_scheduler *scheduler, semu_error *error)
{
    if (c == NULL || state == NULL || logger == NULL || scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "2.33 sentinel guard arguments are incomplete");
        return SEMU_ERR_ARGUMENT;
    }
    if (!state->enabled ||
        state->descriptor != &semu_sapporo_233_sentinel_guard_layer) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "2.33 sentinel guard layer is not the enabled descriptor");
        return SEMU_ERR_STATE;
    }
    c->state = state;
    c->logger = logger;
    c->scheduler = scheduler;
    semu_error_clear(&c->refusal);
    return SEMU_OK;
}

semu_status semu_sapporo_233_sentinel_guard_apply(
    semu_sapporo_233_sentinel_guard_context *c, semu_bus *bus,
    semu_cpu_state *cpu, semu_error *error)
{
    uint32_t head_next, sentinel_next;
    uint64_t allowed, now_ns;
    char effect[160];

    if (c == NULL || bus == NULL || cpu == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "2.33 sentinel guard hook arguments are incomplete");
        return SEMU_ERR_ARGUMENT;
    }
    if (cpu->r[15] != SEMU_SAPPORO_233_SENTINEL_INSERT_PC) return SEMU_OK;
    if (cpu->r[1] != SEMU_SAPPORO_233_SENTINEL_NODE) return SEMU_OK;
    if (!ready(c)) {
        return refuse(c, error, "disabled, exhausted or unexpected state");
    }
    if (semu_bus_read(bus, SEMU_SAPPORO_233_SENSOR_LIST_HEAD_NEXT, 4u,
                      &head_next, error) != SEMU_OK) {
        return error->code;
    }
    if (head_next != SEMU_SAPPORO_233_SENTINEL_NODE) {
        /* The first insert has not completed: this is the native path. */
        return SEMU_OK;
    }
    if (semu_bus_read(bus, SEMU_SAPPORO_233_SENTINEL_NEXT, 4u,
                      &sentinel_next, error) != SEMU_OK) {
        return error->code;
    }
    if (sentinel_next == SEMU_SAPPORO_233_SENTINEL_NODE) {
        return refuse(c, error, "self-ring already present");
    }
    if ((cpu->r[14] & ~UINT32_C(1)) !=
        SEMU_SAPPORO_233_SENTINEL_CONTINUATION) {
        return refuse(c, error, "unexpected return address");
    }
    now_ns = semu_scheduler_now(c->scheduler);
    allowed = rate_allowed(now_ns);
    if (c->state->hits >= allowed) {
        return refuse(c, error, "virtual-time rate budget exhausted");
    }
    c->state->hits += 1u;
    if (c->state->hits == 1u ||
        c->state->hits % SEMU_SAPPORO_233_SENTINEL_LOG_STRIDE == 0u) {
        (void)snprintf(effect, sizeof(effect),
            "trigger=sentinel-insert ordinal=%llu skip pc=0x000a4e8a "
            "virtual_time_ns=%llu",
            (unsigned long long)c->state->hits,
            (unsigned long long)now_ns);
        semu_log_write(c->logger, SEMU_LOG_WARNING, "compat", "layer-hit",
            "layer=%s hit=%llu maximum=%llu effect=%s evidence=%s",
            c->state->descriptor->id,
            (unsigned long long)c->state->hits,
            (unsigned long long)c->state->descriptor->maximum_hits,
            effect, c->state->descriptor->evidence);
    }
    cpu->r[15] = cpu->r[14] & ~UINT32_C(1);
    return SEMU_OK;
}
