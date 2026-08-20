#include "test.h"

#include <stdint.h>

#include "../../src/soc/apollo4/gpio.h"

typedef struct gpio_fixture {
    semu_error error;
    semu_bus *bus;
    semu_apollo4_gpio *gpio;
} gpio_fixture;

static int fixture_init(gpio_fixture *fixture)
{
    semu_error_clear(&fixture->error);
    fixture->bus = semu_bus_create(&fixture->error);
    if (fixture->bus == NULL) return 0;
    fixture->gpio = semu_apollo4_gpio_create(fixture->bus, NULL, NULL,
                                             &fixture->error);
    return fixture->gpio != NULL;
}

static void fixture_destroy(gpio_fixture *fixture)
{
    semu_apollo4_gpio_destroy(fixture->gpio);
    semu_bus_destroy(fixture->bus);
}

static void test_unreachable_state_refuses(semu_test_context *context)
{
    static const size_t offsets[] = { 512u, 640u, 768u, 976u };
    static const uint8_t values[] = { 2u, 2u, 4u, 1u };
    gpio_fixture source;
    gpio_fixture target;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    size_t index;

    SEMU_TEST_ASSERT(context, fixture_init(&source));
    SEMU_TEST_ASSERT(context, fixture_init(&target));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_gpio_snapshot_write(source.gpio, &writer,
                                                      &source.error));
    for (index = 0u; index < sizeof(offsets) / sizeof(offsets[0]); ++index) {
        uint8_t original = writer.data[offsets[index]];
        writer.data[offsets[index]] = values[index];
        semu_snapshot_reader_init(&reader, writer.data, writer.size);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                         semu_apollo4_gpio_snapshot_read(target.gpio, &reader,
                                                          &target.error));
        writer.data[offsets[index]] = original;
    }
    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&target);
    fixture_destroy(&source);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_unreachable_state_refuses)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
