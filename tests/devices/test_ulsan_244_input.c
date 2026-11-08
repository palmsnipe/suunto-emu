#include "../../src/devices/ulsan_crown.h"
#include "../../src/devices/ulsan_touch.h"
#include "../../src/soc/apollo4/dma.h"
#include "../../src/soc/apollo4/iom.h"
#include "test.h"

#include <string.h>

static void test_crown_motion(semu_test_context *context)
{
    uint8_t reg, out;
    semu_serial_transaction read = {0x75u, 0u, &reg, 1u, &out, 1u};
    semu_ulsan_crown crown = {0};
    semu_error error;
    semu_bus *bus = semu_bus_create(&error);
    unsigned i;
    static const uint8_t regs[] = {2u, 3u, 4u, 0x12u, 2u};
    static const uint8_t expected[] = {0x80u, 0xe8u, 0u, 0xf0u, 0u};
    static const uint8_t reverse[] = {0x80u, 0x18u, 0u, 0u, 0u};

    SEMU_TEST_ASSERT(context, bus != NULL);
    crown.gpio = semu_apollo4_gpio_create(bus, NULL, NULL, &error);
    SEMU_TEST_ASSERT(context, crown.gpio != NULL);
    semu_ulsan_crown_reset(&crown);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, semu_ulsan_crown_rotate(&crown, -2048, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_crown_rotate(&crown, 24, &error));
    SEMU_TEST_EQ_U64(context, 0u, semu_apollo4_gpio_get_input(crown.gpio, 104u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, semu_ulsan_crown_rotate(&crown, -24, &error));
    reg = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, semu_ulsan_crown_transfer(&crown, &read, &error));
    for (i = 0u; i < sizeof(regs); ++i) {
        reg = regs[i];
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, semu_ulsan_crown_transfer(&crown, &read, &error));
        SEMU_TEST_EQ_U64(context, expected[i], out);
        SEMU_TEST_EQ_U64(context, i >= 3u, semu_apollo4_gpio_get_input(crown.gpio, 104u));
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_crown_rotate(&crown, -24, &error));
    for (i = 0u; i < sizeof(regs); ++i) {
        reg = regs[i];
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, semu_ulsan_crown_transfer(&crown, &read, &error));
        SEMU_TEST_EQ_U64(context, reverse[i], out);
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_crown_rotate(&crown, 24, &error));
    semu_ulsan_crown_reset(&crown);
    SEMU_TEST_EQ_U64(context, 1u, semu_apollo4_gpio_get_input(crown.gpio, 104u));
    semu_apollo4_gpio_destroy(crown.gpio);
    semu_bus_destroy(bus);
}

static semu_status touch_read(semu_bus *bus, uint32_t address, uint32_t size,
    semu_error *error)
{
    const uint32_t iom = SEMU_APOLLO4_IOM3_BASE;
    if (semu_bus_write(bus, iom + 0x220u, 4u, address, error) != SEMU_OK ||
        semu_bus_write(bus, iom + 0x21cu, 4u, size, error) != SEMU_OK ||
        semu_bus_write(bus, iom + 0x218u, 4u, 0x101u, error) != SEMU_OK)
        return error->code;
    return semu_bus_write(bus, iom + 0x120u, 4u, (size << 8u) | 2u, error);
}

static void test_touch_dma(semu_test_context *context)
{
    static const uint8_t expected[13] = {
        0u, 0u, 1u, 50u, 0u, 1u, 0u, 0u, 0x80u, 0xe9u, 0u, 0x4au, 1u
    };
    const uint32_t address = 0x10000000u;
    semu_ulsan_touch touch = {0};
    const semu_serial_endpoint endpoint = {"touch", semu_ulsan_touch_transfer, &touch};
    semu_error error;
    semu_bus *bus = semu_bus_create(&error);
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    semu_apollo4_dma *dma;
    semu_apollo4_iom *iom;
    uint8_t data[13];
    uint32_t input;

    SEMU_TEST_ASSERT(context, bus != NULL && scheduler != NULL);
    dma = semu_apollo4_dma_create(bus, scheduler, &error);
    touch.gpio = semu_apollo4_gpio_create(bus, NULL, NULL, &error);
    SEMU_TEST_ASSERT(context, dma != NULL && touch.gpio != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        semu_apollo4_gpio_read(touch.gpio, 0x210u, 4u, &input, &error));
    semu_apollo4_gpio_enable_race_s_inputs(touch.gpio);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_gpio_write(touch.gpio, 0x200u, 4u, 0x73u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_gpio_write(touch.gpio, 0x190u, 4u, 0x53u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_gpio_write(touch.gpio, 0x1a0u, 4u, 0xe053u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_gpio_write(touch.gpio, 0x2f0u, 4u, 0x110u, &error));
    SEMU_TEST_ASSERT(context, semu_apollo4_gpio_falling_input_armed(touch.gpio, 100u));
    iom = semu_apollo4_iom_create(bus, SEMU_APOLLO4_IOM3_BASE, SEMU_APOLLO4_IOM3_IRQ,
        NULL, NULL, semu_apollo4_dma_execute, dma, scheduler, &error);
    SEMU_TEST_ASSERT(context, iom != NULL);
    semu_apollo4_iom_select_race_s(iom);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_iom_attach_endpoint(iom, &endpoint, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_ram(bus, "touch", address, 16u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, SEMU_APOLLO4_IOM3_BASE + 0x2c4u, 4u, 0x24u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, touch_read(bus, address, 2u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, semu_ulsan_touch_point(&touch, 4096, 330, 100, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_touch_point(&touch, 233, 330, 100, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_gpio_read(touch.gpio, 0x210u, 4u, &input, &error));
    SEMU_TEST_EQ_U64(context, 0x100u, input);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, semu_ulsan_touch_point(&touch, 0, 0, 0, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, touch_read(bus, address, 13u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, touch_read(bus, address, 2u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_copy_out(bus, address, data, 2u, &error));
    SEMU_TEST_EQ_U64(context, 13u, data[0]);
    SEMU_TEST_EQ_U64(context, 0u, data[1]);
    SEMU_TEST_ASSERT(context, touch_read(bus, address + 8u, 13u, &error) != SEMU_OK);
    SEMU_TEST_EQ_U64(context, 0u, semu_apollo4_gpio_get_input(touch.gpio, 100u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, touch_read(bus, address, 13u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_copy_out(bus, address, data, sizeof(data), &error));
    SEMU_TEST_ASSERT(context, memcmp(data, expected, sizeof(data)) == 0);
    SEMU_TEST_EQ_U64(context, 1u, semu_apollo4_gpio_get_input(touch.gpio, 100u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_touch_point(&touch, 233, 330, 0, &error));
    semu_ulsan_touch_reset(&touch);
    SEMU_TEST_EQ_U64(context, 1u, semu_apollo4_gpio_get_input(touch.gpio, 100u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, touch_read(bus, address, 2u, &error));
    semu_apollo4_iom_destroy(iom);
    semu_apollo4_dma_destroy(dma);
    semu_apollo4_gpio_destroy(touch.gpio);
    semu_scheduler_destroy(scheduler);
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_crown_motion), SEMU_TEST_CASE(test_touch_dma)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
