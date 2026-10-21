/* Ticket 710 instance-87 / E-SAP-0127: the 2.33 synthetic touch route.
 * The owner-authorized synthetic enablement values are pinned; the route
 * record layout and the IOM3 probe shape are firmware-derived. The test
 * fails red-first without the layer. */
#include "sapporo_233_touch_route.h"
#include "semu/bus.h"
#include "semu/compat.h"
#include "semu/hash.h"
#include "test.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    semu_sapporo_233_touch_route_context touch;
    semu_layer_state state;
    semu_logger logger;
    FILE *log;
    semu_cpu_state cpu;
    semu_bus *bus;
} fixture;

static int init(fixture *f, semu_error *e)
{
    const char *hashes[3];
    unsigned i;
    memset(f, 0, sizeof(*f));
    f->log = tmpfile();
    f->bus = semu_bus_create(e);
    if (!f->log || !f->bus) return 0;
    if (semu_bus_map_ram(f->bus, "sram", UINT32_C(0x10000000),
            UINT32_C(0x180000), e) != SEMU_OK) {
        return 0;
    }
    semu_log_init(&f->logger, f->log, SEMU_LOG_WARNING);
    for (i = 0u; i < 3u; ++i) {
        hashes[i] = semu_sapporo_233_touch_route_layer.component_hashes[i];
    }
    if (semu_layer_enable_checked(&f->state,
            &semu_sapporo_233_touch_route_layer, "sapporo-2.33.16",
            hashes, 3u, e) != SEMU_OK) {
        return 0;
    }
    f->touch.state = &f->state;
    f->touch.logger = &f->logger;
    return 1;
}

static void destroy(fixture *f)
{
    semu_bus_destroy(f->bus);
    if (f->log) (void)fclose(f->log);
}

static void test_descriptor(semu_test_context *context)
{
    SEMU_TEST_ASSERT(context, strcmp(semu_sapporo_233_touch_route_layer.id,
        "sapporo-2.33-touch-route") == 0);
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_layer.kind ==
        SEMU_LAYER_DEVICE_FIXTURE);
    SEMU_TEST_ASSERT(context, strcmp(
        semu_sapporo_233_touch_route_layer.profile_id,
        "sapporo-2.33.16") == 0);
    SEMU_TEST_ASSERT(context, strcmp(
        semu_sapporo_233_touch_route_layer.evidence, "E-SAP-0127") == 0);
    SEMU_TEST_EQ_U64(context, 3u,
        semu_sapporo_233_touch_route_layer.component_hash_count);
    SEMU_TEST_EQ_U64(context, SEMU_SAPPORO_233_TOUCH_ROUTE_MAX_HITS,
        semu_sapporo_233_touch_route_layer.maximum_hits);
}

static void test_install_and_red_first(semu_test_context *context)
{
    fixture f;
    semu_error e;
    SEMU_TEST_ASSERT(context, init(&f, &e));
    /* Red first: a disabled layer refuses the install. */
    f.state.enabled = 0;
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_install(
        &f.touch, NULL, &e) != SEMU_OK);
    /* Enabled: the synthetic enablement entry is installed. */
    f.state.enabled = 1;
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_install(
        &f.touch, NULL, &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context, f.touch.installed == 1);
    SEMU_TEST_EQ_U64(context, UINT32_C(0x0013dbd0), f.touch.name_id);
    SEMU_TEST_EQ_U64(context, SEMU_SAPPORO_233_TOUCH_ROUTE_TYPE,
        f.touch.route_id);
    SEMU_TEST_EQ_U64(context, 1u, f.touch.route_e);
    SEMU_TEST_EQ_U64(context, 1u, f.touch.touch_e);
    /* Re-install is idempotent after a guest reset. */
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_install(
        &f.touch, NULL, &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, UINT32_C(0x0013dbd0), f.touch.name_id);
    destroy(&f);
}

