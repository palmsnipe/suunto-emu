/*
 * Sapporo 2.35.34 live RTC alarm at 0x40004800 (ticket 710 instance-3,
 * E-SAP-0032). The E-SAP-0032 lane probe pair recorded the startup path
 * issuing two byte-identical 13-transaction blocks per run: reads and
 * stores on +0x00 (0, then 0xE), reads and a store of 0 on +0x30, the
 * alarm-load store pair +0x208=1 and +0x200=1, then +0x20/+0x24 counter
 * reads (guest seqlock), and the WFI park at 0x000e1862 waiting for the
 * IRQ 2 alarm (lane platform repl "rtc -> nvic@2") that neither the
 * upstream lane class nor the auxiliary stub ever raised.
 *
 * The firing law is the E-ULS-0048 one-second alarm established on the
 * Ulsan lane for this same Apollo4 RTC block (forked-class census,
 * three byte-identical captures, plus the Apollo4 RTC architectural
 * reference): the pair 0x200=1 and 0x208=1 (either order) arms 1e9 ns
 * ahead when none is pending; the occurrence raises line 2, the drop is
 * scheduled 61,035 ns later (acknowledge-side fall convention of
 * E-ULS-0048/E-ULS-0035), and the next occurrence is scheduled 1e9 ns
 * after this one. A rewrite of the pair while pending does not
 * reschedule. Any other alarm value stores without arming.
 * E-SAP-0032 states explicitly that the Sapporo lane never fired this
 * alarm, so this carry is a same-SoC-block law adoption, not a
 * Sapporo-observed firing.
 *
 * Accepted accesses are exactly the auxiliary-stub set (E-SAP-0032
 * census offsets): reads at +0x00/+0x20/+0x24/+0x30/+0x200/+0x208,
 * writes at +0x00/+0x30/+0x200/+0x208, all 32-bit aligned. +0x00,
 * +0x30, +0x200, +0x208 are framework stores (write stores, read
 * answers, reset 0); +0x20 reads the live counter, BCD hundredths of
 * scheduler virtual time (E-ULS-0036 law; a store there is refused
 * here because no Sapporo store at that offset was ever observed);
 * +0x24 answers the constant 0 the 2.22-era stub pinned. Everything
 * else refuses. Reset cancels events, drops the line, clears stores.
 */

#include "sapporo_rtc.h"

#include "semu/types.h"

#define RTC_ALARM_IRQ 2u /* lane repl: rtc -> nvic@2 (E-SAP-0032) */
#define RTC_ALARM_PERIOD_NS UINT64_C(1000000000)
#define RTC_PULSE_FALL_NS UINT64_C(61035)
#define RTC_ALARM_VALUE UINT32_C(1)

typedef struct {
    uint32_t regs[4]; /* +0x00, +0x30, +0x200, +0x208 */
    semu_scheduler *scheduler;
    semu_apollo4_irq_fn irq_sink;
    void *irq_context;
    semu_event_id alarm_event;
    int alarm_event_valid;
    semu_event_id clear_event;
    int clear_event_valid;
    int line_high;
    uint64_t now_high_water; /* max scheduler now observed (E-SAP-0034) */
} sapporo_rtc_state;

static sapporo_rtc_state rtc_instance;

static int rtc_slot_read(uint32_t offset, unsigned *slot)
{
    switch (offset) {
    case 0x000u: *slot = 0u; return 1;
    case 0x020u: *slot = 4u; return 1; /* live BCD counter */
    case 0x024u: *slot = 5u; return 1; /* answers constant 0 */
    case 0x030u: *slot = 1u; return 1;
    case 0x200u: *slot = 2u; return 1;
    case 0x208u: *slot = 3u; return 1;
    default: return 0;
    }
}

static int rtc_slot_write(uint32_t offset, unsigned *slot)
{
    switch (offset) {
    case 0x000u: *slot = 0u; return 1;
    case 0x030u: *slot = 1u; return 1;
    case 0x200u: *slot = 2u; return 1;
    case 0x208u: *slot = 3u; return 1;
    default: return 0;
    }
}

/* Elapsed scheduler time in hundredths of a second, BCD-packed
 * (max 8 digits; observed runs never reach the cap). */
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

static void rtc_set_line(sapporo_rtc_state *s, int level)
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
    sapporo_rtc_state *s = (sapporo_rtc_state *)context;

    (void)now_ns;
    s->clear_event_valid = 0;
    rtc_set_line(s, 0);
}

static void rtc_alarm_event(void *context, uint64_t now_ns)
{
    sapporo_rtc_state *s = (sapporo_rtc_state *)context;
    semu_error error;

    if (now_ns > s->now_high_water) {
        s->now_high_water = now_ns;
    }
    s->alarm_event_valid = 0;
    rtc_set_line(s, 1);
    if (semu_scheduler_schedule(s->scheduler, RTC_PULSE_FALL_NS,
            rtc_clear_event, s, &s->clear_event, &error) == SEMU_OK) {
        s->clear_event_valid = 1;
    }
    /* The E-ULS-0048 lane repeats at the occurrence, so the period
     * never carries the service delay. */
    if (semu_scheduler_schedule(s->scheduler, RTC_ALARM_PERIOD_NS,
            rtc_alarm_event, s, &s->alarm_event, &error) == SEMU_OK) {
        s->alarm_event_valid = 1;
    }
}

