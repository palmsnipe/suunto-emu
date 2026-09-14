/*
 * Ulsan IOM4 reset semantics test (ticket 730, E-ULS-0023/0025/0026).
 * Companion of test_ulsan_iom4.c: every stored register returns to its
 * reset read value after semu_bus_reset - plain stores to 0, +0x11C to
 * the 0xE20 submodule-type enum bits, +0x248 to the IDLEST bit alone,
 * and +0x280 keeps its 0x00200000 reset constant.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semu/bus.h"
#include "test.h"
#include "../../src/boards/ulsan_board.h"
#include "../../src/devices/ulsan_iom4.h"

#define IOM4 0x40054000u

static void test_stored_registers_clear_on_reset(semu_test_context *context)
{
    static const uint32_t stores[][2] = {
        { 0x104u, UINT32_C(0x00001010) }, { 0x118u, UINT32_C(0x1D0E1301) },
        { 0x11Cu, UINT32_C(0x00000010) }, { 0x200u, UINT32_C(0x00007FFF) },
        { 0x210u, UINT32_C(0x00000003) }, { 0x248u, UINT32_C(0x00000001) },
        { 0x120u, UINT32_C(0x00000401) }, { 0x124u, UINT32_C(0x00000010) },
        { 0x128u, UINT32_C(0x00000000) }, { 0x218u, UINT32_C(0x00000303) },
        { 0x21Cu, UINT32_C(0x00000004) }, { 0x220u, UINT32_C(0x10001000) },
        { 0x2C0u, UINT32_C(0x0103F270) }, { 0x2C4u, UINT32_C(0x00000028) }
    };
    static const uint32_t reset_reads[][2] = {
        { 0x104u, UINT32_C(0) }, { 0x118u, UINT32_C(0) },
        { 0x11Cu, UINT32_C(0x00000E20) }, { 0x200u, UINT32_C(0) },
        { 0x210u, UINT32_C(0) }, { 0x248u, UINT32_C(0x00000004) },
        { 0x120u, UINT32_C(0) }, { 0x124u, UINT32_C(0) },
        { 0x128u, UINT32_C(0) }, { 0x218u, UINT32_C(0) },
        { 0x21Cu, UINT32_C(0) }, { 0x220u, UINT32_C(0) },
        { 0x2C0u, UINT32_C(0) }, { 0x2C4u, UINT32_C(0) },
        { 0x280u, UINT32_C(0x00200000) }
    };
    semu_bus *bus;
    semu_error error;
    size_t index;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));
    for (index = 0u; index < sizeof(stores) / sizeof(stores[0]); ++index) {
        semu_error_clear(&error);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(
            bus, IOM4 + stores[index][0], 4u, stores[index][1], &error));
    }
    semu_bus_reset(bus);
    for (index = 0u; index < sizeof(reset_reads) / sizeof(
             reset_reads[0]); ++index) {
        uint32_t value = 9u;
        semu_error_clear(&error);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(
            bus, IOM4 + reset_reads[index][0], 4u, &value, &error));
        SEMU_TEST_EQ_U64(context, reset_reads[index][1], value);
    }
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        { "test_stored_registers_clear_on_reset",
          test_stored_registers_clear_on_reset }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
