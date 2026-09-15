/*
 * Ulsan TIMER comparator wake tests (ticket 730, E-ULS-0035).
 *
 * Success case: with the lane wiring attached the compare events pulse
 * the combined (IRQ 14) and comparator (IRQ 68) lines once per
 * CMP0-period and stop when the enable bit clears. Refusal cases for the
 * register window live in test_ulsan_timer0.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/scheduler.h"
#include "test.h"
#include "../../src/boards/ulsan_board.h"
#include "../../src/devices/ulsan_timer0.h"

/*
 * Comparator wake (E-ULS-0035): with the lane wiring attached (combined
 * line IRQ 14, comparator line IRQ 68) the 0x220 enable store arms a
 * compare event every CMP0 x 61035 ns (GSTATCLK/2 = 16.384 kHz); each
 * compare drives both lines high for one 61035 ns tick and re-arms, so
 * pulses repeat while enabled (lane log: 511 pulses over 3.3 s, with
 * the guest rewriting the compare adaptively). Clearing the enable bit
 * cancels the next compare; the in-flight tick still drops the lines.
 * This test arms with CMP0 0x20 as the tree's first boot compare store
 * does, so the period is 32 x 61035 ns.
 */
static unsigned g_edge_irq[16];
static unsigned g_edge_level[16];
static size_t g_edge_count;

static void wake_sink(void *context, unsigned irq, int level)
{
    (void)context;
    if (g_edge_count < 16u) {
        g_edge_irq[g_edge_count] = irq;
        g_edge_level[g_edge_count] = level != 0 ? 1u : 0u;
    }
    ++g_edge_count;
}

static void test_compare_wake_edges_and_cancel(semu_test_context *context)
{
    semu_scheduler *scheduler;
    semu_bus *bus;
    semu_error error;
    const uint64_t period_ns = UINT64_C(32) * UINT64_C(61035);
    const unsigned expect_irq[8] = {14u, 68u, 14u, 68u, 14u, 68u, 14u, 68u};
    const unsigned expect_level[8] = {1u, 1u, 0u, 0u, 1u, 1u, 0u, 0u};
    size_t index;

    g_edge_count = 0u;
    semu_error_clear(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_board_map(bus, &error));
    /* The machine attaches after mapping: device maps reset detach. */
    semu_ulsan_timer0_attach(scheduler, wake_sink, NULL);

    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40008228u, 4u, UINT32_C(0x20),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40008220u, 4u, UINT32_C(0xA21),
                                    &error));
    /* No edge one tick before the first compare. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, period_ns - 1u,
                                            &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), g_edge_count);
    /* Compare raises both lines; one tick later they drop. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(1), &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(2), g_edge_count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler,
                                            UINT64_C(61035) - 1u, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(2), g_edge_count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(1), &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(4), g_edge_count);
    /* The counter wraps at compare: the pulse repeats one period later. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler,
                                            period_ns - UINT64_C(61036),
                                            &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(4), g_edge_count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(1), &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(6), g_edge_count);
    /* Clearing the enable bit stops the next compare; the open tick still
     * drops the lines and nothing follows. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40008220u, 4u, UINT32_C(0xA22),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(61035),
                                            &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(8), g_edge_count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, period_ns * 3u,
                                            &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(8), g_edge_count);
    for (index = 0u; index < 8u; ++index) {
        SEMU_TEST_EQ_U64(context, expect_irq[index], g_edge_irq[index]);
        SEMU_TEST_EQ_U64(context, expect_level[index],
                         g_edge_level[index]);
    }
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}


int main(void)
{
    static const semu_test_case cases[] = {
        { "test_compare_wake_edges_and_cancel",
          test_compare_wake_edges_and_cancel }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
