#ifndef SEMU_BOARDS_ULSAN_BOARD_H
#define SEMU_BOARDS_ULSAN_BOARD_H

#include "semu/apollo4.h"
#include "semu/bus.h"

/*
 * Ulsan (Suunto Race S) 2.35.36 reset board for the Apollo4 Plus variant
 * (ticket 725). Evidence: E-ULS-0001 (component identities), E-ULS-0002
 * (bounded reference reset trace), E-ULS-0006 (boot tuple and memory map).
 *
 * The board maps exactly the proven memory classes plus the one attached
 * controller blocks (GPIO E-ULS-0007, power control E-ULS-0008, bootrom
 * stub E-ULS-0009, ticket 730). Every
 * other MMIO block, the bootrom, and the device-specific first 256 KiB of
 * the MSPI1 XIP window stay unmapped so execution fails closed at the
 * first unsupported transaction. No Sapporo device wiring is inherited.
 */

semu_status semu_ulsan_board_map(semu_bus *bus, semu_error *error);

/* Machine-side IRQ wiring (integrator seam, E-ULS-0030): pass the
 * machine irq_sink through to the Ulsan devices that assert lines -
 * today only the IOM4 command-complete line at NVIC IRQ 10. Calling
 * this is the one remaining machine.c change for the WFI wake. */
void semu_ulsan_board_attach_irq_sink(semu_apollo4_irq_fn sink,
                                      void *context);

/* Accepts only the one evidence-eligible Ulsan profile (board plus id). */
int semu_ulsan_board_accepted(const char *board, const char *profile_id);

#endif
