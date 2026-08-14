#include "test.h"

#include <stdint.h>

#include "../../src/soc/apollo4/gpio.h"

typedef struct gpio_fixture {
    semu_error error;
    semu_bus *bus;
    semu_apollo4_gpio *gpio;
    unsigned irq_count;
    unsigned irq[16];
    int irq_level[16];
    unsigned output_count;
    unsigned output_pin[16];
    int output_level[16];
} gpio_fixture;

static void irq(void *context, unsigned line, int level)
{
    gpio_fixture *fixture = (gpio_fixture *)context;
    if (fixture->irq_count < SEMU_ARRAY_LEN(fixture->irq)) {
        fixture->irq[fixture->irq_count] = line;
        fixture->irq_level[fixture->irq_count] = level;
    }
    ++fixture->irq_count;
}

static void output(void *context, unsigned pin, int level)
{
    gpio_fixture *fixture = (gpio_fixture *)context;
    if (fixture->output_count < SEMU_ARRAY_LEN(fixture->output_pin)) {
        fixture->output_pin[fixture->output_count] = pin;
        fixture->output_level[fixture->output_count] = level;
    }
    ++fixture->output_count;
}

static int fixture_init(gpio_fixture *fixture)
{
    semu_error_clear(&fixture->error);
    fixture->bus = semu_bus_create(&fixture->error);
    if (fixture->bus == NULL) return 0;
    fixture->gpio = semu_apollo4_gpio_create(fixture->bus, irq, fixture,
                                             &fixture->error);
    return fixture->gpio != NULL;
}

static void fixture_destroy(gpio_fixture *fixture)
{
    semu_apollo4_gpio_destroy(fixture->gpio);
    semu_bus_destroy(fixture->bus);
}

static semu_status read_register(gpio_fixture *fixture, uint32_t offset,
                                 uint32_t *value)
{
    return semu_bus_read(fixture->bus, SEMU_APOLLO4_GPIO_BASE + offset, 4u,
                         value, &fixture->error);
}

static semu_status write_register(gpio_fixture *fixture, uint32_t offset,
                                  uint32_t value)
{
    return semu_bus_write(fixture->bus, SEMU_APOLLO4_GPIO_BASE + offset, 4u,
                          value, &fixture->error);
}

