#include "test.h"

#include <stdint.h>

#include "../../src/soc/apollo4/stimer.h"

typedef struct irq_record {
    unsigned count;
    semu_apollo4_stimer_irq irq[8];
    int level[8];
    uint64_t time[8];
} irq_record;

typedef struct stimer_fixture {
    semu_error error;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4_stimer *stimer;
    irq_record record;
} stimer_fixture;

static void irq_callback(void *context, unsigned irq, int level)
{
    stimer_fixture *fixture = (stimer_fixture *)context;
    unsigned index = fixture->record.count;
    if (index < 8u) {
        fixture->record.irq[index] = irq;
        fixture->record.level[index] = level;
        fixture->record.time[index] = semu_scheduler_now(fixture->scheduler);
    }
    ++fixture->record.count;
}

static int fixture_init(stimer_fixture *fixture)
{
    semu_error_clear(&fixture->error);
    fixture->bus = semu_bus_create(&fixture->error);
    fixture->scheduler = semu_scheduler_create(&fixture->error);
    if (fixture->bus == NULL || fixture->scheduler == NULL) return 0;
    fixture->stimer = semu_apollo4_stimer_create(
        fixture->bus, fixture->scheduler, irq_callback, fixture,
        &fixture->error);
    return fixture->stimer != NULL;
}

static void fixture_destroy(stimer_fixture *fixture)
{
    semu_apollo4_stimer_destroy(fixture->stimer);
    semu_scheduler_destroy(fixture->scheduler);
    semu_bus_destroy(fixture->bus);
}

static semu_status read_register(stimer_fixture *fixture, uint32_t offset,
                                 uint32_t *value)
{
    return semu_bus_read(fixture->bus, SEMU_APOLLO4_STIMER_BASE + offset, 4u,
                         value, &fixture->error);
}

static semu_status write_register(stimer_fixture *fixture, uint32_t offset,
                                  uint32_t value)
{
    return semu_bus_write(fixture->bus, SEMU_APOLLO4_STIMER_BASE + offset, 4u,
                          value, &fixture->error);
}

static void test_reset_retention_and_counter(semu_test_context *context)
{
    stimer_fixture fixture = { 0u };
    uint32_t value = 0u;
    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x00u, &value));
    SEMU_TEST_EQ_U64(context, UINT32_C(0x80000000), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x50u, 0x11u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x54u, 0x22u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x58u, 0x33u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x5cu, 0x44u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler, 9u,
                                             &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x04u, &value));
    SEMU_TEST_EQ_U64(context, 9u, value);
    semu_apollo4_stimer_reset(fixture.stimer);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x04u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x50u, &value));
    SEMU_TEST_EQ_U64(context, 0x11u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x5cu, &value));
    SEMU_TEST_EQ_U64(context, 0x44u, value);
    fixture_destroy(&fixture);
}

static void test_relative_compare_irq_and_clear(semu_test_context *context)
{
    stimer_fixture fixture = { 0u };
    uint32_t value = 0u;
    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x00u, 0x503u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x100u, 0x100u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x20u, 5u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x20u, &value));
    SEMU_TEST_EQ_U64(context, 5u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler, 4u,
                                             &fixture.error));
    SEMU_TEST_EQ_U64(context, 0u, fixture.record.count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler, 1u,
                                             &fixture.error));
    SEMU_TEST_EQ_U64(context, 1u, fixture.record.count);
    SEMU_TEST_EQ_U64(context, SEMU_APOLLO4_STIMER_IRQ_A,
                     fixture.record.irq[0]);
    SEMU_TEST_EQ_U64(context, 1u, fixture.record.level[0]);
    SEMU_TEST_EQ_U64(context, 5u, fixture.record.time[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x104u, &value));
    SEMU_TEST_EQ_U64(context, 1u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x108u, 1u));
    SEMU_TEST_EQ_U64(context, 2u, fixture.record.count);
    SEMU_TEST_EQ_U64(context, 0u, fixture.record.level[1]);
    fixture_destroy(&fixture);
}

