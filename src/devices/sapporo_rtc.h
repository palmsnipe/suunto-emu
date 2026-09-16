#ifndef SEMU_DEVICES_SAPPORO_RTC_H
#define SEMU_DEVICES_SAPPORO_RTC_H

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/scheduler.h"

/*
 * Sapporo 2.35.34 RTC block semantics at 0x40004800 (ticket 710
 * instance-3, E-SAP-0032; law source E-ULS-0048/E-ULS-0036): the live
 * one-second RTC alarm the startup path arms with the stores
 * +0x208=1 and +0x200=1 before WFI-parking (E-SAP-0032 lane probe),
 * pulsing IRQ line 2 (lane repl "rtc -> nvic@2") momentarily once a
 * second, repeat scheduled at each occurrence. Reads at +0x20 answer
 * the scheduler's virtual time as BCD hundredths (guest seqlock retry
 * reads +0x20 twice); +0x24 answers the constant 0 the stub answers.
 * Bus ops for the sapporo-2.35.34 profile only: the auxiliary stub
 * dispatches here when the SoC profile selection says so, so the
 * 2.22/2.33/2.39 stub stays byte-for-byte. The accepted offset/width
 * set is exactly the stub's (E-SAP-0032 census offsets); service-time
 * stores beyond that set refuse, fail-closed, and the refusal site is
 * the recorded next stop. Counter reads need the attached scheduler
 * (no scheduler: refuse).
 */

const semu_bus_device_ops *semu_sapporo_rtc_ops(void);

/* Same attach seam as src/devices/ulsan_rtc.c: the machine attaches
 * after the board map; device resets preserve the seams. */
void semu_sapporo_rtc_attach(semu_scheduler *scheduler,
                             semu_apollo4_irq_fn sink, void *context);

/* Machine-create detach (ulsan_rtc map-reset analogue): clears state
 * and the saved seams without touching a possibly-freed sink. */
void semu_sapporo_rtc_detach(void);

#endif