static void test_reset_and_input_banks(semu_test_context *context)
{
    gpio_fixture fixture = { 0 };
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, 1u,
                     semu_apollo4_gpio_get_input(fixture.gpio, 127u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x204u,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 0xffffffffu, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x208u,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 0xffffffffu, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_gpio_set_input(fixture.gpio, 63u, 0,
                                                 &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x208u,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 0x7fffffffu, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_gpio_set_input(fixture.gpio, 127u, 0,
                                                 &fixture.error));
    SEMU_TEST_EQ_U64(context, 0u, fixture.irq_count);
    fixture_destroy(&fixture);
}

static void test_pin_config_and_output(semu_test_context *context)
{
    gpio_fixture fixture = { 0 };
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_gpio_set_output_observer(
                         fixture.gpio, output, &fixture, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x200u,
                                                      0x73u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x1fcu,
                                                      0x105u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x200u, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x1fcu,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 0x105u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_gpio_configure_pin(
                         fixture.gpio, 127u, SEMU_APOLLO4_GPIO_OUTPUT,
                         SEMU_APOLLO4_GPIO_EDGE_DISABLED, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x230u,
                                                      0x80000000u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x240u,
                                                      0x80000000u));
    SEMU_TEST_EQ_U64(context, 2u, fixture.output_count);
    SEMU_TEST_EQ_U64(context, 127u, fixture.output_pin[0]);
    SEMU_TEST_EQ_U64(context, 1u, fixture.output_level[0]);
    SEMU_TEST_EQ_U64(context, 0u, fixture.output_level[1]);
    fixture_destroy(&fixture);
}

static void test_edges_status_and_irq(semu_test_context *context)
{
    gpio_fixture fixture = { 0 };
    uint32_t value = 0u;
    const uint32_t bit = UINT32_C(1) << 25;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_gpio_configure_pin(
                         fixture.gpio, 57u, SEMU_APOLLO4_GPIO_INPUT,
                         SEMU_APOLLO4_GPIO_EDGE_FALLING, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_gpio_set_input(fixture.gpio, 57u, 0,
                                                 &fixture.error));
    SEMU_TEST_EQ_U64(context, 0u, fixture.irq_count);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x2d0u,
                                                      bit));
    SEMU_TEST_EQ_U64(context, 1u, fixture.irq_count);
    SEMU_TEST_EQ_U64(context, 57u, fixture.irq[0]);
    SEMU_TEST_EQ_U64(context, 1u, fixture.irq_level[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x2d4u,
                                                     &value));
    SEMU_TEST_EQ_U64(context, bit, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x2d8u,
                                                      bit));
    SEMU_TEST_EQ_U64(context, 2u, fixture.irq_count);
    SEMU_TEST_EQ_U64(context, 0u, fixture.irq_level[1]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_gpio_configure_pin(
                         fixture.gpio, 57u, SEMU_APOLLO4_GPIO_INPUT,
                         SEMU_APOLLO4_GPIO_EDGE_BOTH, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_gpio_set_input(fixture.gpio, 57u, 1,
                                                 &fixture.error));
    SEMU_TEST_EQ_U64(context, 3u, fixture.irq_count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_gpio_configure_pin(
                         fixture.gpio, 57u, SEMU_APOLLO4_GPIO_INPUT,
                         SEMU_APOLLO4_GPIO_EDGE_DISABLED, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_gpio_set_input(fixture.gpio, 57u, 0,
                                                 &fixture.error));
    SEMU_TEST_EQ_U64(context, 3u, fixture.irq_count);
    fixture_destroy(&fixture);
}

static void test_refusal_is_atomic(semu_test_context *context)
{
    gpio_fixture fixture = { 0 };
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
                     write_register(&fixture, 0x00u, 0xE053u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x200u,
                                                      0x73u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x00u,
                                                      0xE013u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     write_register(&fixture, 0x20cu, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     read_register(&fixture, 0x210u, &value));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     write_register(&fixture, 0x200u, 0x74u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x00u,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 0xE013u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
                     semu_apollo4_gpio_set_input(fixture.gpio, 128u, 0,
                                                 &fixture.error));
    SEMU_TEST_EQ_U64(context, 1u,
                     semu_apollo4_gpio_get_input(fixture.gpio, 127u));
    fixture_destroy(&fixture);
}

static void test_reset_repeatability(semu_test_context *context)
{
    gpio_fixture fixture = { 0 };
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_gpio_configure_pin(
                         fixture.gpio, 127u, SEMU_APOLLO4_GPIO_INPUT,
                         SEMU_APOLLO4_GPIO_EDGE_RISING, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, write_register(&fixture, 0x2f0u,
                                                      0x80000000u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_gpio_set_input(fixture.gpio, 127u, 0,
                                                 &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_gpio_set_input(fixture.gpio, 127u, 1,
                                                 &fixture.error));
    SEMU_TEST_EQ_U64(context, 59u, fixture.irq[0]);
    semu_apollo4_gpio_reset(fixture.gpio);
    SEMU_TEST_EQ_U64(context, 2u, fixture.irq_count);
    SEMU_TEST_EQ_U64(context, 0u, fixture.irq_level[1]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x2f0u,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, read_register(&fixture, 0x2f4u,
                                                     &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, 1u,
                     semu_apollo4_gpio_get_input(fixture.gpio, 127u));
    fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_reset_and_input_banks),
        SEMU_TEST_CASE(test_pin_config_and_output),
        SEMU_TEST_CASE(test_edges_status_and_irq),
        SEMU_TEST_CASE(test_refusal_is_atomic),
        SEMU_TEST_CASE(test_reset_repeatability)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
