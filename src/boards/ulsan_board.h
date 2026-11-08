#ifndef SEMU_BOARDS_ULSAN_BOARD_H
#define SEMU_BOARDS_ULSAN_BOARD_H

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/scheduler.h"
#include "../devices/ulsan_clkgen.h"
#include "../devices/ulsan_disp.h"
#include "../devices/ulsan_iom2.h"
#include "../devices/ulsan_iom4.h"
#include "../devices/ulsan_motion.h"
#include "../devices/ulsan_mspi1_native.h"
#include "../devices/ulsan_pwrctrl.h"
#include "../devices/ulsan_rtc.h"
#include "../devices/ulsan_sdio.h"
#include "../devices/ulsan_stimer.h"
#include "../devices/ulsan_timer0.h"
#include "../devices/ulsan_wdt.h"

typedef struct semu_ulsan_244_board {
    semu_ulsan_pwrctrl power;
    semu_ulsan_clkgen clock;
    semu_ulsan_stimer stimer;
    semu_ulsan_wdt watchdog;
    semu_ulsan_rtc rtc;
    semu_ulsan_timer0 timer;
    semu_ulsan_iom2 iom2;
    semu_ulsan_motion motion;
    semu_ulsan_iom4 iom4;
    ulsan_mspi1_state mspi1;
    semu_ulsan_disp display;
    semu_ulsan_sdio sdio;
    uint8_t *emmc_media;
} semu_ulsan_244_board;

/* The machine owns the mutable 2.44.52 startup devices. */
semu_status semu_ulsan_244_board_map(semu_bus *bus,
                                    semu_ulsan_244_board *board,
                                    semu_scheduler *scheduler,
                                    semu_apollo4_irq_fn sink, void *context,
                                    semu_error *error);

/* Adds the evidence-backed 2.35.36 controllers to the reset memory map. */
semu_status semu_ulsan_board_map(semu_bus *bus, semu_error *error);

/* Machine-side IRQ wiring (integrator seam, E-ULS-0030): pass the
 * machine irq_sink (and, for the TIMER comparator, the machine
 * scheduler) through to the Ulsan devices that assert lines: IOM4 at
 * IRQ 10, MSPI1 at IRQ 21, TIMER1 comparator at IRQ 14 and IRQ 68.
 * Calling this from machine.c is the integrator seam for the WFI
 * wake. */
void semu_ulsan_board_attach_irq_sink(semu_apollo4_irq_fn sink,
                                      void *context,
                                      semu_scheduler *scheduler);

/* Accepts only registered Ulsan board/profile pairs. */
int semu_ulsan_board_accepted(const char *board, const char *profile_id);

#endif