static void rtc_alarm_arm(sapporo_rtc_state *s)
{
    semu_error error;
    uint64_t now;

    if (s->scheduler == NULL) {
        return;
    }
    now = semu_scheduler_now(s->scheduler);
    if (now < s->now_high_water) {
        /* The engine's software-reset path (machine reset with SRAM
         * retention, E-SAP-0034) zeroes the scheduler and drops every
         * pending event without a device bus reset. A regressed clock
         * therefore proves our recorded ids are stale, not pending:
         * drop them (and the possibly stuck line) so the guest's
         * post-reset re-arm of this re-initialised boot can take. */
        s->alarm_event_valid = 0;
        s->clear_event_valid = 0;
        rtc_set_line(s, 0);
    }
    if (now > s->now_high_water) {
        s->now_high_water = now;
    }
    if (s->alarm_event_valid ||
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
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Sapporo RTC read required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || (offset & 3u) != 0u || !rtc_slot_read(offset, &slot)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Sapporo RTC read at 0x%08x width %u is unsupported",
                       offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    if (slot == 4u) {
        if (rtc_instance.scheduler == NULL) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "Sapporo RTC counter needs the machine scheduler");
            return SEMU_ERR_UNSUPPORTED;
        }
        *value = rtc_bcd_hundredths(rtc_instance.scheduler);
        semu_error_clear(error);
        return SEMU_OK;
    }
    if (slot == 5u) {
        *value = 0u;
        semu_error_clear(error);
        return SEMU_OK;
    }
    *value = rtc_instance.regs[slot];
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status rtc_write(void *context, uint32_t offset, unsigned width,
                             uint32_t value, semu_error *error)
{
    unsigned slot = 0u;

    (void)context;
    if (width != 4u || (offset & 3u) != 0u || !rtc_slot_write(offset, &slot)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Sapporo RTC write at 0x%08x width %u is unsupported",
                       offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    rtc_instance.regs[slot] = value;
    rtc_alarm_arm(&rtc_instance);
    semu_error_clear(error);
    return SEMU_OK;
}

static void rtc_reset(void *context)
{
    /* Dispatched from the auxiliary stub with the SoC as bus context;
     * state lives in the module instance like the read/write ops. */
    sapporo_rtc_state *s = &rtc_instance;

    (void)context;
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
    s->now_high_water = 0u;
}

static const semu_bus_device_ops rtc_ops = {
    rtc_read, rtc_write, rtc_reset
};

const semu_bus_device_ops *semu_sapporo_rtc_ops(void)
{
    return &rtc_ops;
}

void semu_sapporo_rtc_detach(void)
{
    /* Called from the SoC auxiliary map at every machine create (the
     * ulsan_rtc map-reset analogue): the previous machine's scheduler
     * may already be freed, so the seams clear first and any pending
     * event ids are dropped without a cancel - they die with their own
     * scheduler, and the new machine's attach starts fresh. */
    rtc_instance.scheduler = NULL;
    rtc_instance.irq_sink = NULL;
    rtc_instance.irq_context = NULL;
    rtc_instance.alarm_event_valid = 0;
    rtc_instance.clear_event_valid = 0;
    rtc_instance.line_high = 0;
    rtc_instance.regs[0] = 0u;
    rtc_instance.regs[1] = 0u;
    rtc_instance.regs[2] = 0u;
    rtc_instance.regs[3] = 0u;
    rtc_instance.now_high_water = 0u;
}

void semu_sapporo_rtc_probe(uint32_t regs[4], int *armed, int *line_high)
{
    /* Pure observation for the E-SAP-0034 boot-cycle census; touches
     * nothing, so it is safe to call between machine runs. */
    if (regs != NULL) {
        regs[0] = rtc_instance.regs[0];
        regs[1] = rtc_instance.regs[1];
        regs[2] = rtc_instance.regs[2];
        regs[3] = rtc_instance.regs[3];
    }
    if (armed != NULL) {
        *armed = rtc_instance.alarm_event_valid != 0 ? 1 : 0;
    }
    if (line_high != NULL) {
        *line_high = rtc_instance.line_high != 0 ? 1 : 0;
    }
}

void semu_sapporo_rtc_attach(semu_scheduler *scheduler,
                             semu_apollo4_irq_fn sink, void *context)
{
    /* The machine attaches after the board map (ulsan_rtc.c seam):
     * device maps reset detach; device resets keep the seams. */
    rtc_instance.scheduler = scheduler;
    rtc_instance.irq_sink = sink;
    rtc_instance.irq_context = context;
}

