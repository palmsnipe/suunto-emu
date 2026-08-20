#include "test.h"

#include "../../src/soc/apollo4/apollo4_internal.h"

typedef struct apollo4_fixture {
    semu_error error;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4 *soc;
} apollo4_fixture;

static int fixture_init(apollo4_fixture *fixture)
{
    semu_error_clear(&fixture->error);
    fixture->bus = semu_bus_create(&fixture->error);
    fixture->scheduler = semu_scheduler_create(&fixture->error);
    if (fixture->bus == NULL || fixture->scheduler == NULL) return 0;
    fixture->soc = semu_apollo4_create(fixture->bus, &fixture->error);
    if (fixture->soc == NULL) return 0;
    return semu_apollo4_init(fixture->soc, fixture->scheduler, NULL, NULL,
                             &fixture->error) == SEMU_OK;
}

static void fixture_destroy(apollo4_fixture *fixture)
{
    semu_apollo4_destroy(fixture->soc);
    semu_scheduler_destroy(fixture->scheduler);
    semu_bus_destroy(fixture->bus);
}

static void test_invalid_gpio_level_refuses(semu_test_context *context)
{
    apollo4_fixture source;
    apollo4_fixture target;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;

    SEMU_TEST_ASSERT(context, fixture_init(&source));
    SEMU_TEST_ASSERT(context, fixture_init(&target));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_snapshot_write(source.soc, &writer,
                                                  &source.error));
    writer.data[0u] = 2u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_apollo4_snapshot_read(target.soc, &reader,
                                                &target.error));
    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&target);
    fixture_destroy(&source);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_invalid_gpio_level_refuses)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
