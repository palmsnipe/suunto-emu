#ifndef SEMU_BOARDS_ULSAN_BOARD_H
#define SEMU_BOARDS_ULSAN_BOARD_H

#include "semu/bus.h"

/*
 * Ulsan (Suunto Race S) 2.35.36 reset board for the Apollo4 Plus variant
 * (ticket 725). Evidence: E-ULS-0001 (component identities), E-ULS-0002
 * (bounded reference reset trace), E-ULS-0006 (boot tuple and memory map).
 *
 * The board maps exactly the proven memory classes and nothing else: every
 * MMIO block, the bootrom, and the device-specific first 256 KiB of the
 * MSPI1 XIP window stay unmapped so execution fails closed at the first
 * unsupported transaction. No Sapporo device wiring is inherited.
 */

semu_status semu_ulsan_board_map(semu_bus *bus, semu_error *error);

/* Accepts only the one evidence-eligible Ulsan profile (board plus id). */
int semu_ulsan_board_accepted(const char *board, const char *profile_id);

#endif
