#ifndef SEMU_DEVICES_SAPPORO_RTC_H
#define SEMU_DEVICES_SAPPORO_RTC_H

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/scheduler.h"

/*
 * Sapporo-2.35.34 live RTC block at 0x40004800 (E-SAP-0035): the
 * register law mirrors the lane oracle peripheral
 * Timers.AmbiqApollo4_RTC (Control/Status/Counters/Alarms/Interrupt*,
 * WRTC gating, per-field BCD validation, 100 Hz epoch clock, RPT
 * repeat occurrences, IRQ = Enable && Status until Clear). Backed by
 * three byte-identical lane probe pairs and the in-tree access
 * census recorded in E-SAP-0035; it retires the approximate
 * E-SAP-0033/0034 alarm-pair law with that evidence.
 *
 * Bus ops for the sapporo-2.35.34 profile only: the auxiliary stub
 * dispatches here when profile selection says so, so the
 * 2.22/2.33/2.39 stub stays byte-for-byte. Reads inside the 0x210
 * window answer the law (unmodelled offsets read 0, unmodelled writes
 * drop, as on the lane); widths other than 4 and offsets at or beyond
 * 0x210 refuse fail-closed. Counter/alarm timing needs the attached
 * scheduler (no scheduler: no scheduling, reads use time 0).
 */

const semu_bus_device_ops *semu_sapporo_rtc_ops(void);

/* Same attach seam as src/devices/ulsan_rtc.c: the machine attaches
 * after the board map; device resets preserve the seams. */
void semu_sapporo_rtc_attach(semu_scheduler *scheduler,
                             semu_apollo4_irq_fn sink, void *context);

/* Machine-create detach (ulsan_rtc map-reset analogue): clears state
 * and the saved seams without touching a possibly-freed sink. */
void semu_sapporo_rtc_detach(void);

/* Read-only observation for focused tests: register value at offset
 * (exactly what a guest read answers, counter fields included);
 * pending/line state via the return and out parameter. */
uint32_t semu_sapporo_rtc_probe(uint32_t offset);
int semu_sapporo_rtc_probe_pending(int *line_high);

#endif
