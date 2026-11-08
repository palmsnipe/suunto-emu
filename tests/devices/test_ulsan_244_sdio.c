#include "semu/bus.h"
#include "test.h"
#include "../../src/devices/ulsan_sdio.h"

#include <string.h>

#define BASE 0x40070000u

static semu_status wr(semu_bus *bus, uint32_t offset, uint32_t value)
{
    semu_error error;
    return semu_bus_write(bus, BASE + offset, 4u, value, &error);
}

static uint32_t rd(semu_bus *bus, uint32_t offset)
{
    semu_error error;
    uint32_t result;
    return semu_bus_read(bus, BASE + offset, 4u, &result, &error) == SEMU_OK
        ? result : UINT32_MAX;
}

static void test_244_sdio_sector_transactions(semu_test_context *context)
{
    semu_error error;
    semu_ulsan_sdio sdio;
    semu_bus *bus = semu_bus_create(&error);
    uint8_t media[1024] = {0};
    unsigned i;

    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_ulsan_sdio_map(bus, &sdio, media, sizeof(media), &error));
    SEMU_TEST_EQ_U64(context, 0x10000u, rd(bus, 0x24u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x2cu, 0x01000001u));
    SEMU_TEST_EQ_U64(context, 3u, rd(bus, 0x2cu));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x08u, 0x1a5u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x0cu, 0x08020000u));
    SEMU_TEST_EQ_U64(context, 0x10000u, rd(bus, 0x30u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x30u, 0xffffffffu));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x08u, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x0cu, 0x01020000u));
    SEMU_TEST_EQ_U64(context, 0xc0ff8080u, rd(bus, 0x10u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x0cu, 0x02090000u));
    SEMU_TEST_EQ_U64(context, 0x25010000u, rd(bus, 0x1cu));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x08u, 0x10000u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x0cu, 0x071b0000u));
    SEMU_TEST_EQ_U64(context, 0u, rd(bus, 0x1cu));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x30u, 0xffffffffu));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x04u, 0x20200u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x08u, 1u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, wr(bus, 0x0cu, 0x123a0036u));
    SEMU_TEST_EQ_U64(context, 0u, rd(bus, 0x30u));
    SEMU_TEST_EQ_U64(context, 0x900u, rd(bus, 0x10u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, wr(bus, 0x0cu, 0xff000000u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, wr(bus, 0x20u, 1u));

    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x04u, 0x10200u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x0cu, 0x183a0002u));
    SEMU_TEST_EQ_U64(context, 0x10400u, rd(bus, 0x24u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x20u, 0xffffffffu));
    SEMU_TEST_EQ_U64(context, 0u, media[512u]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x2cu, 0x04000001u));
    SEMU_TEST_EQ_U64(context, 0u, media[512u]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x0cu, 0x183a0002u));
    for (i = 0u; i < 128u; ++i)
        SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x20u, 0x12345678u));
    SEMU_TEST_EQ_U64(context, 3u, rd(bus, 0x30u));
    SEMU_TEST_EQ_U64(context, 0x10000u, rd(bus, 0x24u));
    SEMU_TEST_EQ_U64(context, 0x78u, media[512u]);
    SEMU_TEST_EQ_U64(context, 0x12u, media[1023u]);
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x30u, 0xffffffffu));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x0cu, 0x113a0012u));
    SEMU_TEST_EQ_U64(context, 0x10800u, rd(bus, 0x24u));
    for (i = 0u; i < 128u; ++i)
        SEMU_TEST_EQ_U64(context, 0x12345678u, rd(bus, 0x20u));
    SEMU_TEST_EQ_U64(context, 3u, rd(bus, 0x30u));

    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x30u, 0xffffffffu));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x04u, 0x20200u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x08u, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(bus, 0x0cu, 0x123a0036u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, wr(bus, 0x20u, 1u));
    for (i = 0u; i < 256u; ++i)
        SEMU_TEST_EQ_U64(context, i < 128u ? 0u : 0x12345678u, rd(bus, 0x20u));
    SEMU_TEST_EQ_U64(context, 3u, rd(bus, 0x30u));

    semu_bus_reset(bus);
    SEMU_TEST_EQ_U64(context, 0x78u, media[512u]);
    SEMU_TEST_EQ_U64(context, 0u, rd(bus, 0x30u));
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_244_sdio_sector_transactions)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
