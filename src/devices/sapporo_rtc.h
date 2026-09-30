#ifndef SEMU_DEVICES_SAPPORO_RTC_H
#define SEMU_DEVICES_SAPPORO_RTC_H

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/scheduler.h"
#include "../core/scheduler_internal.h"
#include "../core/snapshot_io.h"

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
 * 0x210 refuse fail-closed. Counter/alarm timing uses the owning SoC's
 * scheduler and never another machine's clock or interrupt sink.
 */

const semu_bus_device_ops *semu_sapporo_rtc_ops(void);

typedef struct semu_sapporo_rtc semu_sapporo_rtc;

/* SoC-owned instance. Bus callbacks receive this object as their context.
 * The scheduler must outlive it; destroy cancels only this owner's event.
 * Reset preserves the scheduler and IRQ bindings. */
semu_sapporo_rtc *semu_sapporo_rtc_create(semu_scheduler *scheduler,
    semu_apollo4_irq_fn sink, void *context, semu_error *error);
void semu_sapporo_rtc_destroy(semu_sapporo_rtc *rtc);

/* Read-only observation for focused tests: register value at offset
 * (exactly what a guest read answers, counter fields included);
 * pending/line state via the return and out parameter. */
uint32_t semu_sapporo_rtc_probe(semu_sapporo_rtc *rtc, uint32_t offset);
int semu_sapporo_rtc_probe_pending(const semu_sapporo_rtc *rtc, int *line_high);

/* Ticket 792: v2 snapshot codec for the live 2.35 RTC (E-SAP-0035 state).
 * Round-trips only state the running model exposes; bindings (scheduler,
 * IRQ sink) stay with the instance. The pending one-second alarm keeps its
 * scheduler identity: resolve/match serve the tagged alarm event. */
semu_status semu_sapporo_rtc_snapshot_write(
    const semu_sapporo_rtc *rtc, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_sapporo_rtc_snapshot_read(
    semu_sapporo_rtc *rtc, semu_snapshot_reader *reader, semu_error *error);
semu_status semu_sapporo_rtc_snapshot_resolve_event(
    semu_sapporo_rtc *rtc, uint32_t subject,
    semu_event_callback *callback, void **context, semu_error *error);
semu_status semu_sapporo_rtc_snapshot_event_id_matches(
    const semu_sapporo_rtc *rtc, uint32_t subject, semu_event_id event_id,
    semu_error *error);
semu_status semu_sapporo_rtc_snapshot_event_links_match(
    const semu_sapporo_rtc *rtc, const semu_scheduled_event_state *events,
    size_t count, semu_error *error);

#endif
