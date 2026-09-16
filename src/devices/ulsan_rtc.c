/*
 * Ulsan 2.35.36 RTC block registers at 0x40004800 (ticket 730,
 * E-ULS-0019, E-ULS-0036, E-ULS-0048).
 *
 * Boot touches four RTC registers per delta pass (in-tree scratch access
 * trace through instruction 200,000,000): two reads and two stores
 * (0x0E then 0) on +0x0, two reads and a store of 0 on +0x30, a read
 * and a store of 1 on +0x200, and a store of 1 on +0x208. These four
 * are framework stores: writes store, reads answer the store, reset to
 * 0 (lane-silent at the default log level; the E-ULS-0048 full-trace
 * lane capture shows the framework handling its own registers, so the
 * E-ULS-0019 note "the lane logged no RTC lines at all" only held for
 * the default log level, superseded by E-ULS-0048 below).
 *
 * E-ULS-0036 adds the sleep-path counter window (tree refusal site pc
 * 0x0007aa66, guest seqlock 0x0007aa5c): +0x20 answers the lane's live
 * counter - BCD hundredths-of-a-second since the epoch, lane calibration
 * run lp40 (0.1 s -> 0x00000010, 0.9 s -> 0x90, 1.001 s -> 0x100, 2.0 s
 * -> 0x200, 10.0 s -> 0x1000, 11.0 s -> 0x1100; hundredths resolution
 * confirmed at 3 ms sampling), modelled as the scheduler's virtual time
 * divided by 10 ms and BCD-packed; +0x24 answers its store with reset
 * value 0x14700101, the constant the lane logged at every sample time.
 * The guest reads +0x20 twice and retries until stable; deterministic
 * virtual time keeps that loop bounded. Reads need the attached machine
 * scheduler (no scheduler: the +0x20 read refuses, fail-closed). Every
 * other RTC address and width refuses. Reset clears the stores, drops
 * the alarm line, cancels alarm events, and restarts the counter epoch
 * with the scheduler; attach survives device reset (maps detach, per
 * the E-ULS-0035 seam).
 *
 * E-ULS-0048 (IRQ 18 census) adds the two observed service-time counter
 * stores and the one-second alarm line. In the full-trace capture
 * (lp34b/lp50, three byte-identical runs) the lane RTC is
 * Timers.AmbiqApollo4_RTC at 0x40004800 wired "rtc -> nvic@2"
 * (platforms/cpus/ambiq-apollo4.repl), and it logs "First alarm set to:
 * <epoch+1 s>, alarm repeat interval: Second" once right after the boot
 * stores 0x200=1 and 0x208=1, then "Alarm occurred at: <t>; next alarm:
 * <t+1 s>" every second (3 of 3 in the 3.0 s capture), each raising the
 * line ("rtc: IRQ set" -> "External IRQ 18: True" -> wake) and dropping
 * it at the NVIC acknowledge ("Acknowledged IRQ HardwareIRQ#2" ->
 * "rtc: IRQ reset" -> False) with no RTC MMIO in between: the guest
 * never reads a status register here (tree scratch probe77: the service
 * touches only the registers below). In-tree scratch probe77 v2, run
 * twice with two passes each and byte-identical, observed the guest
 * service itself (first at vt 1,012,595,271): a store of 1 to +0x208
 * from pc 0x0009bf1e, the clock dance on +0x0 (read 0x0E, store 0x0F,
 * store 0x0E), stores +0x20=0x100 (pc 0x0009be70) and +0x24=20230101
 * (pc 0x0009beae), then the seqlock reads; the lane survives the same
 * stores every second in its steady era, so +0x20/+0x24 stores are
 * accepted here (the +0x20 store is kept but the read stays the live
 * counter of E-ULS-0036; the +0x24 store answers its read as before).
 * Observed-only alarm semantics: the store pair 0x200=1 and 0x208=1
 * (in either order) arms the alarm 1e9 ns ahead when none is pending -
 * the lane logs exactly one "First alarm" line and keeps a 1 s period -
 * while the rewrite of 0x208=1 inside every service does not reschedule
 * (that would add a ~60 us phase drift the lane log's fixed 1 s "next
 * alarm" stamps do not show); any other alarm value stores without
 * arming (unobserved). At the occurrence the model raises IRQ line 2
 * through the attached machine sink, schedules the drop 61,035 ns later
 * (the lane's momentary True->False across the acknowledge; same
 * convention as the adopted E-ULS-0035 comparator pulse), and schedules
 * the next occurrence 1e9 ns after this one, as the lane's "next alarm"
 * stamp does.
 */

