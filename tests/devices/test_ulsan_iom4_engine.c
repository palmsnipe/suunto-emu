/*
 * Ulsan IOM4 doorbell data-plane tests (ticket 730, E-ULS-0029).
 *
 * The lane trace shows every boot doorbell running the DMA path: a
 * four-byte SRAM payload is loaded and written to the recorder at
 * device address 0x28, reads carry a one-byte offset select, and
 * completion auto-clears DMAEN. These tests reproduce the observed
 * write-then-read cycle byte-exactly and pin the refusal-side rules:
 * out-of-bounds targets and unobserved endpoints accept the doorbell
 * without any transfer or completion side effect, and the internal
 * status registers stay refused.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semu/bus.h"
#include "test.h"
#include "../../src/boards/ulsan_board.h"
#include "../../src/devices/ulsan_iom4.h"

#define IOM4 0x40054000u
#define BUFFER 0x10029C3Cu /* the observed guest stack payload slot */

static void wr(semu_test_context *context, semu_bus *bus, uint32_t offset,
               uint32_t value)
{
    semu_error error;
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, IOM4 + offset, 4u, value, &error));
}

static uint32_t rd(semu_test_context *context, semu_bus *bus,
                   uint32_t offset)
{
    semu_error error;
    uint32_t value = 0xdeadbeefu;
    (void)context;
    semu_error_clear(&error);
    if (semu_bus_read(bus, IOM4 + offset, 4u, &value, &error) != SEMU_OK) {
        return 0xdeadbeefu;
    }
    return value;
}

static void test_doorbell_write_read_cycle(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value;
    static const uint8_t payload[4] = { 0x7Fu, 0x01u, 0xD0u, 0xF0u };
    unsigned i;

    (void)context;
    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    for (i = 0u; i < 4u; i++) {
        semu_error_clear(&error);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_bus_write(bus, BUFFER + i, 1u, payload[i],
                                        &error));
    }

    /* Observed four-byte DMA write cycle: enable (direction
     * memory-to-device), count, target, device 0x28, doorbell. */
    wr(context, bus, 0x218u, 0x3u);
    wr(context, bus, 0x21Cu, 4u);
    wr(context, bus, 0x220u, BUFFER);
    wr(context, bus, 0x2C4u, 0x28u);
    wr(context, bus, 0x120u, 0x401u); /* write, size 4 */

    /* Wrapper CompleteDma clears DMAEN once the transfer lands; the
     * direction bit stays in the wrapper mirror (0x3 -> 0x2). */
    SEMU_TEST_EQ_U64(context, 2u, rd(context, bus, 0x218u));

    /* Observed follow-up: a one-byte read with offset 0x7F selects the
     * recorder register the write above stored 0x01 into, and the
     * result lands in the DMA target. */
    wr(context, bus, 0x218u, 0x1u);
    wr(context, bus, 0x21Cu, 1u);
    wr(context, bus, 0x220u, BUFFER + 16u);
    wr(context, bus, 0x120u, 0x7F000112u); /* read, size 1, offset 0x7F */
    semu_error_clear(&error);
    value = 0xdeadbeefu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, BUFFER + 16u, 1u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x01u, value);

    /* Reading an unwritten recorder register answers zero. */
    wr(context, bus, 0x218u, 0x1u);
    wr(context, bus, 0x220u, BUFFER + 20u);
    wr(context, bus, 0x120u, 0x36000112u); /* read, size 1, offset 0x36 */
    semu_error_clear(&error);
    value = 0xdeadbeefu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, BUFFER + 20u, 1u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
}

static void test_doorbell_refusal_paths(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0xdeadbeefu;

    (void)context;
    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* Out-of-bounds DMA target (outside SRAM and the write-direction
     * flash allowance): the lane wrapper logs DmaError, keeps DMAEN,
     * and transfers nothing. The doorbell itself still stores. */
    wr(context, bus, 0x218u, 0x3u);
    wr(context, bus, 0x21Cu, 4u);
    wr(context, bus, 0x220u, 0x20000000u);
    wr(context, bus, 0x2C4u, 0x28u);
    wr(context, bus, 0x120u, 0x401u);
    SEMU_TEST_EQ_U64(context, 0x3u, rd(context, bus, 0x218u));
    SEMU_TEST_EQ_U64(context, 0x401u, rd(context, bus, 0x120u));

    /* A doorbell to the fuel-gauge address (0x36) is beyond the
     * observed recorder traffic: no transfer, no completion. */
    wr(context, bus, 0x218u, 0x1u);
    wr(context, bus, 0x220u, BUFFER);
    wr(context, bus, 0x2C4u, 0x36u);
    wr(context, bus, 0x120u, 0x36000112u);
    SEMU_TEST_EQ_U64(context, 0x1u, rd(context, bus, 0x218u));

    /* With DMA disabled the doorbell is the plain masked register
     * store the earlier instance modelled. */
    wr(context, bus, 0x2C4u, 0x28u);
    wr(context, bus, 0x120u, 0x401u);
    SEMU_TEST_EQ_U64(context, 0x401u, rd(context, bus, 0x120u));

    /* The completion status planes stay refused (guest never reads
     * them in the observed window). */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, IOM4 + 0x204u, 4u, &value,
                                   &error) != SEMU_OK);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, IOM4 + 0x224u, 4u, &value,
                                   &error) != SEMU_OK);
}

int main(void)
{
    static const semu_test_case cases[] = {
        { "test_doorbell_write_read_cycle", test_doorbell_write_read_cycle },
        { "test_doorbell_refusal_paths", test_doorbell_refusal_paths }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
