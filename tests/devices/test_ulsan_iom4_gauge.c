/*
 * Ulsan IOM4 fuel-gauge endpoint tests (ticket 730, E-ULS-0032).
 *
 * The lane trace shows only size-2 gauge reads behind the doorbell
 * (DEVADDR 0x36) with a one-byte offset select, and the lane gauge
 * class logs the exact returned words: 0x00=0x0000, 0x06=0x3200,
 * 0x09=0xC000, 0x19=0xC000, and the polled undefined register
 * 0x21=0x0000 fifteen times. These tests replay that contract through
 * the engine and add the class-source write path plus the SRAM-boundary
 * refusal.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semu/bus.h"
#include "test.h"
#include "../../src/boards/ulsan_board.h"
#include "../../src/devices/ulsan_iom4.h"

#define IOM4 0x40054000u
#define GAUGE_BUFFER 0x10029C80u

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

/* Arm a gauge read: DEVADDR 0x36, DMA enable read-direction, count,
 * target; doorbell cmd 2, size 2, OFFSETEN, OFFSETLO in bits 31:24. */
static void gauge_read(semu_test_context *context, semu_bus *bus,
                       uint32_t register_offset, uint32_t target)
{
    wr(context, bus, 0x2C4u, 0x36u);
    wr(context, bus, 0x218u, 0x1u);
    wr(context, bus, 0x21Cu, 2u);
    wr(context, bus, 0x220u, target);
    wr(context, bus, 0x120u, 0x212u | (register_offset << 24));
}

static void expect_gauge_word(semu_test_context *context, semu_bus *bus,
                              uint32_t register_offset, uint16_t expected)
{
    semu_error error;
    uint32_t low = 0xdeadbeefu;
    uint32_t high = 0xdeadbeefu;

    gauge_read(context, bus, register_offset, GAUGE_BUFFER);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, GAUGE_BUFFER, 1u, &low, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, GAUGE_BUFFER + 1u, 1u, &high,
                                   &error));
    /* Little-endian byte pair exactly as the lane class returns it. */
    SEMU_TEST_EQ_U64(context, (uint32_t)(expected & 0xFFu), low);
    SEMU_TEST_EQ_U64(context, (uint32_t)(expected >> 8), high);
}

static void test_gauge_observed_reads(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    unsigned poll;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* Every observed word from the boot trace, in trace order. */
    expect_gauge_word(context, bus, 0x00u, 0x0000u);
    expect_gauge_word(context, bus, 0x06u, 0x3200u);
    expect_gauge_word(context, bus, 0x09u, 0xC000u);
    expect_gauge_word(context, bus, 0x19u, 0xC000u);
    for (poll = 0u; poll < 15u; poll++) {
        expect_gauge_word(context, bus, 0x21u, 0x0000u);
    }
    /* The class installs 0x1900 in the temperature register; reading it
     * is source truth even though the boot window never polled it. */
    expect_gauge_word(context, bus, 0x08u, 0x1900u);

    /* Each transaction completes: the read-direction DMAEN bit clears
     * (wrapper CompleteDma) and the command stays stored. */
    SEMU_TEST_EQ_U64(context, 0u, rd(context, bus, 0x218u));

    semu_bus_destroy(bus);
}

static void test_gauge_write_and_boundary_refusal(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    static const uint8_t payload[3] = { 0x06u, 0x11u, 0x22u };
    unsigned i;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* Class-source write path: first byte is the register pointer, the
     * rest are byte pairs, pointer advances after each completed pair
     * (no gauge writes occur in the observed window; this mirrors the
     * cited class exactly). */
    for (i = 0u; i < 3u; i++) {
        semu_error_clear(&error);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_bus_write(bus, GAUGE_BUFFER + i, 1u,
                                        payload[i], &error));
    }
    wr(context, bus, 0x2C4u, 0x36u);
    wr(context, bus, 0x218u, 0x3u);
    wr(context, bus, 0x21Cu, 3u);
    wr(context, bus, 0x220u, GAUGE_BUFFER);
    wr(context, bus, 0x120u, 0x301u | (3u << 8)); /* write, size 3 */
    expect_gauge_word(context, bus, 0x06u, 0x2211u);

    /* SRAM-boundary refusal: a 16-byte transfer that would run past
     * 0x10267000 (SIZE field bits 19:8) leaves DMAEN armed and
     * completes nothing. */
    wr(context, bus, 0x218u, 0x1u);
    wr(context, bus, 0x21Cu, 16u);
    wr(context, bus, 0x220u, 0x10266FF8u);
    wr(context, bus, 0x120u, 0x1012u | (0x06u << 24));
    SEMU_TEST_EQ_U64(context, 0x1u, rd(context, bus, 0x218u));

    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        { "test_gauge_observed_reads", test_gauge_observed_reads },
        { "test_gauge_write_and_boundary_refusal",
          test_gauge_write_and_boundary_refusal }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