#include "ulsan_rtc.h"

#include "semu/scheduler.h"
#include "semu/types.h"

#define RTC_BASE 0x40004800u
#define RTC_SIZE 0x800u

#define RTC_UPPER_RESET UINT32_C(0x14700101)
#define RTC_ALARM_VALUE UINT32_C(1)
#define RTC_ALARM_IRQ 2u /* lane repl: rtc -> nvic@2 (E-ULS-0048) */
#define RTC_ALARM_PERIOD_NS UINT64_C(1000000000)
#define RTC_PULSE_FALL_NS UINT64_C(61035)

typedef struct {
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
} rtc_state;

static rtc_state rtc_instance;

static int rtc_slot(uint32_t offset, unsigned *slot)
{
    switch (offset) {
    case 0x000u: *slot = 0u; return 1;
    case 0x030u: *slot = 1u; return 1;
    case 0x200u: *slot = 2u; return 1;
    case 0x208u: *slot = 3u; return 1;
    case 0x020u: *slot = 4u; return 1; /* live counter read, store kept */
    case 0x024u: *slot = 5u; return 1; /* counter read answers the store */
    default: return 0;
    }
}

/* Elapsed scheduler time in hundredths of a second, BCD-packed (max 8
 * digits; observed runs never reach the cap). */
static uint32_t rtc_bcd_hundredths(const semu_scheduler *scheduler)
{
    uint64_t hundredths = semu_scheduler_now(scheduler) / UINT64_C(10000000);
    uint32_t out = 0u;
    unsigned shift = 0u;
    unsigned digit;

    for (digit = 0u; digit < 8u && hundredths != 0u; ++digit) {
        out |= (uint32_t)((hundredths % 10u) << shift);
        hundredths /= 10u;
        shift += 4u;
    }
    return out;
}

static void rtc_set_line(rtc_state *s, int level)
{
    if (s->irq_sink == NULL || ((s->line_high != 0) == (level != 0))) {
        return;
    }
    s->line_high = level;
    s->irq_sink(s->irq_context, RTC_ALARM_IRQ, level);
}

static void rtc_alarm_event(void *context, uint64_t now_ns);

static void rtc_clear_event(void *context, uint64_t now_ns)
{
    rtc_state *s = (rtc_state *)context;

    (void)now_ns;
    s->clear_event_valid = 0;
    rtc_set_line(s, 0);
}

static void rtc_alarm_event(void *context, uint64_t now_ns)
{
    rtc_state *s = (rtc_state *)context;
    semu_error error;

    (void)now_ns;
    s->alarm_event_valid = 0;
    rtc_set_line(s, 1);
    if (semu_scheduler_schedule(s->scheduler, RTC_PULSE_FALL_NS,
            rtc_clear_event, s, &s->clear_event, &error) == SEMU_OK) {
        s->clear_event_valid = 1;
    }
    /* The lane schedules the repeat at the occurrence ("next alarm:
     * t+1 s"), so the period never carries the service delay. */
    if (semu_scheduler_schedule(s->scheduler, RTC_ALARM_PERIOD_NS,
            rtc_alarm_event, s, &s->alarm_event, &error) == SEMU_OK) {
        s->alarm_event_valid = 1;
    }
}

