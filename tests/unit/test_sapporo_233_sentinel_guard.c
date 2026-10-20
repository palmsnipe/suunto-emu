/* Ticket 710 / E-SAP-0092..0107: the 2.33.16 sensor-poll sentinel-insert
 * guard. The witnessed corrupting shape skips the insert (PC -> wrapper
 * continuation); every other shape stays native; a present self-ring, an
 * unexpected return address, a disabled/exhausted layer or a virtual-time
 * rate overrun refuses fail-closed. */
#include "test.h"
#include "sapporo_233_sentinel_guard.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    semu_sapporo_233_sentinel_guard_context ctx;
    semu_layer_state state;
    semu_logger logger;
    FILE *log;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_cpu_state cpu;
} fixture;

static int init(fixture *f, int map_sram, semu_error *e)
{
    const char *hashes[3];
    unsigned i;
    memset(f, 0, sizeof(*f));
    f->log = tmpfile();
    f->bus = semu_bus_create(e);
    f->scheduler = semu_scheduler_create(e);
    if (!f->log || !f->bus || !f->scheduler) return 0;
    semu_log_init(&f->logger, f->log, SEMU_LOG_WARNING);
    if (map_sram &&
        semu_bus_map_ram(f->bus, "sram", UINT32_C(0x10000000),
            UINT32_C(0x180000), e) != SEMU_OK) {
        return 0;
    }
    for (i = 0u; i < 3u; ++i) hashes[i] =
        semu_sapporo_233_sentinel_guard_layer.component_hashes[i];
    if (semu_layer_enable_checked(&f->state,
            &semu_sapporo_233_sentinel_guard_layer, "sapporo-2.33.16",
            hashes, 3u, e) != SEMU_OK) {
        return 0;
    }
    if (semu_sapporo_233_sentinel_guard_install(&f->ctx, &f->state,
            &f->logger, f->scheduler, e) != SEMU_OK) {
        return 0;
    }
    f->cpu.r[15] = SEMU_SAPPORO_233_SENTINEL_INSERT_PC;
    f->cpu.r[1] = SEMU_SAPPORO_233_SENTINEL_NODE;
    f->cpu.r[14] = SEMU_SAPPORO_233_SENTINEL_CONTINUATION | UINT32_C(1);
    return 1;
}

static void destroy(fixture *f)
{
    semu_bus_destroy(f->bus);
    semu_scheduler_destroy(f->scheduler);
    if (f->log != NULL) (void)fclose(f->log);
}

/* The first insert completed: the sensor list's head-anchor next word
 * points at the foreign sentinel, whose own +4 field is not yet self. */
static int linked_not_self(fixture *f, semu_error *e)
{
    return semu_bus_write(f->bus, SEMU_SAPPORO_233_SENSOR_LIST_HEAD_NEXT,
               4u, SEMU_SAPPORO_233_SENTINEL_NODE, e) == SEMU_OK &&
        semu_bus_write(f->bus, SEMU_SAPPORO_233_SENTINEL_NEXT, 4u,
               UINT32_C(0x10029c4c), e) == SEMU_OK;
}

static void test_descriptor_and_enable(semu_test_context *context)
{
    const semu_layer_descriptor *d = &semu_sapporo_233_sentinel_guard_layer;
    const char *hashes[3];
    const char *wrong[3];
    semu_layer_state state;
    semu_error e;
    unsigned i;
    SEMU_TEST_ASSERT(context, strcmp(d->id,
        "sapporo-2.33-sentinel-guard") == 0);
    SEMU_TEST_ASSERT(context, strcmp(d->profile_id,
        "sapporo-2.33.16") == 0);
    SEMU_TEST_ASSERT(context, strcmp(d->evidence,
        "E-SAP-0092..E-SAP-0107") == 0);
    SEMU_TEST_EQ_U64(context, SEMU_LAYER_DEVICE_FIXTURE, d->kind);
    SEMU_TEST_EQ_U64(context, 3u, d->component_hash_count);
    SEMU_TEST_EQ_U64(context, SEMU_SAPPORO_233_SENTINEL_MAX_HITS,
        d->maximum_hits);
    for (i = 0u; i < 3u; ++i) hashes[i] = d->component_hashes[i];
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_layer_enable_checked(&state, d,
        "sapporo-2.33.16", hashes, 3u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT, semu_layer_enable_checked(
        &state, d, "sapporo-2.35.34", hashes, 3u, &e));
    for (i = 0u; i < 3u; ++i) wrong[i] = hashes[i];
    wrong[1] = "ba286a7bce55dd2f7aba81436d3c5152401c28d86bef89b2c9dbbacd04b7e9e6";
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT, semu_layer_enable_checked(
        &state, d, "sapporo-2.33.16", wrong, 3u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, semu_layer_enable_checked(
        &state, d, "sapporo-2.33.16", hashes, 2u, &e));
}

