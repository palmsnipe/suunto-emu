/*
 * Ulsan 2.35.36 display-controller identity endpoint at 0x400A0000,
 * IRQ 29 (ticket 730, E-ULS-0039).
 *
 * Mirrors the lane's lane-local class Apollo4DisplayController
 * (emulator/renode/display/Apollo4DisplayController.cs, repl
 * apollo4-display-controller-ulsan.repl "IRQ -> nvic@29"): a
 * store-through register dictionary over a 0x9000 window (unknown
 * reads answer 0, writes store), two identity words served ahead of
 * the dictionary (+0xF4 hardware id 0x87452365, +0xEC panel-ready
 * 0x77), a PLAY write at +0x00 that sets status bit 4 at +0xF8 and
 * raises the line, and a +0xF8 write without bit 4 that drops it.
 * Byte and halfword accesses translate through the aligned doubleword
 * exactly as the class's AllowedTranslations declares. Reset clears
 * the dictionary and the line, like the class.
 */
#ifndef SEMU_DEVICES_ULSAN_DISP_H
#define SEMU_DEVICES_ULSAN_DISP_H

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/frame.h"

#define SEMU_ULSAN_DISP_WORDS (0x9000u / 4u)

struct ulsan_disp_scanout;

typedef struct semu_ulsan_disp {
    uint32_t registers[SEMU_ULSAN_DISP_WORDS];
    semu_apollo4_irq_fn irq_sink;
    void *irq_context;
    unsigned line_high;
    semu_bus *bus;
    semu_frame_callback frame_callback;
    void *frame_context;
    struct ulsan_disp_scanout *scanout;
} semu_ulsan_disp;

semu_status semu_ulsan_244_disp_map(semu_bus *bus, semu_ulsan_disp *state,
                                    semu_apollo4_irq_fn irq, void *irq_context,
                                    semu_frame_callback frame,
                                    void *frame_context, semu_error *error);
void semu_ulsan_disp_destroy_state(semu_ulsan_disp *state);

semu_status semu_ulsan_disp_map(semu_bus *bus, semu_error *error);

/* Same shared seam as the IOM4 (E-ULS-0030): the integrator wires one
 * machine irq sink through semu_ulsan_board_attach_irq_sink. */
void semu_ulsan_disp_set_irq_sink(semu_apollo4_irq_fn sink, void *context);

#endif /* SEMU_DEVICES_ULSAN_DISP_H */