static void test_apply_seam(semu_test_context *context)
{
    fixture f;
    semu_error e;
    SEMU_TEST_ASSERT(context, init(&f, &e));
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_install(
        &f.touch, NULL, &e) == SEMU_OK);
    /* A non-seam PC is a no-op. */
    f.cpu.r[15] = UINT32_C(0x00100000);
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_apply(
        &f.touch, NULL, &f.cpu, &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 0u, f.touch.hits);
    /* The seam PC with a list logs the entry array view and counts a
     * hit. The derived view sits at record+0x98: data, byte size (a
     * multiple of the 0x30-byte stride), cursor (= data) and count. */
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus, UINT32_C(0x10050098),
        4u, UINT32_C(0x10040000), &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus, UINT32_C(0x1005009c),
        4u, UINT32_C(0x60), &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus, UINT32_C(0x100500a0),
        4u, UINT32_C(0x10040000), &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus, UINT32_C(0x100500a4),
        4u, 2u, &e) == SEMU_OK);
    f.cpu.r[15] = SEMU_SAPPORO_233_TOUCH_ROUTE_PC;
    f.cpu.r[0] = UINT32_C(0x10030000);
    f.cpu.r[1] = UINT32_C(0x10050000);
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_apply(
        &f.touch, f.bus, &f.cpu, &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 1u, f.touch.hits);
    /* A null container refuses (the refusal is sticky). */
    f.cpu.r[1] = 0u;
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_apply(
        &f.touch, NULL, &f.cpu, &e) != SEMU_OK);
    destroy(&f);
}

static void test_entry_array_refusals(semu_test_context *context)
{
    fixture f;
    semu_error e;
    SEMU_TEST_ASSERT(context, init(&f, &e));
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_install(
        &f.touch, NULL, &e) == SEMU_OK);
    /* A byte size that is not a multiple of the 0x30-byte entry stride
     * refuses fail-closed. */
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus, UINT32_C(0x10050098),
        4u, UINT32_C(0x10040000), &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus, UINT32_C(0x1005009c),
        4u, UINT32_C(0x40), &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus, UINT32_C(0x100300a0),
        4u, UINT32_C(0x10040000), &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus, UINT32_C(0x100500a4),
        4u, 0u, &e) == SEMU_OK);
    f.cpu.r[15] = SEMU_SAPPORO_233_TOUCH_ROUTE_PC;
    f.cpu.r[1] = UINT32_C(0x10050000);
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_apply(
        &f.touch, f.bus, &f.cpu, &e) != SEMU_OK);
    destroy(&f);

    /* A cursor that differs from the data pointer refuses. */
    SEMU_TEST_ASSERT(context, init(&f, &e));
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_install(
        &f.touch, NULL, &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus, UINT32_C(0x10050098),
        4u, UINT32_C(0x10040000), &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus, UINT32_C(0x1005009c),
        4u, UINT32_C(0x60), &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus, UINT32_C(0x100500a0),
        4u, UINT32_C(0x10040030), &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus, UINT32_C(0x100500a4),
        4u, 0u, &e) == SEMU_OK);
    f.cpu.r[15] = SEMU_SAPPORO_233_TOUCH_ROUTE_PC;
    f.cpu.r[1] = UINT32_C(0x10050000);
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_apply(
        &f.touch, f.bus, &f.cpu, &e) != SEMU_OK);
    destroy(&f);

    /* A count that overruns the byte size refuses. */
    SEMU_TEST_ASSERT(context, init(&f, &e));
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_install(
        &f.touch, NULL, &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus, UINT32_C(0x10050098),
        4u, UINT32_C(0x10040000), &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus, UINT32_C(0x1005009c),
        4u, UINT32_C(0x60), &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus, UINT32_C(0x100500a0),
        4u, UINT32_C(0x10040000), &e) == SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(f.bus, UINT32_C(0x100500a4),
        4u, 3u, &e) == SEMU_OK);
    f.cpu.r[15] = SEMU_SAPPORO_233_TOUCH_ROUTE_PC;
    f.cpu.r[1] = UINT32_C(0x10050000);
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_apply(
        &f.touch, f.bus, &f.cpu, &e) != SEMU_OK);
    destroy(&f);
}