/* The observed pair 0x200=1 and 0x208=1 arms one second ahead when no
 * alarm is pending; with one pending (the lane repeats at its 1 s
 * schedule) the observed rewrite from the service leaves it running. */
static void rtc_alarm_arm(rtc_state *s)
{
    semu_error error;

    if (s->scheduler == NULL || s->alarm_event_valid ||
        s->regs[2] != RTC_ALARM_VALUE || s->regs[3] != RTC_ALARM_VALUE) {
        return;
    }
    if (semu_scheduler_schedule(s->scheduler, RTC_ALARM_PERIOD_NS,
            rtc_alarm_event, s, &s->alarm_event, &error) == SEMU_OK) {
        s->alarm_event_valid = 1;
    }
}

static semu_status rtc_read(void *context, uint32_t offset, unsigned width,
                            uint32_t *value, semu_error *error)
{
    unsigned slot = 0u;

    (void)context;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Ulsan RTC read required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || (offset & 3u) != 0u || !rtc_slot(offset, &slot)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan RTC read at 0x%08x width %u is unsupported",
                       offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    if (slot == 4u) {
        if (rtc_instance.scheduler == NULL) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "Ulsan RTC counter needs the machine scheduler");
            return SEMU_ERR_UNSUPPORTED;
        }
        *value = rtc_bcd_hundredths(rtc_instance.scheduler);
        return SEMU_OK;
    }
    *value = (slot == 5u) ? rtc_instance.upper : rtc_instance.regs[slot];
    return SEMU_OK;
}

static semu_status rtc_write(void *context, uint32_t offset, unsigned width,
                             uint32_t value, semu_error *error)
{
    unsigned slot = 0u;

    (void)context;
    if (width != 4u || (offset & 3u) != 0u || !rtc_slot(offset, &slot)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan RTC write at 0x%08x width %u is unsupported",
                       offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    if (slot == 4u) {
        rtc_instance.subsec = value; /* kept; read stays the live counter */
        return SEMU_OK;
    }
    if (slot == 5u) {
        rtc_instance.upper = value;
        return SEMU_OK;
    }
    rtc_instance.regs[slot] = value;
    rtc_alarm_arm(&rtc_instance);
    return SEMU_OK;
}

static void rtc_reset(void *context)
{
    rtc_state *s = (rtc_state *)context;

    if (s->alarm_event_valid && s->scheduler != NULL) {
        (void)semu_scheduler_cancel(s->scheduler, s->alarm_event);
    }
    if (s->clear_event_valid && s->scheduler != NULL) {
        (void)semu_scheduler_cancel(s->scheduler, s->clear_event);
    }
    s->alarm_event_valid = 0;
    s->clear_event_valid = 0;
    rtc_set_line(s, 0);
    s->regs[0] = 0u;
    s->regs[1] = 0u;
    s->regs[2] = 0u;
    s->regs[3] = 0u;
    s->upper = RTC_UPPER_RESET;
    s->subsec = 0u;
}

static const semu_bus_device_ops rtc_ops = {
    rtc_read,
    rtc_write,
    rtc_reset
};

semu_status semu_ulsan_rtc_map(semu_bus *bus, semu_error *error)
{
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Ulsan RTC needs a bus");
        return SEMU_ERR_ARGUMENT;
    }
    rtc_instance.scheduler = NULL;
    rtc_instance.irq_sink = NULL;
    rtc_instance.irq_context = NULL;
    rtc_reset(&rtc_instance);
    return semu_bus_map_device(bus, "ulsan.rtc", RTC_BASE, RTC_SIZE,
                               &rtc_ops, &rtc_instance, error);
}

void semu_ulsan_rtc_attach(semu_scheduler *scheduler,
                           semu_apollo4_irq_fn sink, void *context)
{
    /* The machine attaches after the board map: device maps reset
     * detach. Device resets keep the seams (epoch restarts with the
     * scheduler's own reset). */
    rtc_instance.scheduler = scheduler;
    rtc_instance.irq_sink = sink;
    rtc_instance.irq_context = context;
}
