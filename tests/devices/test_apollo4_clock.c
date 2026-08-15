#include "test.h"

#include <stdint.h>

#include "../../src/soc/apollo4/clock.h"

typedef struct clock_fixture {
    semu_error error;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4_clock *clock;
} clock_fixture;

static int fixture_init(clock_fixture *fixture)
{
    semu_error_clear(&fixture->error);
    fixture->bus = semu_bus_create(&fixture->error);
    fixture->scheduler = semu_scheduler_create(&fixture->error);
    if (fixture->bus == NULL || fixture->scheduler == NULL) {
        return 0;
    }
    fixture->clock = semu_apollo4_clock_create(fixture->bus, fixture->scheduler,
                                               &fixture->error);
    return fixture->clock != NULL;
}

static void fixture_destroy(clock_fixture *fixture)
{
    semu_apollo4_clock_destroy(fixture->clock);
    semu_scheduler_destroy(fixture->scheduler);
    semu_bus_destroy(fixture->bus);
}

static semu_status read_register(clock_fixture *fixture, uint32_t offset,
                                  uint32_t *value)
{
    return semu_bus_read(fixture->bus, SEMU_APOLLO4_CLOCK_BASE + offset,
                         4u, value, &fixture->error);
}

static semu_status write_register(clock_fixture *fixture, uint32_t offset,
                                   uint32_t value)
{
    return semu_bus_write(fixture->bus, SEMU_APOLLO4_CLOCK_BASE + offset,
                          4u, value, &fixture->error);
}

static void test_reset_values(semu_test_context *context)
{
    clock_fixture fixture;
    uint32_t value = 0xDEADu;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x44u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x0cu, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    fixture_destroy(&fixture);
}

static void test_evidenced_access_sequence(semu_test_context *context)
{
    clock_fixture fixture;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));

    /* Offset 0x44: read 0x0, write 0xF80000, read 0xF80000, write 0xF80040 */
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x44u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x44u,
                                                      UINT32_C(0xF80000)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x44u, &value));
    SEMU_TEST_EQ_U64(context, UINT32_C(0xF80000), value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x44u,
                                                      UINT32_C(0xF80040)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x44u, &value));
    SEMU_TEST_EQ_U64(context, UINT32_C(0xF80040), value);

    /* Offset 0x0c: read 0x0, write 0x0 */
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x0cu, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x0cu, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x0cu, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);

    fixture_destroy(&fixture);
}

static void test_wrong_width(semu_test_context *context)
{
    clock_fixture fixture;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus, SEMU_APOLLO4_CLOCK_BASE + 0x44u,
                                   2u, &value, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_write(fixture.bus, SEMU_APOLLO4_CLOCK_BASE + 0x44u,
                                    1u, 0u, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus, SEMU_APOLLO4_CLOCK_BASE + 0x0cu,
                                   2u, &value, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x44u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    fixture_destroy(&fixture);
}

static void test_adjacent_and_unknown_offsets_refuse(semu_test_context *context)
{
    clock_fixture fixture;
    uint32_t value = 0u;
    static const uint32_t refuse_offsets[] = { 0x80u, 0x84u, 0x00u, 0x40u,
                                               0x48u, 0x08u, 0x10u };
    size_t i;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    for (i = 0u; i < sizeof(refuse_offsets) / sizeof(refuse_offsets[0]); ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                         semu_bus_read(fixture.bus,
                                       SEMU_APOLLO4_CLOCK_BASE + refuse_offsets[i],
                                       4u, &value, &fixture.error));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                         semu_bus_write(fixture.bus,
                                        SEMU_APOLLO4_CLOCK_BASE + refuse_offsets[i],
                                        4u, 0u, &fixture.error));
    }
    fixture_destroy(&fixture);
}

static void test_reset_clears_state(semu_test_context *context)
{
    clock_fixture fixture;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x44u,
                                                      UINT32_C(0xF80040)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x0cu,
                                                      UINT32_C(0xDEAD)));
    semu_apollo4_clock_reset(fixture.clock);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x44u, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x0cu, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    fixture_destroy(&fixture);
}

static void test_two_run_equality(semu_test_context *context)
{
    clock_fixture a, b;
    uint32_t va = 0u, vb = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&a));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&a, 0x44u,
                                                      UINT32_C(0xF80000)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&a, 0x44u,
                                                      UINT32_C(0xF80040)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&a, 0x0cu, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&a, 0x44u, &va));
    fixture_destroy(&a);

    SEMU_TEST_ASSERT(context, fixture_init(&b));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&b, 0x44u,
                                                      UINT32_C(0xF80000)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&b, 0x44u,
                                                      UINT32_C(0xF80040)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&b, 0x0cu, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&b, 0x44u, &vb));
    fixture_destroy(&b);

    SEMU_TEST_EQ_U64(context, va, vb);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_reset_values),
        SEMU_TEST_CASE(test_evidenced_access_sequence),
        SEMU_TEST_CASE(test_wrong_width),
        SEMU_TEST_CASE(test_adjacent_and_unknown_offsets_refuse),
        SEMU_TEST_CASE(test_reset_clears_state),
        SEMU_TEST_CASE(test_two_run_equality)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
