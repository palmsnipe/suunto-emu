#include "semu/apollo4.h"

#include <stdio.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

int main(void)
{
    semu_error error;
    semu_bus *bus;
    semu_apollo4 *soc;
    uint32_t value = 0u;
    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    CHECK(bus != NULL);
    soc = semu_apollo4_create(bus, &error);
    CHECK(soc != NULL);
    CHECK(semu_bus_write(bus, 0x40004084u, 4u, 0x12345678u, &error) == SEMU_OK);
    CHECK(semu_bus_read(bus, 0x40004084u, 4u, &value, &error) == SEMU_OK);
    CHECK(value == 0x12345678u);
    CHECK(semu_bus_read(bus, 0x40004080u, 4u, &value, &error) ==
          SEMU_ERR_UNSUPPORTED);
    CHECK(semu_apollo4_set_gpio_input(soc, 57u, 0, &error) == SEMU_OK);
    CHECK(semu_apollo4_get_gpio_input(soc, 57u) == 0);
    CHECK(semu_apollo4_set_gpio_input(soc, 128u, 0, &error) == SEMU_ERR_RANGE);
    semu_apollo4_destroy(soc);
    semu_bus_destroy(bus);
    return failures != 0 ? 1 : 0;
}
