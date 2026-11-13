#include "test.h"

#include <stdint.h>

#include "../../src/core/scheduler_internal.h"
#include "../../src/soc/apollo4/stimer.h"

#define STIMER_TICK_NS UINT64_C(30518)

typedef struct irq_record {
    unsigned count;
    semu_apollo4_stimer_irq irq[8];
    int level[8];
    uint64_t time[8];
} irq_record;

typedef struct wismar_stimer_fixture {
    semu_error error;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4_stimer *stimer;
    irq_record record;
} wismar_stimer_fixture;

static void irq_callback(void *context, unsigned irq, int level)
{
    wismar_stimer_fixture *fixture = (wismar_stimer_fixture *)context;
    unsigned index = fixture->record.count;
    if (index < 8u) {
        fixture->record.irq[index] = irq;
        fixture->record.level[index] = level;
        fixture->record.time[index] = semu_scheduler_now(fixture->scheduler);
    }
    ++fixture->record.count;
}

static int fixture_init(wismar_stimer_fixture *fixture, int plain_config,
                        int tick_release)
{
    semu_error_clear(&fixture->error);
    fixture->bus = semu_bus_create(&fixture->error);
    fixture->scheduler = semu_scheduler_create(&fixture->error);
    if (fixture->bus == NULL || fixture->scheduler == NULL) return 0;
    fixture->stimer = semu_apollo4_stimer_create(
        fixture->bus, fixture->scheduler, irq_callback, fixture,
        &fixture->error);
    if (fixture->stimer == NULL) return 0;
    /* Mirror the apollo4.c profile wiring: the plain-config law rides
     * every Wismar machine and the tick law rides it; 0/0 is the shared
     * (Sapporo) profile with no Wismar flags. */
    if (plain_config)
        semu_apollo4_stimer_enable_wismar_plain_config(fixture->stimer);
    if (tick_release)
        semu_apollo4_stimer_enable_wismar_tick_release(fixture->stimer);
    return 1;
}

static void fixture_destroy(wismar_stimer_fixture *fixture)
{
    semu_apollo4_stimer_destroy(fixture->stimer);
    semu_scheduler_destroy(fixture->scheduler);
    semu_bus_destroy(fixture->bus);
}

static semu_status read_register(wismar_stimer_fixture *fixture,
                                 uint32_t offset, uint32_t *value)
{
    return semu_bus_read(fixture->bus, SEMU_APOLLO4_STIMER_BASE + offset,
                         4u, value, &fixture->error);
}

static semu_status write_register(wismar_stimer_fixture *fixture,
                                  uint32_t offset, uint32_t value)
{
    return semu_bus_write(fixture->bus, SEMU_APOLLO4_STIMER_BASE + offset,
                          4u, value, &fixture->error);
}

/* E-WIS-0009 (E-WIS-0006 law): CMPR0 (+0x20, IRQ 32) and CMPR1 (+0x24,
 * IRQ 33) arm off the shared relative compare law; INTEN bits {0,1}
 * enable the engine; the pending bits live in INTSTAT bits {0,1} and
 * INTCLR bits {0,1} deassert the pulse lines. */
static void test_wismar_compare_b_arms_and_fires(semu_test_context *context)
{
    wismar_stimer_fixture fixture = { 0 };
    uint32_t value = 0u;
    SEMU_TEST_ASSERT(context, fixture_init(&fixture, 1, 1));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x00u, 0x303u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x100u, 0x3u));
    /* The firmware pre-subtracts the three-tick commit latency from
     * each arm (E-WIS-0009 arm law, per-arm fresh counter reads), so a
     * pair of deltas 32/33 lands as raw writes 29/30 and absolute
     * deadlines 32/36. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x20u, 29u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x24u, 30u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x20u, &value));
    SEMU_TEST_EQ_U64(context, 32u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x24u, &value));
    SEMU_TEST_EQ_U64(context, 36u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler,
                             29u * STIMER_TICK_NS, &fixture.error));
    SEMU_TEST_EQ_U64(context, 1u, fixture.record.count);
    SEMU_TEST_EQ_U64(context, SEMU_APOLLO4_STIMER_IRQ_A,
                     fixture.record.irq[0]);
    SEMU_TEST_EQ_U64(context, 1u, fixture.record.level[0]);
    SEMU_TEST_EQ_U64(context, UINT64_C(885010), fixture.record.time[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler, STIMER_TICK_NS,
                                            &fixture.error));
    SEMU_TEST_EQ_U64(context, 2u, fixture.record.count);
    SEMU_TEST_EQ_U64(context, SEMU_APOLLO4_STIMER_IRQ_B,
                     fixture.record.irq[1]);
    SEMU_TEST_EQ_U64(context, 1u, fixture.record.level[1]);
    SEMU_TEST_EQ_U64(context, UINT64_C(915528), fixture.record.time[1]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x104u, &value));
    SEMU_TEST_EQ_U64(context, 3u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x108u, 1u));
    SEMU_TEST_EQ_U64(context, 3u, fixture.record.count);
    SEMU_TEST_EQ_U64(context, 0u, fixture.record.level[2]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x104u, &value));
    SEMU_TEST_EQ_U64(context, 2u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x108u, 2u));
    SEMU_TEST_EQ_U64(context, 4u, fixture.record.count);
    SEMU_TEST_EQ_U64(context, 0u, fixture.record.level[3]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x104u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    fixture_destroy(&fixture);
}

/* E-WIS-0009: unevidenced INTEN/INTCLR bits drop without state change,
 * +0x28 drops under the plain-config law, and the shared (non-Wismar)
 * profile keeps refusing +0x24 outright. */
