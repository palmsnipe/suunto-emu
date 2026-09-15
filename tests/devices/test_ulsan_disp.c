/*
 * Ulsan display-controller identity endpoint tests (ticket 730,
 * E-ULS-0039). Success cases pin the class semantics (identity words,
 * store-through dictionary, PLAY/status/IRQ side effects, translated
 * byte/halfword lanes, reset clearing); refusals pin non-doubleword
 * widths, cross-lane accesses, and the window edge.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semu/bus.h"
#include "test.h"
#include "../../src/boards/ulsan_board.h"
#include "../../src/devices/ulsan_disp.h"

#define DISP 0x400a0000u

typedef struct { unsigned irq; int level; } sink_record;
static sink_record irq_events[16];
static unsigned irq_count;

static void record_sink(void *context, unsigned irq, int level)
{
    (void)context;
    if (irq_count < 16u) {
        irq_events[irq_count].irq = irq;
        irq_events[irq_count].level = level;
    }
    irq_count++;
}

static uint32_t rd(semu_test_context *context, semu_bus *bus, uint32_t off,
                   unsigned width, semu_status *status)
{
    semu_error error;
    uint32_t value = 0xdeaddeadu;
    (void)context;
    semu_error_clear(&error);
    *status = semu_bus_read(bus, DISP + off, width, &value, &error);
    return *status == SEMU_OK ? value : 0xdeaddeadu;
}

static semu_status wr(semu_test_context *context, semu_bus *bus,
                      uint32_t off, unsigned width, uint32_t value)
{
    semu_error error;
    semu_status status;
    (void)context;
    semu_error_clear(&error);
    status = semu_bus_write(bus, DISP + off, width, value, &error);
    return status;
}

/* Identity words, store-through dictionary, and the IRQ seam exactly as
 * the lane class behaves. */
static void test_identity_dictionary_and_play_irq(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    semu_status status;
    uint32_t value;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));
    semu_ulsan_disp_set_irq_sink(record_sink, NULL);
    irq_count = 0u;

    value = rd(context, bus, 0xf4u, 4u, &status);
    SEMU_TEST_EQ_U64(context, SEMU_OK, (uint64_t)status);
    SEMU_TEST_EQ_U64(context, UINT64_C(0x87452365), value);
    value = rd(context, bus, 0xecu, 4u, &status);
    SEMU_TEST_EQ_U64(context, SEMU_OK, (uint64_t)status);
    SEMU_TEST_EQ_U64(context, UINT64_C(0x77), value);
    /* The observed epoch-1 read: DSI PHY window, dictionary answer 0. */
    value = rd(context, bus, 0x8074u, 4u, &status);
    SEMU_TEST_EQ_U64(context, SEMU_OK, (uint64_t)status);
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);

    /* Store-through dictionary. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     (uint64_t)wr(context, bus, 0x10u, 4u,
                                  UINT32_C(0xA5A5A5A5)));
    value = rd(context, bus, 0x10u, 4u, &status);
    SEMU_TEST_EQ_U64(context, UINT64_C(0xA5A5A5A5), value);
    /* Identity words are served ahead of the dictionary even when the
     * guest stores over them (class ReadDoubleWord order). */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     (uint64_t)wr(context, bus, 0xf4u, 4u, UINT32_C(1)));
    value = rd(context, bus, 0xf4u, 4u, &status);
    SEMU_TEST_EQ_U64(context, UINT64_C(0x87452365), value);

    /* PLAY write: value stored, +0xF8 bit 4 set, line raised once. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     (uint64_t)wr(context, bus, 0x00u, 4u, UINT32_C(1)));
    value = rd(context, bus, 0x00u, 4u, &status);
    SEMU_TEST_EQ_U64(context, UINT64_C(1), value);
    value = rd(context, bus, 0xf8u, 4u, &status);
    SEMU_TEST_EQ_U64(context, UINT64_C(0x10), value & 0x10u);
    SEMU_TEST_EQ_U64(context, UINT64_C(1), irq_count);
    SEMU_TEST_EQ_U64(context, UINT64_C(29), irq_events[0].irq);
    SEMU_TEST_EQ_U64(context, UINT64_C(1),
                     (uint64_t)irq_events[0].level);
    /* +0xF8 write keeping bit 4: no new edge. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     (uint64_t)wr(context, bus, 0xf8u, 4u, UINT32_C(0x10)));
    SEMU_TEST_EQ_U64(context, UINT64_C(1), irq_count);
    /* +0xF8 write without bit 4: line drops. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     (uint64_t)wr(context, bus, 0xf8u, 4u, UINT32_C(0)));
    SEMU_TEST_EQ_U64(context, UINT64_C(2), irq_count);
    SEMU_TEST_EQ_U64(context, UINT64_C(0),
                     (uint64_t)irq_events[1].level);
    semu_bus_destroy(bus);
}

/* AllowedTranslations: the engine slices merged byte/halfword lanes
 * through the aligned doubleword, effects included. */