static void test_skip_and_native(semu_test_context *context)
{
    fixture f;
    semu_error e;
    char text[512];
    size_t count;
    SEMU_TEST_ASSERT(context, init(&f, 1, &e));
    SEMU_TEST_ASSERT(context, linked_not_self(&f, &e));
    SEMU_TEST_ASSERT(context, semu_scheduler_advance(f.scheduler,
        UINT64_C(40000000000), &e) == SEMU_OK);
    /* The witnessed corrupting shape: skip the insert. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_sentinel_guard_apply(
        &f.ctx, f.bus, &f.cpu, &e));
    SEMU_TEST_EQ_U64(context, SEMU_SAPPORO_233_SENTINEL_CONTINUATION,
        f.cpu.r[15]);
    SEMU_TEST_EQ_U64(context, 1u, f.state.hits);
    rewind(f.log);
    count = fread(text, 1u, sizeof(text) - 1u, f.log);
    text[count] = '\0';
    SEMU_TEST_ASSERT(context, strstr(text, "layer-hit") != NULL);
    SEMU_TEST_ASSERT(context, strstr(text, "sapporo-2.33-sentinel-guard")
        != NULL);
    /* Every other shape stays native: PC, node argument, and the
     * not-yet-linked head-anchor state. */
    f.cpu.r[15] = UINT32_C(0x000bb6b0);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_sentinel_guard_apply(
        &f.ctx, f.bus, &f.cpu, &e));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x000bb6b0), f.cpu.r[15]);
    f.cpu.r[15] = SEMU_SAPPORO_233_SENTINEL_INSERT_PC;
    f.cpu.r[1] = UINT32_C(0x100331ac);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_sentinel_guard_apply(
        &f.ctx, f.bus, &f.cpu, &e));
    SEMU_TEST_EQ_U64(context, SEMU_SAPPORO_233_SENTINEL_INSERT_PC,
        f.cpu.r[15]);
    f.cpu.r[1] = SEMU_SAPPORO_233_SENTINEL_NODE;
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus,
        SEMU_SAPPORO_233_SENSOR_LIST_HEAD_NEXT, 4u,
        UINT32_C(0x10029c4c), &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_sentinel_guard_apply(
        &f.ctx, f.bus, &f.cpu, &e));
    SEMU_TEST_EQ_U64(context, SEMU_SAPPORO_233_SENTINEL_INSERT_PC,
        f.cpu.r[15]);
    SEMU_TEST_EQ_U64(context, 1u, f.state.hits);
    destroy(&f);
}

