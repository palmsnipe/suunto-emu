#include "test.h"

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/scheduler.h"

static void test_documented_word_and_refusals(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4 *soc;
    uint32_t value = 0u;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    scheduler = semu_scheduler_create(&error);
    soc = bus == NULL || scheduler == NULL
              ? NULL
              : semu_apollo4_create(bus, &error);
    SEMU_TEST_ASSERT(context, bus != NULL && scheduler != NULL && soc != NULL);
    if (bus == NULL || scheduler == NULL || soc == NULL) {
        semu_apollo4_destroy(soc);
        semu_scheduler_destroy(scheduler);
        semu_bus_destroy(bus);
        return;
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_init(soc, scheduler, NULL, NULL, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004800u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004800u, 4u, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004830u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004830u, 4u, 0u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004a08u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004a08u, 4u, 0u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004a00u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(bus, 0x40004800u, 2u, &value, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(bus, 0x40004804u, 4u, &value, &error));
    semu_apollo4_destroy(soc);
    semu_scheduler_destroy(scheduler);
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_documented_word_and_refusals)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
