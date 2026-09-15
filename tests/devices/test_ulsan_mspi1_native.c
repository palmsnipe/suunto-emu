/*
 * Ulsan MSPI1 native response machine tests (ticket 730, E-ULS-0034).
 *
 * Replays the 2.35.36 boot-trace command sequence at register level:
 * JEDEC via the return-word contract, the payload-free PIO chain
 * B7/35/06, the configuration write, the blank-sector persistence scan
 * (4-byte header probes and 64-byte records at 0x00010000+), the A5
 * sentinel re-identification, and the 64-byte persistence program with
 * its retained read-back. Refusal shapes mirror the lane: a stored
 * control value with no INTSTAT bit and no line event.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semu/bus.h"
#include "test.h"
#include "../../src/boards/ulsan_board.h"
#include "../../src/devices/ulsan_mspi1.h"

#define MSPI1 0x40061000u
#define BUF 0x10029B90u /* first traced descriptor address */
#define INTEN_MASK 0x41u  /* lane INTEN carried bits 0 and 6 here      */

typedef struct { unsigned irq; int level; } sink_record;
static sink_record events[64];
static unsigned event_count;
static unsigned expected_ops;

static void record_sink(void *context, unsigned irq, int level)
{
    (void)context;
    if (event_count < 64u) {
        events[event_count].irq = irq;
        events[event_count].level = level;
    }
    event_count++;
}

static void wr8(semu_bus *bus, uint32_t address, uint8_t value)
{
    semu_error error;
    semu_error_clear(&error);
    if (semu_bus_write(bus, address, 1u, value, &error) != SEMU_OK) {
        abort();
    }
}

static uint32_t rd8(semu_bus *bus, uint32_t address)
{
    semu_error error;
    uint32_t value = 0xdeadbeefu;
    semu_error_clear(&error);
    if (semu_bus_read(bus, address, 1u, &value, &error) != SEMU_OK) {
        return 0xdeadbeefu;
    }
    return value;
}

static void wr(semu_test_context *context, semu_bus *bus, uint32_t offset,
               uint32_t value)
{
    semu_error error;
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, MSPI1 + offset, 4u, value, &error));
}

static uint32_t rd(semu_bus *bus, uint32_t offset)
{
    semu_error error;
    uint32_t value = 0xdeaddeadu;
    semu_error_clear(&error);
    if (semu_bus_read(bus, MSPI1 + offset, 4u, &value, &error) != SEMU_OK) {
        return 0xdeaddeadu;
    }
    return value;
}

static void ram_u32(semu_bus *bus, uint32_t address, uint32_t value)
{
    unsigned index;
    for (index = 0u; index < 4u; index++) {
        wr8(bus, address + index, (uint8_t)((value >> (8u * index)) & 0xFFu));
    }
}

/* Queue arm + start. bit is the INTSTAT bit the completion must set. */
static void queue(semu_test_context *context, semu_bus *bus, uint32_t control,
                  uint32_t dev, uint32_t count, uint32_t bit)
{
    wr(context, bus, 0x10Cu, dev);
    wr(context, bus, 0x108u, BUF);
    wr(context, bus, 0x110u, count);
    wr(context, bus, 0x100u, control);
    SEMU_TEST_EQ_U64(context, bit, rd(bus, 0x204u) & bit);
    wr(context, bus, 0x208u, bit);
    expected_ops++;
}

static void pio(semu_test_context *context, semu_bus *bus, uint32_t command)
{
    wr(context, bus, 0xCu, command);
    wr(context, bus, 0x0u, 0xC1u);
    SEMU_TEST_EQ_U64(context, 1u, rd(bus, 0x204u) & 1u);
    wr(context, bus, 0x208u, 1u);
    expected_ops++;
}

static void expect_identity(semu_test_context *context, semu_bus *bus)
{
    SEMU_TEST_EQ_U64(context, 0xC2u, rd8(bus, BUF));
    SEMU_TEST_EQ_U64(context, 0x25u, rd8(bus, BUF + 1u));
    SEMU_TEST_EQ_U64(context, 0x39u, rd8(bus, BUF + 2u));
}

