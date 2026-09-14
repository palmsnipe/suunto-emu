/*
 * Ulsan MSPI1 register-plane and interrupt-seam tests (ticket 730,
 * E-ULS-0031/0033). Queue-start control values store like the lane's
 * register dictionary and population REFUSES (stage machine not yet
 * ported), so completion never fires; the interrupt plane is exercised
 * through INTSET/INTCLR, which the lane class exposes identically.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semu/bus.h"
#include "test.h"
#include "../../src/boards/ulsan_board.h"
#include "../../src/devices/ulsan_mspi1.h"

#define MSPI1 0x40061000u

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

static uint32_t rd(semu_test_context *context, semu_bus *bus, uint32_t off)
{
    semu_error error;
    uint32_t value = 0xdeaddeadu;
    (void)context;
    semu_error_clear(&error);
    if (semu_bus_read(bus, MSPI1 + off, 4u, &value, &error) != SEMU_OK) {
        return 0xdeaddeadu;
    }
    return value;
}

static int wr(semu_bus *bus, uint32_t off, uint32_t value)
{
    semu_error error;
    semu_error_clear(&error);
    return (int)semu_bus_write(bus, MSPI1 + off, 4u, value, &error);
}

static void test_queue_registers_and_refusal_semantics(
    semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* Queue registers store and read back like the lane dictionary. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, (uint32_t)wr(bus, 0x108u, 0x10029B90u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, (uint32_t)wr(bus, 0x10Cu, 0x01FF0000u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, (uint32_t)wr(bus, 0x110u, 3u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, (uint32_t)wr(bus, 0x100u, 0x13u));
    SEMU_TEST_EQ_U64(context, 0x10029B90u, rd(context, bus, 0x108u));
    SEMU_TEST_EQ_U64(context, 0x01FF0000u, rd(context, bus, 0x10Cu));
    SEMU_TEST_EQ_U64(context, 3u, rd(context, bus, 0x110u));
    SEMU_TEST_EQ_U64(context, 0x13u, rd(context, bus, 0x100u));

    /* Start accepted into the store but population refuses (E-ULS-0033):
     * INTSTAT stays clear - the completion bit only lands with a proven
     * response branch. */
    SEMU_TEST_EQ_U64(context, 0u, rd(context, bus, 0x204u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, (uint32_t)wr(bus, 0x100u, 0x17u));
    SEMU_TEST_EQ_U64(context, 0x17u, rd(context, bus, 0x100u));
    SEMU_TEST_EQ_U64(context, 0u, rd(context, bus, 0x204u));

    /* Untraced offsets refuse: PIO-plane registers (the Sapporo DIAP4
     * path, absent from Ulsan traces) and unknown interrupt offsets. */
    SEMU_TEST_ASSERT(context, wr(bus, 0x0u, 0xC1u) != SEMU_OK);
    SEMU_TEST_ASSERT(context, wr(bus, 0x210u, 1u) != SEMU_OK);
    SEMU_TEST_ASSERT(context, wr(bus, 0x1000u, 1u) != SEMU_OK);

    semu_bus_destroy(bus);
}

static void test_interrupt_plane_and_seam(semu_test_context *context)
{
    semu_bus *bus;
    semu_error error;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_ulsan_board_attach_irq_sink(record_sink, NULL);
    irq_count = 0u;
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));

    /* Enable alone never raises; INTSET lands the queue-complete bit
     * and the edge-only line fires as IRQ 21 high. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, (uint32_t)wr(bus, 0x200u, 0x40u));
    SEMU_TEST_EQ_U64(context, 0u, (uint32_t)irq_count);
    SEMU_TEST_EQ_U64(context, SEMU_OK, (uint32_t)wr(bus, 0x20Cu, 0x40u));
    SEMU_TEST_EQ_U64(context, 1u, (uint32_t)irq_count);
    SEMU_TEST_EQ_U64(context, 21u, irq_events[0].irq);
    SEMU_TEST_EQ_U64(context, 1, (uint32_t)irq_events[0].level);

    /* INTSTAT readable; direct INTSTAT writes refuse; INTCLR W1C
     * drops the line (lane ISR acknowledge shape). */
    SEMU_TEST_EQ_U64(context, 0x40u, rd(context, bus, 0x204u));
    SEMU_TEST_ASSERT(context, wr(bus, 0x204u, 0u) != SEMU_OK);
    SEMU_TEST_EQ_U64(context, SEMU_OK, (uint32_t)wr(bus, 0x208u, 0x40u));
    SEMU_TEST_EQ_U64(context, 2u, (uint32_t)irq_count);
    SEMU_TEST_EQ_U64(context, 21u, irq_events[1].irq);
    SEMU_TEST_EQ_U64(context, 0, (uint32_t)irq_events[1].level);
    SEMU_TEST_EQ_U64(context, 0u, rd(context, bus, 0x204u));

    /* Reset drops a raised line and clears the plane. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, (uint32_t)wr(bus, 0x20Cu, 0x40u));
    SEMU_TEST_EQ_U64(context, 3u, (uint32_t)irq_count);
    semu_bus_reset(bus);
    SEMU_TEST_EQ_U64(context, 4u, (uint32_t)irq_count);
    SEMU_TEST_EQ_U64(context, 0, (uint32_t)irq_events[3].level);
    SEMU_TEST_EQ_U64(context, 0u, rd(context, bus, 0x204u));
    SEMU_TEST_EQ_U64(context, 0u, rd(context, bus, 0x200u));

    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        { "test_queue_registers_and_refusal_semantics",
          test_queue_registers_and_refusal_semantics },
        { "test_interrupt_plane_and_seam", test_interrupt_plane_and_seam }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
