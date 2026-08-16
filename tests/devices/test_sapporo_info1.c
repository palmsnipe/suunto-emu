#include "test.h"

#include "sapporo_info1.h"

static void test_zero_read_and_refusals(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = UINT32_C(0xffffffff);

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    if (bus == NULL) {
        return;
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_info1_map(bus, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x42003310u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(bus, 0x42003310u, 2u, &value, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_write(bus, 0x42003310u, 4u, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
                     semu_bus_read(bus, 0x42000000u, 4u, &value, &error));
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_zero_read_and_refusals)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