static void test_ulsan_mspi1_native_boot_sequence(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    unsigned index;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));
    semu_ulsan_mspi1_set_irq_sink(record_sink, NULL);
    wr(context, bus, 0x200u, INTEN_MASK);

    /* First JEDEC: the native return word selects 0x000F4543 and the
     * three destination bytes must be zero (class line 818). */
    for (index = 0u; index < 16u; index++) {
        wr8(bus, BUF + index, 0u);
    }
    ram_u32(bus, BUF + 0x2Cu, 0x000F4543u);
    queue(context, bus, 0x13u, 0u, 3u, 0x40u);
    expect_identity(context, bus);

    /* Payload-free PIO chain: enter 4-byte mode, enter QPI mode. */
    pio(context, bus, 0xB7u);
    pio(context, bus, 0x35u);

    /* Second identification now expects the QPI return word. */
    for (index = 0u; index < 16u; index++) {
        wr8(bus, BUF + index, 0u);
    }
    ram_u32(bus, BUF + 0x2Cu, 0x000F4595u);
    queue(context, bus, 0x13u, 0u, 3u, 0x40u);
    expect_identity(context, bus);

    /* Zero status read, write enable, one-byte configuration write
     * (control 0x17), then the confirming status read. */
    queue(context, bus, 0x13u, 0u, 1u, 0x40u);
    SEMU_TEST_EQ_U64(context, 0u, rd8(bus, BUF));
    pio(context, bus, 0x06u);
    wr8(bus, BUF, 0x77u);
    queue(context, bus, 0x17u, 0u, 1u, 0x40u);
    SEMU_TEST_EQ_U64(context, 0x77u, rd8(bus, BUF)); /* store, no mutate */
    queue(context, bus, 0x13u, 0u, 1u, 0x40u);
    SEMU_TEST_EQ_U64(context, 0u, rd8(bus, BUF));

    /* Blank-sector persistence scan at logical flash 0x00010000:
     * four-byte header, 64-byte record, next-slot header (off-sector
     * erased header ends the scan). */
    queue(context, bus, 0x13u, 0x00010000u, 4u, 0x40u);
    for (index = 0u; index < 4u; index++) {
        SEMU_TEST_EQ_U64(context, 0xFFu, rd8(bus, BUF + index));
    }
    queue(context, bus, 0x13u, 0x00010000u, 64u, 0x40u);
    for (index = 0u; index < 64u; index++) {
        SEMU_TEST_EQ_U64(context, 0xFFu, rd8(bus, BUF + index));
    }
    queue(context, bus, 0x13u, 0x00010040u, 4u, 0x40u);

    /* Sentinel re-identification: the A5A5A5 buffer becomes the QPI
     * identity (class line 448). */
    wr8(bus, BUF, 0xA5u);
    wr8(bus, BUF + 1u, 0xA5u);
    wr8(bus, BUF + 2u, 0xA5u);
    queue(context, bus, 0x13u, 0u, 3u, 0x40u);
    expect_identity(context, bus);

    /* Append header, then the status read that arms programming, and
     * write enable before each program. */
    queue(context, bus, 0x13u, 0x00010040u, 4u, 0x40u);
    queue(context, bus, 0x13u, 0u, 1u, 0x40u);
    pio(context, bus, 0x06u);

    /* 64-byte persistence record program at the append cursor, then a
     * retained read must show exactly the guest payload bytes. */
    for (index = 0u; index < 64u; index++) {
        wr8(bus, BUF + index, (uint8_t)(index ^ 0xA5u));
    }
    queue(context, bus, 0x17u, 0x00010040u, 64u, 0x40u);
    queue(context, bus, 0x13u, 0u, 1u, 0x40u); /* program status */
    queue(context, bus, 0x13u, 0x00010040u, 64u, 0x40u);
    for (index = 0u; index < 64u; index++) {
        SEMU_TEST_EQ_U64(context, (uint32_t)(index ^ 0xA5u),
                        rd8(bus, BUF + index));
    }

    /* Interrupt audit: every accepted operation raised the line once
     * and the INTCLR cleared it; IRQ 21 only. */
    SEMU_TEST_EQ_U64(context, (uint64_t)expected_ops * 2u,
                    (uint64_t)event_count);
    for (index = 0u; index < event_count; index++) {
        SEMU_TEST_EQ_U64(context, 21u, events[index].irq);
        SEMU_TEST_EQ_U64(context, (uint32_t)(index % 2u == 0u ? 1 : 0),
                        (uint32_t)events[index].level);
    }

    semu_bus_destroy(bus);
}