static void test_wismar_tick_masks_and_refusals(semu_test_context *context)
{
    wismar_stimer_fixture fixture = { 0 };
    wismar_stimer_fixture shared = { 0 };
    uint32_t value = 0u;
    SEMU_TEST_ASSERT(context, fixture_init(&fixture, 1, 1));
    SEMU_TEST_ASSERT(context, fixture_init(&shared, 0, 0));
    /* INTEN bit 8 (the shared 0x100 enable) is not evidenced for the
     * Wismar law: the store masks to bits {0,1}. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x100u, 0x200u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x100u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x100u, 0x103u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x100u, &value));
    SEMU_TEST_EQ_U64(context, 3u, value);
    /* CMPR1 arms and holds its deadline across a CMPR_C (+0x28) drop. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x24u, 33u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x28u, 96u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x28u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x24u, &value));
    SEMU_TEST_EQ_U64(context, 36u, value);
    /* Pending stays untouched by unevidenced INTCLR bits. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x00u, 0x303u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x24u, 33u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler,
                             34u * STIMER_TICK_NS, &fixture.error));
    SEMU_TEST_EQ_U64(context, 1u, fixture.record.count);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x104u, &value));
    SEMU_TEST_EQ_U64(context, 2u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x108u, 0x100u));
    SEMU_TEST_EQ_U64(context, 1u, fixture.record.count);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x104u, &value));
    SEMU_TEST_EQ_U64(context, 2u, value);
    /* Misaligned access still refuses (validate_access). */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus, SEMU_APOLLO4_STIMER_BASE + 0x21u,
                                   4u, &value, &fixture.error));
    /* The shared profile keeps the +0x24 refusal (Sapporo guard). */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     write_register(&shared, 0x24u, 96u));
    fixture_destroy(&fixture);
    fixture_destroy(&shared);
}

/* Under the flag the committed snapshot keeps CFG 0x303 / INTEN 0x3
 * alive through the round trip (the class masks would reject them). */
static void test_wismar_tick_snapshot_roundtrip(semu_test_context *context)
{
    wismar_stimer_fixture fixture = { 0 };
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    uint32_t value = 0u;
    SEMU_TEST_ASSERT(context, fixture_init(&fixture, 1, 1));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x00u, 0x303u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x100u, 0x3u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x20u, 5u));
    SEMU_TEST_EQ_U64(context, 1u,
                     semu_scheduler_event_count(fixture.scheduler));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_stimer_snapshot_write(fixture.stimer, &writer,
                                           &fixture.error));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_apollo4_stimer_snapshot_read(fixture.stimer, &reader,
                                          &fixture.error));
    SEMU_TEST_EQ_U64(context, 1u,
                     semu_scheduler_event_count(fixture.scheduler));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x00u, &value));
    SEMU_TEST_EQ_U64(context, 0x303u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x100u, &value));
    SEMU_TEST_EQ_U64(context, 3u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x20u, &value));
    SEMU_TEST_EQ_U64(context, 8u, value);
    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_wismar_compare_b_arms_and_fires),
        SEMU_TEST_CASE(test_wismar_tick_masks_and_refusals),
        SEMU_TEST_CASE(test_wismar_tick_snapshot_roundtrip)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