static void test_sink_event(semu_test_context *context)
{
    fixture f;
    semu_error e;
    uint32_t v = 0u;
    SEMU_TEST_ASSERT(context, init(&f, &e));
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_install(
        &f.touch, NULL, &e) == SEMU_OK);
    /* A non-sink PC is a no-op. */
    f.cpu.r[15] = UINT32_C(0x00100000);
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_apply(
        &f.touch, f.bus, &f.cpu, &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 0u, f.touch.hits);
    /* The sink PC writes the 12-byte record at sp. */
    f.cpu.r[15] = SEMU_SAPPORO_233_TOUCH_ROUTE_SINK_PC;
    f.cpu.r[13] = UINT32_C(0x10030000);
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_apply(
        &f.touch, f.bus, &f.cpu, &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 1u, f.touch.hits);
    SEMU_TEST_ASSERT(context, semu_bus_read(f.bus, UINT32_C(0x10030000),
        2u, &v, &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 120u, v & 0xffffu);
    SEMU_TEST_ASSERT(context, semu_bus_read(f.bus, UINT32_C(0x10030002),
        2u, &v, &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 120u, v & 0xffffu);
    SEMU_TEST_ASSERT(context, semu_bus_read(f.bus, UINT32_C(0x1003000c),
        1u, &v, &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 1u, v & 0xffu);
    /* Hit-bounded: the layer refuses after the budget. */
    f.touch.hits = SEMU_SAPPORO_233_TOUCH_ROUTE_MAX_HITS;
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_apply(
        &f.touch, f.bus, &f.cpu, &e) != SEMU_OK);
    destroy(&f);
}

static void test_queue_event(semu_test_context *context)
{
    fixture f;
    semu_error e;
    uint32_t v = 0u;
    SEMU_TEST_ASSERT(context, init(&f, &e));
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_install(
        &f.touch, NULL, &e) == SEMU_OK);
    f.cpu.r[15] = SEMU_SAPPORO_233_TOUCH_ROUTE_QUEUE_PC;
    f.cpu.r[2] = UINT32_C(0x10030000);
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_apply(
        &f.touch, f.bus, &f.cpu, &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 1u, f.touch.hits);
    SEMU_TEST_ASSERT(context, semu_bus_read(f.bus, UINT32_C(0x10030000),
        2u, &v, &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 120u, v & 0xffffu);
    SEMU_TEST_ASSERT(context, semu_bus_read(f.bus, UINT32_C(0x10030002),
        2u, &v, &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 120u, v & 0xffffu);
    SEMU_TEST_ASSERT(context, semu_bus_read(f.bus, UINT32_C(0x1003000c),
        1u, &v, &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 1u, v & 0xffu);
    destroy(&f);
}

static void test_manager_redirect(semu_test_context *context)
{
    fixture f;
    semu_error e;
    uint32_t v = 0u;
    SEMU_TEST_ASSERT(context, init(&f, &e));
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_install(
        &f.touch, NULL, &e) == SEMU_OK);
    f.cpu.r[15] = SEMU_SAPPORO_233_TOUCH_ROUTE_MANAGER_PC;
    f.cpu.r[0] = UINT32_C(0x10030000);
    f.cpu.r[14] = UINT32_C(0x10040000);
    f.cpu.r[13] = UINT32_C(0x10020000);
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_apply(
        &f.touch, f.bus, &f.cpu, &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, UINT32_C(0x10030000), f.touch.manager);
    SEMU_TEST_EQ_U64(context, 1u, f.touch.pending);
    f.cpu.r[15] = UINT32_C(0x10040000);
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_apply(
        &f.touch, f.bus, &f.cpu, &e) == SEMU_OK);
    /* The constructor's return first starts the manager's activity
     * through its own start handler 0x100b07 (r0 = manager + 8). */
    SEMU_TEST_EQ_U64(context, UINT32_C(0x00100b06), f.cpu.r[15]);
    SEMU_TEST_EQ_U64(context, UINT32_C(0x10030008), f.cpu.r[0]);
    SEMU_TEST_EQ_U64(context, UINT32_C(0x10040000), f.cpu.r[14]);
    SEMU_TEST_EQ_U64(context, 3u, f.touch.pending);
    f.cpu.r[15] = UINT32_C(0x10040000);
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_apply(
        &f.touch, f.bus, &f.cpu, &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, SEMU_SAPPORO_233_TOUCH_ROUTE_EVENT_PC,
        f.cpu.r[15]);
    SEMU_TEST_EQ_U64(context, UINT32_C(0x1003001c), f.cpu.r[0]);
    SEMU_TEST_EQ_U64(context, 120u, f.cpu.r[1]);
    SEMU_TEST_EQ_U64(context, 0u, f.cpu.r[2]);
    SEMU_TEST_EQ_U64(context, UINT32_C(0x1001fffc), f.cpu.r[13]);
    SEMU_TEST_EQ_U64(context, UINT32_C(0x10040000), f.cpu.r[14]);
    SEMU_TEST_ASSERT(context, semu_bus_read(f.bus, UINT32_C(0x1001fffc),
        4u, &v, &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 120u, v);
    SEMU_TEST_EQ_U64(context, 2u, f.touch.pending);
    /* The handler's pop returns to the recorded return PC; the layer
     * then restores the stack and clears the redirect. */
    f.cpu.r[15] = UINT32_C(0x10040000);
    SEMU_TEST_ASSERT(context, semu_sapporo_233_touch_route_apply(
        &f.touch, f.bus, &f.cpu, &e) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, UINT32_C(0x10020000), f.cpu.r[13]);
    SEMU_TEST_EQ_U64(context, 0u, f.touch.pending);
    SEMU_TEST_EQ_U64(context, 2u, f.touch.hits);
    destroy(&f);
}

static void test_endpoint(semu_test_context *context)
{
    fixture f;
    semu_error e;
    semu_serial_endpoint ep;
    semu_serial_transaction txn;
    uint8_t rx[12];
    SEMU_TEST_ASSERT(context, init(&f, &e));
    ep = semu_sapporo_233_touch_route_endpoint(&f.touch);
    memset(&txn, 0, sizeof(txn));
    memset(rx, 0, sizeof(rx));
    txn.address = 0x24u;
    txn.rx = rx;
    txn.rx_size = sizeof(rx);
    SEMU_TEST_ASSERT(context, ep.transfer(ep.context, &txn, &e) ==
        SEMU_TRANSACTION_OK);
    SEMU_TEST_EQ_U64(context, 0x2fu, rx[0]);
    SEMU_TEST_EQ_U64(context, 0x01u, rx[5]);
    SEMU_TEST_EQ_U64(context, 0x01u, rx[8]);
    /* A different address refuses. */
    txn.address = 0x45u;
    SEMU_TEST_ASSERT(context, ep.transfer(ep.context, &txn, &e) ==
        SEMU_TRANSACTION_REFUSE);
    /* A null context refuses. */
    SEMU_TEST_ASSERT(context, ep.transfer(NULL, &txn, &e) ==
        SEMU_TRANSACTION_REFUSE);
    destroy(&f);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_descriptor),
        SEMU_TEST_CASE(test_install_and_red_first),
        SEMU_TEST_CASE(test_apply_seam),
        SEMU_TEST_CASE(test_entry_array_refusals),
        SEMU_TEST_CASE(test_sink_event),
        SEMU_TEST_CASE(test_queue_event),
        SEMU_TEST_CASE(test_manager_redirect),
        SEMU_TEST_CASE(test_endpoint)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
