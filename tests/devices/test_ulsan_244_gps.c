#include "../../src/compat/ulsan_244_gps.h"
#include "test.h"

typedef struct {
    int uart;
    int gpio;
} irq_levels;

static void record_irq(void *context, unsigned irq, int level)
{
    irq_levels *levels = context;
    if (irq == SEMU_APOLLO4_UART1_IRQ) levels->uart = level;
    if (irq == SEMU_APOLLO4_GPIO_IRQ_BASE) levels->gpio = level;
}

static void receive_status(semu_test_context *context, semu_bus *bus,
    semu_scheduler *scheduler, const irq_levels *levels)
{
    static const char line[] = "$PSS0000\r\n";
    const uint32_t uart = SEMU_APOLLO4_UART1_BASE;
    semu_error error;
    uint32_t value;
    size_t i;

    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(scheduler, 9999999u, &error));
    SEMU_TEST_EQ_U64(context, 0u, levels->uart);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(scheduler, 1u, &error));
    SEMU_TEST_EQ_U64(context, 1u, levels->uart);
    for (i = 0u; i < sizeof(line) - 1u; ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_read(bus, uart + SEMU_APOLLO4_UART_DATA, 4u, &value, &error));
        SEMU_TEST_EQ_U64(context, (uint8_t)line[i], value);
    }
    SEMU_TEST_EQ_U64(context, 0u, levels->uart);
}

static void send_command(semu_test_context *context, semu_bus *bus,
    const char *line)
{
    semu_error error;
    for (; *line != '\0'; ++line)
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus,
            SEMU_APOLLO4_UART1_BASE + SEMU_APOLLO4_UART_DATA, 4u, (uint8_t)*line, &error));
}

static void test_native_gps_sequence(semu_test_context *context)
{
    const semu_layer_descriptor *desc = &semu_ulsan_244_gps_layer;
    const char *wrong_hashes[] = {"wrong", "wrong", "wrong"};
    const uint32_t uart = SEMU_APOLLO4_UART1_BASE;
    const uint32_t gpio_base = SEMU_APOLLO4_GPIO_BASE;
    const uint32_t driver = 0x10000000u;
    semu_layer_state layer = {0};
    irq_levels levels = {0};
    semu_ulsan_244_gps gps = {0};
    semu_cpu_state cpu = {0};
    semu_error error;
    semu_bus *bus = semu_bus_create(&error);
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    semu_apollo4_gpio *gpio;

    SEMU_TEST_ASSERT(context, bus != NULL && scheduler != NULL);
    gpio = semu_apollo4_gpio_create(bus, NULL, NULL, &error);
    SEMU_TEST_ASSERT(context, gpio != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_ram(bus, "gps-ram", driver, 0x1000u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT, semu_layer_enable_checked(&layer,
        desc, desc->profile_id, wrong_hashes, SEMU_ARRAY_LEN(wrong_hashes), &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_layer_enable_checked(&layer, desc,
        desc->profile_id, desc->component_hashes, desc->component_hash_count, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_244_gps_bind(&gps, bus, gpio,
        scheduler, record_irq, &levels, &layer, NULL, &error));

    cpu.r[4] = driver;
    cpu.r[15] = 0x0012c128u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, semu_ulsan_244_gps_observe(&gps, bus, &cpu, &error));
    SEMU_TEST_EQ_U64(context, 0u, layer.hits);
    semu_ulsan_244_gps_reset(&gps);

    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, driver + 0x650u, 4u, driver + 0x800u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, uart + SEMU_APOLLO4_UART_CONTROL, 4u, 0x319u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, uart + SEMU_APOLLO4_UART_INTERRUPT_MASK, 4u, 0x51u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_244_gps_observe(&gps, bus, &cpu, &error));
    receive_status(context, bus, scheduler, &levels);
    send_command(context, bus, "@VER\r\n");
    receive_status(context, bus, scheduler, &levels);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, driver + 0x6a2u, 1u, 19u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_244_gps_observe(&gps, bus, &cpu, &error));
    receive_status(context, bus, scheduler, &levels);
    send_command(context, bus, "@GSR\r\n");
    receive_status(context, bus, scheduler, &levels);

    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, gpio_base + 0x200u, 4u, 0x73u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, gpio_base + 0x60u, 4u, 0x93u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, gpio_base + 0x2c0u, 4u, 1u << 24u, &error));
    cpu.r[15] = 0x0012c628u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_244_gps_observe(&gps, bus, &cpu, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(scheduler, 99999999u, &error));
    SEMU_TEST_EQ_U64(context, 0u, levels.gpio);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(scheduler, 1u, &error));
    SEMU_TEST_EQ_U64(context, 1u, levels.gpio);
    SEMU_TEST_EQ_U64(context, 1u, semu_apollo4_gpio_get_input(gpio, 24u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, gpio_base + 0x2c8u, 4u, 1u << 24u, &error));
    SEMU_TEST_EQ_U64(context, 0u, levels.gpio);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(scheduler, 1000000u, &error));
    SEMU_TEST_EQ_U64(context, 0u, semu_apollo4_gpio_get_input(gpio, 24u));
    SEMU_TEST_EQ_U64(context, 5u, layer.hits);

    layer.hits = desc->maximum_hits;
    cpu.r[15] = 0x0012c79cu;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, semu_ulsan_244_gps_observe(&gps, bus, &cpu, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(scheduler, 100000000u, &error));
    SEMU_TEST_EQ_U64(context, 0u, levels.gpio);

    semu_ulsan_244_gps_destroy(&gps);
    semu_apollo4_gpio_destroy(gpio);
    semu_scheduler_destroy(scheduler);
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_native_gps_sequence)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
