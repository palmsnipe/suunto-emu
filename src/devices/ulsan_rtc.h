#ifndef SEMU_DEVICES_ULSAN_RTC_H
#define SEMU_DEVICES_ULSAN_RTC_H

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/scheduler.h"

/*
 * Ulsan 2.35.36 RTC block at 0x40004800 (ticket 730, E-ULS-0019,
 * E-ULS-0036, E-ULS-0048): the four registers boot touches with
 * framework store semantics (writes store, reads answer the store,
 * reset to 0), the sleep-path counter window (+0x20 answers BCD
 * hundredths of scheduler virtual time and its observed service store
 * is kept without entering the live-counter read; +0x24 answers its
 * store, reset 0x14700101), and the observed one-second alarm: stores
 * 0x200=1 and 0x208=1 arm an alarm one second ahead that pulses IRQ
 * line 2 (lane repl "rtc -> nvic@2") momentarily once a second. All
 * other RTC addresses and widths refuse; see src/devices/ulsan_rtc.c
 * for the observed transaction sequence and lane calibration.
 */

typedef struct semu_ulsan_rtc {
    uint32_t regs[4]; /* +0x00, +0x30, +0x200, +0x208 */
    uint32_t upper;   /* +0x24, reset 0x14700101 */
    uint32_t subsec;  /* +0x20 store, kept; reads answer the live counter */
    semu_scheduler *scheduler; /* attached machine scheduler */
    semu_apollo4_irq_fn irq_sink;
    void *irq_context;
    semu_event_id alarm_event;
    int alarm_event_valid;
    semu_event_id clear_event;
    int clear_event_valid;
    int line_high;
} semu_ulsan_rtc;

semu_status semu_ulsan_rtc_map_state(semu_bus *bus, semu_ulsan_rtc *state,
                                     semu_scheduler *scheduler,
                                     semu_apollo4_irq_fn sink, void *context,
                                     semu_error *error);
void semu_ulsan_rtc_reset_state(semu_ulsan_rtc *state);
semu_status semu_ulsan_rtc_map(semu_bus *bus, semu_error *error);

/* Counter-window source and alarm line (ticket 730, E-ULS-0036,
 * E-ULS-0048); device reset preserves both, device maps reset detach. */
void semu_ulsan_rtc_attach(semu_scheduler *scheduler,
                           semu_apollo4_irq_fn sink, void *context);

#endif