static void test_translated_lanes_semantics(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    semu_status status;
    uint32_t value;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));
    semu_ulsan_disp_set_irq_sink(record_sink, NULL);
    irq_count = 0u;

    value = rd(context, bus, 0xf5u, 1u, &status);
    SEMU_TEST_EQ_U64(context, SEMU_OK, (uint64_t)status);
    SEMU_TEST_EQ_U64(context, UINT64_C(0x23), value); /* id byte 1    */
    value = rd(context, bus, 0xf4u, 2u, &status);
    SEMU_TEST_EQ_U64(context, SEMU_OK, (uint64_t)status);
    SEMU_TEST_EQ_U64(context, UINT64_C(0x2365), value); /* id half 0  */

    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     (uint64_t)wr(context, bus, 0x10u, 4u,
                                  UINT32_C(0xA5A5A5A5)));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     (uint64_t)wr(context, bus, 0x11u, 1u, UINT32_C(0x77)));
    value = rd(context, bus, 0x10u, 4u, &status);
    SEMU_TEST_EQ_U64(context, UINT64_C(0xA5A577A5), value);
    /* A byte write inside +0x00..+0x03 merges and fires PLAY through
     * the translated doubleword write path. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     (uint64_t)wr(context, bus, 0x01u, 1u, UINT32_C(0x42)));
    value = rd(context, bus, 0x00u, 4u, &status);
    SEMU_TEST_EQ_U64(context, UINT64_C(0x4200), value);
    value = rd(context, bus, 0xf8u, 4u, &status);
    SEMU_TEST_EQ_U64(context, UINT64_C(0x10), value & 0x10u);
    SEMU_TEST_EQ_U64(context, UINT64_C(1), irq_count);
    SEMU_TEST_EQ_U64(context, UINT64_C(1),
                     (uint64_t)irq_events[0].level);

    /* Reset clears the dictionary and drops a raised line. */
    semu_bus_reset(bus);
    value = rd(context, bus, 0x10u, 4u, &status);
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
    value = rd(context, bus, 0xf4u, 4u, &status);
    SEMU_TEST_EQ_U64(context, UINT64_C(0x87452365), value);
    SEMU_TEST_EQ_U64(context, UINT64_C(2), irq_count);
    SEMU_TEST_EQ_U64(context, UINT64_C(0),
                     (uint64_t)irq_events[1].level);
    semu_bus_destroy(bus);
}

static void test_unsupported_accesses_refused(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* Width 3 never reaches the class (engine has no such translation). */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, DISP + 0x10u, 3u, &value, &error) !=
                         SEMU_OK);
    /* Doubleword on an unaligned offset and halfword on an odd byte
     * cross the engine's translation lanes. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, DISP + 0xf5u, 4u, &value, &error) !=
                         SEMU_OK);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, DISP + 0x01u, 2u, &value, &error) !=
                         SEMU_OK);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_write(bus, DISP + 0x11u, 2u, UINT32_C(1),
                                    &error) != SEMU_OK);
    /* The class window is 0x9000; the lane falls back beyond it. */
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, 0x400a9000u, 4u, &value, &error) !=
                         SEMU_OK);
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(bus, 0x4009ffffu, 4u, &value, &error) !=
                         SEMU_OK);
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        { "test_identity_dictionary_and_play_irq",
          test_identity_dictionary_and_play_irq },
        { "test_translated_lanes_semantics",
          test_translated_lanes_semantics },
        { "test_unsupported_accesses_refused",
          test_unsupported_accesses_refused }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