static void test_compare_c_rewrite_and_wrap(semu_test_context *context)
{
    stimer_fixture fixture = { 0u };
    uint32_t value = 0u;
    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x00u, 0x503u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x100u, 0x101u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x28u, 8u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x28u, &value));
    SEMU_TEST_EQ_U64(context, 8u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler, 3u,
                                             &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x28u, 4u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler, 3u,
                                             &fixture.error));
    SEMU_TEST_EQ_U64(context, 0u, fixture.record.count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler, 1u,
                                             &fixture.error));
    SEMU_TEST_EQ_U64(context, 1u, fixture.record.count);
    SEMU_TEST_EQ_U64(context, SEMU_APOLLO4_STIMER_IRQ_C,
                     fixture.record.irq[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x108u, 4u));
    semu_apollo4_stimer_reset(fixture.stimer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler,
                                             UINT32_MAX - 2u,
                                             &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x00u, 0x503u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x100u, 0x100u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x20u, 5u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler, 5u,
                                             &fixture.error));
    SEMU_TEST_EQ_U64(context, 3u, fixture.record.count);
    SEMU_TEST_EQ_U64(context, SEMU_APOLLO4_STIMER_IRQ_A,
                     fixture.record.irq[2]);
    fixture_destroy(&fixture);
}

static void test_refusal_atomicity_and_repeatability(semu_test_context *context)
{
    stimer_fixture fixture = { 0u };
    uint32_t value = 0u;
    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x20u, 7u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     write_register(&fixture, 0x24u, 7u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     write_register(&fixture, 0x00u, 0x80000803u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     write_register(&fixture, 0x108u, 2u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus, SEMU_APOLLO4_STIMER_BASE +
                                   0x20u, 2u, &value, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x20u, &value));
    SEMU_TEST_EQ_U64(context, 7u, value);
    SEMU_TEST_EQ_U64(context, 0u, fixture.record.count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler, 7u,
                                             &fixture.error));
    SEMU_TEST_EQ_U64(context, 0u, fixture.record.count);
    semu_apollo4_stimer_reset(fixture.stimer);
    SEMU_TEST_ASSERT(context, !semu_scheduler_has_events(fixture.scheduler));
    fixture_destroy(&fixture);
}

static void test_repeatable_trace(semu_test_context *context)
{
    stimer_fixture first = { 0u };
    stimer_fixture second = { 0u };
    uint32_t first_counter = 0u;
    uint32_t second_counter = 0u;
    SEMU_TEST_ASSERT(context, fixture_init(&first));
    SEMU_TEST_ASSERT(context, fixture_init(&second));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&first, 0x00u, 0x503u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&second, 0x00u, 0x503u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&first, 0x100u, 0x101u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&second, 0x100u, 0x101u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&first, 0x20u, 11u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&second, 0x20u, 11u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(first.scheduler, 11u,
                                             &first.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(second.scheduler, 11u,
                                             &second.error));
    SEMU_TEST_EQ_U64(context, 1u, first.record.count);
    SEMU_TEST_EQ_U64(context, first.record.count, second.record.count);
    SEMU_TEST_EQ_U64(context, first.record.irq[0], second.record.irq[0]);
    SEMU_TEST_EQ_U64(context, first.record.level[0], second.record.level[0]);
    SEMU_TEST_EQ_U64(context, first.record.time[0], second.record.time[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&first, 0x04u, &first_counter));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&second, 0x04u, &second_counter));
    SEMU_TEST_EQ_U64(context, first_counter, second_counter);
    fixture_destroy(&first);
    fixture_destroy(&second);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_reset_retention_and_counter),
        SEMU_TEST_CASE(test_relative_compare_irq_and_clear),
        SEMU_TEST_CASE(test_compare_c_rewrite_and_wrap),
        SEMU_TEST_CASE(test_refusal_atomicity_and_repeatability),
        SEMU_TEST_CASE(test_repeatable_trace)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
