#ifndef SEMU_BOARDS_WISMAR_BOARD_H
#define SEMU_BOARDS_WISMAR_BOARD_H

#include "semu/bus.h"

/*
 * Wismar (Suunto Race 2) 2.46.14 reset board (ticket 755). The reset
 * contract is E-WIS-0001: region map, start tuple, and the observed 0xf0
 * NVIC priority width. No Wismar device semantics exist yet; the shared
 * Apollo4 SoC mapping and CPU fail closed on everything unobserved.
 */

/* Accepts only the registered Wismar board/profile pair. */
int semu_wismar_board_accepted(const char *board, const char *profile_id);

/* Maps the E-WIS-0001 memory regions. The shared Apollo4 SoC attach runs
 * separately (semu_apollo4_init), exactly as the Sapporo board composes. */
semu_status semu_wismar_board_map(semu_bus *bus, semu_error *error);

#endif
