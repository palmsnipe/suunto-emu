/*
 * Ulsan MSPI1 command-queue controller at 0x40061000, IRQ 21
 * (ticket 730, E-ULS-0031). Register plane and interrupt seam; the
 * stage-gated response population refuses until its own instance
 * (E-ULS-0033).
 */
#ifndef SEMU_DEVICES_ULSAN_MSPI1_H
#define SEMU_DEVICES_ULSAN_MSPI1_H

#include "semu/apollo4.h"
#include "semu/bus.h"

semu_status semu_ulsan_mspi1_map(semu_bus *bus, semu_error *error);

/* Same shared seam as the IOM4 (E-ULS-0030): the integrator wires one
 * machine irq sink through semu_ulsan_board_attach_irq_sink. */
void semu_ulsan_mspi1_set_irq_sink(semu_apollo4_irq_fn sink, void *context);

#endif /* SEMU_DEVICES_ULSAN_MSPI1_H */
