/*
 * Ulsan MSPI1 2.44.52 descriptor-shape tests (ticket 730, E-ULS-0034).
 *
 * These descriptor shapes come from the 2.44.52 profile and stay dormant
 * on the 2.35.36 boot, so the boot-sequence replay never reaches them.
 * This file pins the ported branch contract directly from the lane
 * class (SapporoApollo4Mspi1.cs lines 676-771): the Initial descriptor
 * retires into the PIO chain, the QPI repeat at return word 0x003925C2
 * returns the identity, the final zero-return descriptor completes
 * without writing, and the steady-state shape retires silently. The
 * refusals show a broken prefix and an unproven return word completing
 * nothing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semu/bus.h"
#include "test.h"
#include "../../src/boards/ulsan_board.h"
#include "../../src/devices/ulsan_mspi1.h"

#define MSPI1 0x40061000u
#define DESC 0x10029AD0u /* the 2.44.52 descriptor address */

typedef struct { unsigned irq; int level; } sink_record;
static sink_record events[16];
static unsigned event_count;

static void record_sink(void *context, unsigned irq, int level)
{
    (void)context;
    if (event_count < 16u) {
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

static void descriptor_0001(semu_bus *bus, uint32_t return_word)
{
    wr8(bus, DESC, 0x00u);
    wr8(bus, DESC + 1u, 0x01u);
    wr8(bus, DESC + 2u, 0x00u);
    wr8(bus, DESC + 3u, 0x01u);
    ram_u32(bus, DESC + 0x2Cu, return_word);
}

/* Arm and start the queue at DESC; bit6 completion is expected. */
static void retire(semu_test_context *context, semu_bus *bus)
{
    wr(context, bus, 0x10Cu, 0u);
    wr(context, bus, 0x108u, DESC);
    wr(context, bus, 0x110u, 3u);
    wr(context, bus, 0x100u, 0x13u);
    SEMU_TEST_EQ_U64(context, 0x40u, rd(bus, 0x204u) & 0x40u);
    wr(context, bus, 0x208u, 0x40u);
}

static void expect_identity(semu_test_context *context, semu_bus *bus)
{
    SEMU_TEST_EQ_U64(context, 0xC2u, rd8(bus, DESC));
    SEMU_TEST_EQ_U64(context, 0x25u, rd8(bus, DESC + 1u));
    SEMU_TEST_EQ_U64(context, 0x39u, rd8(bus, DESC + 2u));
    SEMU_TEST_EQ_U64(context, 0x01u, rd8(bus, DESC + 3u)); /* untouched  */
}

static void open_board(semu_test_context *context, semu_bus **out)
{
    semu_error error;

    semu_error_clear(&error);
    *out = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, *out != NULL);
    semu_ulsan_mspi1_set_irq_sink(record_sink, NULL);
    event_count = 0u;
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(*out, &error));
    wr(context, *out, 0x200u, 0x41u);
}

static void test_244_descriptor_lifecycle(semu_test_context *context)
{
    semu_bus *bus;

    open_board(context, &bus);

    /* Initial: the 00 01 00 01 descriptor retires with the identity
     * regardless of the return word (lane line 676). */
    descriptor_0001(bus, 0x00000000u);
    retire(context, bus);
    expect_identity(context, bus);

    /* PIO chain advances to the QPI identification stage. */
    wr(context, bus, 0xCu, 0xB7u);
    wr(context, bus, 0x0u, 0xC1u);
    wr(context, bus, 0x208u, 0x01u);
    wr(context, bus, 0xCu, 0x35u);
    wr(context, bus, 0x0u, 0xC1u);
    wr(context, bus, 0x208u, 0x01u);

    /* QPI repeat at return word 0x003925C2 re-returns the identity. */
    descriptor_0001(bus, 0x003925C2u);
    retire(context, bus);
    expect_identity(context, bus);

    /* Final zero-return descriptor completes without writing (lane
     * line 721): the 0x01 at DESC+2 is NOT rewritten here. */
    wr8(bus, DESC + 2u, 0x00u);
    descriptor_0001(bus, 0x00000000u);
    retire(context, bus);
    SEMU_TEST_EQ_U64(context, 0x00u, rd8(bus, DESC + 2u));

    /* Steady state: the same shape retires silently forever. */
    retire(context, bus);

    semu_bus_destroy(bus);
}

static void test_244_descriptor_refusals(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;

    /* Unproven return word in the QPI identification stage. */
    open_board(context, &bus);
    descriptor_0001(bus, 0x00000000u);
    retire(context, bus); /* Initial branch fires first, stage advances */
    wr(context, bus, 0xCu, 0xB7u);
    wr(context, bus, 0x0u, 0xC1u);
    wr(context, bus, 0x208u, 0x01u);
    wr(context, bus, 0xCu, 0x35u);
    wr(context, bus, 0x0u, 0xC1u);
    wr(context, bus, 0x208u, 0x01u);
    descriptor_0001(bus, 0xDEADBEEFu); /* not the observed QPI word     */
    wr(context, bus, 0x10Cu, 0u);
    wr(context, bus, 0x108u, DESC);
    wr(context, bus, 0x110u, 3u);
    wr(context, bus, 0x100u, 0x13u);
    SEMU_TEST_EQ_U64(context, 0u, rd(bus, 0x204u) & 0x40u);
    semu_bus_destroy(bus);

    /* Broken prefix at the descriptor address: neither the 2.44.52
     * shape nor the return-word JEDEC applies - nothing completes. */
    open_board(context, &bus);
    wr8(bus, DESC, 0x00u);
    wr8(bus, DESC + 1u, 0x02u); /* not 00 01 00 01                      */
    wr8(bus, DESC + 2u, 0x00u);
    wr8(bus, DESC + 3u, 0x01u);
    ram_u32(bus, DESC + 0x2Cu, 0x00000000u);
    wr(context, bus, 0x10Cu, 0u);
    wr(context, bus, 0x108u, DESC);
    wr(context, bus, 0x110u, 3u);
    wr(context, bus, 0x100u, 0x13u);
    SEMU_TEST_EQ_U64(context, 0u, rd(bus, 0x204u) & 0x40u);
    SEMU_TEST_EQ_U64(context, 0x13u, rd(bus, 0x100u)); /* store stands   */
    (void)error;

    semu_bus_destroy(bus);
}

static const semu_test_case cases[] = {
    {"244 descriptor lifecycle", test_244_descriptor_lifecycle},
    {"244 descriptor refusals", test_244_descriptor_refusals},
};

int main(void)
{
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