static void test_ulsan_mspi1_native_refusals_and_reset(
    semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    unsigned index;
    unsigned events_before;
    unsigned event_base;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));
    semu_ulsan_mspi1_set_irq_sink(record_sink, NULL);
    event_count = 0u;
    wr(context, bus, 0x200u, INTEN_MASK);

    /* Program count 64 while the backing store was never initialised
     * (fresh Initial stage): outside the lane queue-count contract —
     * control stores, INTSTAT bit 6 never appears, buffer untouched. */
    wr8(bus, BUF, 0x33u);
    wr(context, bus, 0x10Cu, 0x00020000u);
    wr(context, bus, 0x108u, BUF);
    wr(context, bus, 0x110u, 64u);
    wr(context, bus, 0x100u, 0x17u);
    SEMU_TEST_EQ_U64(context, 0u, rd(bus, 0x204u) & 0x40u);
    SEMU_TEST_EQ_U64(context, 0x33u, rd8(bus, BUF));
    SEMU_TEST_EQ_U64(context, 0x17u, rd(bus, 0x100u));

    /* Lane line 234 outranks the stage machine: a queue READ whose
     * device address lies in the authentic OTA window completes from
     * the mapped xip_tail bytes even in the Initial stage. */
    wr(context, bus, 0x10Cu, 0x01FF0000u);
    wr(context, bus, 0x108u, BUF);
    wr(context, bus, 0x110u, 3u);
    wr(context, bus, 0x100u, 0x13u);
    SEMU_TEST_EQ_U64(context, 0x40u, rd(bus, 0x204u) & 0x40u);
    wr(context, bus, 0x208u, 0x40u);

    /* Payload-free PIO with an unaccepted opcode: no event. */
    events_before = event_count;
    wr(context, bus, 0xCu, 0x9Fu);
    wr(context, bus, 0x0u, 0xC1u);
    SEMU_TEST_EQ_U64(context, 0u, rd(bus, 0x204u) & 1u);
    SEMU_TEST_EQ_U64(context, (uint64_t)events_before,
                    (uint64_t)event_count);

    /* Addressed erase at an unproven sector address: no event. */
    wr(context, bus, 0x8u, 0x00005000u);
    wr(context, bus, 0xCu, 0x21u);
    wr(context, bus, 0x0u, 0xE1u);
    SEMU_TEST_EQ_U64(context, 0u, rd(bus, 0x204u) & 1u);
    SEMU_TEST_EQ_U64(context, (uint64_t)events_before,
                    (uint64_t)event_count);

    /* 16-bit access width: refused. */
    {
        semu_error error_local;
        semu_error_clear(&error_local);
        SEMU_TEST_ASSERT(context, semu_bus_write(bus, MSPI1 + 0x100u, 2u,
                                                 0x13u, &error_local) !=
                                      SEMU_OK);
    }

    /* Reset after a raised line: the line drops, and the programmed
     * persistence record survives (emulated flash is non-volatile). */
    for (index = 0u; index < 64u; index++) {
        wr8(bus, BUF + index, (uint8_t)(index ^ 0x5Au));
    }
    event_base = event_count;
    wr(context, bus, 0x20Cu, 0x41u); /* INTSET both bits -> line up      */
    SEMU_TEST_EQ_U64(context, (uint64_t)(event_base + 1u),
                    (uint64_t)event_count);
    SEMU_TEST_EQ_U64(context, 1u, (uint64_t)events[event_base].level);
    semu_bus_reset(bus);
    SEMU_TEST_EQ_U64(context, (uint64_t)(event_base + 2u),
                    (uint64_t)event_count); /* drop event                */
    SEMU_TEST_EQ_U64(context, 0u, (uint64_t)events[event_base + 1u].level);
    SEMU_TEST_EQ_U64(context, 21u, (uint64_t)events[event_base + 1u].irq);
    SEMU_TEST_EQ_U64(context, 0u, rd(bus, 0x204u));

    semu_bus_destroy(bus);
}

static const semu_test_case cases[] = {
    {"boot sequence", test_ulsan_mspi1_native_boot_sequence},
    {"refusals and reset", test_ulsan_mspi1_native_refusals_and_reset},
};

int main(void)
{
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