static void test_refusals(semu_test_context *context)
{
    fixture f;
    semu_error e;
    SEMU_TEST_ASSERT(context, init(&f, 1, &e));
    /* A self-ring already present: the guest is past the witnessed
     * pre-corruption shape. The refusal is sticky. */
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus,
        SEMU_SAPPORO_233_SENSOR_LIST_HEAD_NEXT, 4u,
        SEMU_SAPPORO_233_SENTINEL_NODE, &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus,
        SEMU_SAPPORO_233_SENTINEL_NEXT, 4u,
        SEMU_SAPPORO_233_SENTINEL_NODE, &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_scheduler_advance(f.scheduler,
        UINT64_C(40000000000), &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_sapporo_233_sentinel_guard_apply(&f.ctx, f.bus, &f.cpu, &e));
    SEMU_TEST_ASSERT(context, strstr(e.text, "self-ring") != NULL);
    SEMU_TEST_ASSERT(context, linked_not_self(&f, &e));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_sapporo_233_sentinel_guard_apply(&f.ctx, f.bus, &f.cpu, &e));
    destroy(&f);

    /* An unexpected return address refuses (sticky). */
    SEMU_TEST_ASSERT(context, init(&f, 1, &e));
    SEMU_TEST_ASSERT(context, linked_not_self(&f, &e));
    SEMU_TEST_ASSERT(context, semu_scheduler_advance(f.scheduler,
        UINT64_C(40000000000), &e) == SEMU_OK);
    f.cpu.r[14] = UINT32_C(0x000925db);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_sapporo_233_sentinel_guard_apply(&f.ctx, f.bus, &f.cpu, &e));
    SEMU_TEST_ASSERT(context, strstr(e.text, "return address") != NULL);
    destroy(&f);

    /* The rate cap: at virtual time zero no skip is admitted (the layer
     * has no time source evidence to bound it), and a counter at the
     * rate ceiling refuses. */
    SEMU_TEST_ASSERT(context, init(&f, 1, &e));
    SEMU_TEST_ASSERT(context, linked_not_self(&f, &e));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_sapporo_233_sentinel_guard_apply(&f.ctx, f.bus, &f.cpu, &e));
    SEMU_TEST_ASSERT(context, strstr(e.text, "rate") != NULL);
    destroy(&f);

    SEMU_TEST_ASSERT(context, init(&f, 1, &e));
    SEMU_TEST_ASSERT(context, linked_not_self(&f, &e));
    SEMU_TEST_ASSERT(context, semu_scheduler_advance(f.scheduler,
        UINT64_C(40000000000), &e) == SEMU_OK);
    f.state.hits = UINT64_C(120000000); /* 3M/s x 40 s */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_sapporo_233_sentinel_guard_apply(&f.ctx, f.bus, &f.cpu, &e));
    SEMU_TEST_ASSERT(context, strstr(e.text, "rate") != NULL);
    destroy(&f);

    /* A disabled layer refuses; a bus read failure fails closed. */
    SEMU_TEST_ASSERT(context, init(&f, 1, &e));
    SEMU_TEST_ASSERT(context, linked_not_self(&f, &e));
    SEMU_TEST_ASSERT(context, semu_scheduler_advance(f.scheduler,
        UINT64_C(40000000000), &e) == SEMU_OK);
    f.state.enabled = 0;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_sapporo_233_sentinel_guard_apply(&f.ctx, f.bus, &f.cpu, &e));
    destroy(&f);

    SEMU_TEST_ASSERT(context, init(&f, 0, &e));
    SEMU_TEST_ASSERT(context, semu_scheduler_advance(f.scheduler,
        UINT64_C(40000000000), &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context,
        semu_sapporo_233_sentinel_guard_apply(&f.ctx, f.bus, &f.cpu, &e)
            != SEMU_OK);
    destroy(&f);
}

static void test_log_milestones_and_install(semu_test_context *context)
{
    fixture f;
    semu_error e;
    char text[1024];
    size_t count;
    SEMU_TEST_ASSERT(context, init(&f, 1, &e));
    SEMU_TEST_ASSERT(context, linked_not_self(&f, &e));
    SEMU_TEST_ASSERT(context, semu_scheduler_advance(f.scheduler,
        UINT64_C(40000000000), &e) == SEMU_OK);
    f.state.hits = SEMU_SAPPORO_233_SENTINEL_LOG_STRIDE - 1u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_sentinel_guard_apply(
        &f.ctx, f.bus, &f.cpu, &e));
    SEMU_TEST_EQ_U64(context, SEMU_SAPPORO_233_SENTINEL_LOG_STRIDE,
        f.state.hits);
    rewind(f.log);
    count = fread(text, 1u, sizeof(text) - 1u, f.log);
    text[count] = '\0';
    SEMU_TEST_ASSERT(context, strstr(text, "hit=1000000") != NULL);
    /* A mismatched descriptor or scheduler refuses the install. */
    f.ctx.state = NULL;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        semu_sapporo_233_sentinel_guard_install(&f.ctx, &f.state,
            &f.logger, NULL, &e));
    destroy(&f);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_descriptor_and_enable),
        SEMU_TEST_CASE(test_skip_and_native),
        SEMU_TEST_CASE(test_refusals),
        SEMU_TEST_CASE(test_log_milestones_and_install)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
