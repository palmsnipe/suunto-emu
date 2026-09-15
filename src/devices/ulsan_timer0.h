#ifndef SEMU_DEVICES_ULSAN_TIMER0_H
#define SEMU_DEVICES_ULSAN_TIMER0_H

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/scheduler.h"

/*
 * Ulsan 2.35.36 TIMER block at 0x40008000 (ticket 730, E-ULS-0021): the
 * registers boot touches, with the lane-silent framework store
 * semantics (writes store, reads answer the store for the four
 * read-observed registers, reset to 0), plus the E-ULS-0035 TIMER1 comparator wake:
 * an enabled control store schedules compare-period events that pulse
 * IRQ 14 and IRQ 68 through the attached machine sink. All other
 * TIMER addresses, widths, and reads of write-only-observed
 * registers refuse; see src/devices/ulsan_timer0.c for the observed
 * transaction sequence and lane calibration.
 */

semu_status semu_ulsan_timer0_map(semu_bus *bus, semu_error *error);

/* Machine seams for the comparator wake (ticket 730, E-ULS-0035);
 * device reset preserves both. */
void semu_ulsan_timer0_attach(semu_scheduler *scheduler,
                              semu_apollo4_irq_fn sink, void *context);

#endif
