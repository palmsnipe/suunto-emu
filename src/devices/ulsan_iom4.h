#ifndef SEMU_DEVICES_ULSAN_IOM4_H
#define SEMU_DEVICES_ULSAN_IOM4_H

#include "semu/apollo4.h"
#include "semu/bus.h"

/* Map the Ulsan 2.35.36 IOM4 block (0x40054000, 4 KiB window). See the
 * source for the observed register and doorbell data-plane semantics
 * (ticket 730, E-ULS-0023, E-ULS-0025, E-ULS-0029). */
semu_status semu_ulsan_iom4_map(semu_bus *bus, semu_error *error);

/* Attach the machine IRQ sink for the IOM4 line (NVIC IRQ 10,
 * E-ULS-0028). The asserted level is (INTSTAT & INTEN) != 0
 * (E-ULS-0030). Until this is called the line is inert and the block
 * behaves exactly as before the interrupt plane existed. */
void semu_ulsan_iom4_set_irq_sink(semu_apollo4_irq_fn sink, void *context);

#endif /* SEMU_DEVICES_ULSAN_IOM4_H */
