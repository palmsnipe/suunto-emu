/*
 * Sapporo-2.35.34 live RTC at 0x40004800 (E-SAP-0035 law rewrite): the
 * register law mirrors the lane oracle peripheral
 * Timers.AmbiqApollo4_RTC (renode-infrastructure master source, read
 * in full; register map, WRTC write-gating, per-field BCD validation,
 * 100 Hz epoch clock, per-RPT first-occurrence with strict-earlier
 * skip, Limit==Value immediate occurrence, IRQ = Enable && Status
 * until Clear). Two byte-identical lane probe pairs (write matrix,
 * guest mirror), the in-tree 30-transaction access census,
 * and the E-SAP-0034 flush observation back every law here; the
 * E-SAP-0033/0034 approximate alarm-pair/61us laws retire with this
 * evidence (the 0x200+0x208 stores are clear-then-enable IRQ setup,
 * the cadence comes from RPT=Second). In-window unmodelled offsets
 * read 0 and writes drop exactly like the lane (rb3 pair); the
 * window ends at the lane's 0x210 size; non-4 widths stay refused.
 * The engine software reset (E-SAP-0034) flushes the scheduler; a
 * regressed scheduler clock marks the recorded occurrence stale and
 * re-bases the epoch clock onto the restarted clock.
 */
#include "sapporo_rtc.h"
#include "sapporo_rtc_time.h"

#include "semu/scheduler.h"

#include <string.h>

#define RTC_WINDOW_SIZE 0x210u /* lane IKnownSize */
#define SRTC_TICK_NS UINT64_C(10000000) /* 100 Hz peripheral clock */
#define SRTC_SEC_TICKS UINT64_C(100)
#define SRTC_DAY_TICKS UINT64_C(8640000)

typedef struct {
    semu_scheduler *scheduler;
    semu_apollo4_irq_fn irq_sink;
    void *irq_context;
    uint32_t ctrl; /* WRTC b0, RPT b3:1, RSTOP b4 (lane masks b4:0) */
    int write_busy, cterr, lower_valid;
    uint64_t lower_tick;
    /* epoch clock: epoch_ns of the clock value at base_ns */
    uint64_t base_ns, epoch_ns, refresh_tick;
    int running;
    /* counter field storage (decimal halves of validated BCD) */
    unsigned f_hun, f_sec, f_min, f_hr, f_day, f_mon, f_yr, f_wd;
    int cb, ceb;
    /* alarm match fields (decimal) */
    unsigned a100, asec, amin, ahr, aday, amon, awd;
    /* occurrence bookkeeping */
    uint64_t next_tick, interval_ticks;
    semu_event_id alarm_event;
    int alarm_pending;
    uint64_t high_water;
    int stat, inten, line_high, init_done;
} sapporo_rtc_state;

static sapporo_rtc_state rtc_instance;

static uint64_t rtc_clock_t(const sapporo_rtc_state *s, uint64_t now)
{
    if (s->running && now > s->base_ns) {
        return s->epoch_ns + (now - s->base_ns);
    }
    return s->epoch_ns;
}

static uint64_t rtc_tick(const sapporo_rtc_state *s, uint64_t now)
{
    return rtc_clock_t(s, now) / SRTC_TICK_NS;
}

/* lane UpdateCounterFields: refresh only when the timer value moved */
static void rtc_refresh(sapporo_rtc_state *s, uint64_t now)
{
    uint64_t tick = rtc_tick(s, now);
    unsigned y, mm, dd, h, m, mi, hr;
    if (tick == s->refresh_tick) {
        return;
    }
    srtc_split_tick(tick, &h, &m, &mi, &hr);
    s->f_hun = srtc_bcd(h); s->f_sec = srtc_bcd(m);
    s->f_min = srtc_bcd(mi); s->f_hr = srtc_bcd(hr);
    srtc_civil_from_days((int64_t)(tick / SRTC_DAY_TICKS), &y, &mm, &dd);
    s->f_mon = srtc_bcd(mm); s->f_day = srtc_bcd(dd);
    s->f_yr = srtc_bcd(y % 100u);
    s->f_wd = srtc_tick_wd(tick);
    if (s->ceb) {
        s->cb = (y < 2000u || y >= 2100u) ? 1 : 0;
    }
    s->refresh_tick = tick;
}

static void rtc_set_line(sapporo_rtc_state *s, int level)
{
    if (s->irq_sink == NULL || s->line_high == (level != 0)) {
        return;
    }
    s->line_high = (level != 0);
    s->irq_sink(s->irq_context, 2u, s->line_high);
}

static void rtc_update_line(sapporo_rtc_state *s)
{
    rtc_set_line(s, (s->inten && s->stat) ? 1 : 0);
}

static uint64_t rtc_period_ticks(unsigned rpt)
{
    switch (rpt) {
    case 7u: return 100u;                            /* Second */
    case 6u: return 6000u;                           /* Minute */
    case 5u: return 360000u;                         /* Hour */
    case 4u: return SRTC_DAY_TICKS;                   /* Day */
    case 3u: return 7u * SRTC_DAY_TICKS;              /* Week */
    case 2u: return 31u * SRTC_DAY_TICKS;             /* Month  */
    case 1u: return 365u * SRTC_DAY_TICKS;            /* Year   */
    default: return 0u;
    }
}

/* lane RTCTimer.UpdateAlarm first occurrence for the repeat enum from
 * the clock tick; 0 when no occurrence exists (Disabled). */
static uint64_t rtc_first_alarm(const sapporo_rtc_state *s, uint64_t tick,
                                uint64_t clock_ns, uint64_t *period)
{
    uint64_t day = tick / SRTC_DAY_TICKS, cand;
    unsigned y, m, d;
    unsigned ah, am, as, au, ad, amn;
    uint64_t tod;
    unsigned rpt = (s->ctrl >> 1) & 7u;

    *period = rtc_period_ticks(rpt);
    if (rpt == 0u) {
        return UINT64_MAX; /* no alarm (0 itself is a valid occurrence) */
    }
    ah = srtc_unbcd(s->ahr); am = srtc_unbcd(s->amin);
    as = srtc_unbcd(s->asec); au = srtc_unbcd(s->a100);
    ad = srtc_unbcd(s->aday) ? srtc_unbcd(s->aday) : 1u;
    amn = srtc_unbcd(s->amon) ? srtc_unbcd(s->amon) : 1u;
    tod = (uint64_t)(ah * 3600u + am * 60u + as) * 100u + au;
    switch (rpt) {
    case 7u:
        cand = tick - (tick % SRTC_SEC_TICKS) + au;
        break;
    case 6u:
        cand = tick - (tick % 6000u) + tod - (uint64_t)ah * 360000u;
        break;
    case 5u:
        cand = tick - (tick % 360000u) + tod;
        break;
    case 4u:
        cand = day * SRTC_DAY_TICKS + tod;
        break;
    case 3u:
        cand = day * SRTC_DAY_TICKS + tod;
        cand += (uint64_t)(((s->awd + 7u - srtc_tick_wd(cand)) % 7u)) *
                SRTC_DAY_TICKS;
        break;
    case 2u:
        srtc_civil_from_days((int64_t)day, &y, &m, &d);
        if ((int)ad > srtc_dim(y, m)) {
            return UINT64_MAX; /* lane throws on the invalid DateTime */
        }
        cand = srtc_mk_t(y, m, ad, ah, am, as, au);
        break;
    case 1u:
        srtc_civil_from_days((int64_t)day, &y, &m, &d);
        if ((int)ad > srtc_dim(y, amn)) {
            return UINT64_MAX;
        }
        cand = srtc_mk_t(y, amn, ad, ah, am, as, au);
        break;
    default:
        return UINT64_MAX;
    }
    if (cand * SRTC_TICK_NS < clock_ns) {
        /* lane: strict DateTime comparison of firstAlarm against the
         * current clock (sub-tick precision) skips one whole unit. */
        cand += *period;
    }
    return cand;
}

static void rtc_alarm_cb(void *context, uint64_t now_ns);

static void rtc_reschedule(sapporo_rtc_state *s, uint64_t after_ns)
{
    uint64_t clock, period, cand;
    semu_error error;

    s->alarm_pending = 0;
    clock = rtc_clock_t(s, after_ns);
    if (!s->running) {
        return; /* stopped lane timer cannot reach an occurrence */
    }
    cand = rtc_first_alarm(s, clock / SRTC_TICK_NS, clock, &period);
    if (cand == UINT64_MAX || period == 0u) {
        return;
    }
    if (cand <= clock / SRTC_TICK_NS) {
        /* Limit == Value fires on the next timer advance: commit the
         * occurrence now and move to the following one. */
        s->stat = 1;
        rtc_update_line(s);
        cand += period;
    }
    s->interval_ticks = period;
    s->next_tick = cand;
    if (semu_scheduler_schedule(s->scheduler,
                                cand * SRTC_TICK_NS - clock, rtc_alarm_cb, s,
                                &s->alarm_event, &error) == SEMU_OK) {
        s->alarm_pending = 1;
    }
}

static void rtc_alarm_cb(void *context, uint64_t now_ns)
{
    sapporo_rtc_state *s = (sapporo_rtc_state *)context;
    uint64_t clock = rtc_clock_t(s, now_ns);
    uint64_t t = s->next_tick + s->interval_ticks;
    uint64_t t_ns = t * SRTC_TICK_NS;
    semu_error error;

    s->alarm_pending = 0;
    s->stat = 1;
    rtc_update_line(s);
    s->next_tick = t;
    if (!s->running || t_ns <= clock) {
        rtc_reschedule(s, now_ns);
        return;
    }
    if (semu_scheduler_schedule(s->scheduler, t_ns - clock, rtc_alarm_cb, s,
                                &s->alarm_event, &error) == SEMU_OK) {
        s->alarm_pending = 1;
    }
}

/* Recompute on every alarm-input write; the scheduler clock is
 * monotonic inside a machine life, so a regression is the
 * E-SAP-0034 software-reset flush: the recorded event is gone and
 * the epoch clock re-bases onto the restarted clock. */
static void rtc_update_alarm(sapporo_rtc_state *s, uint64_t now)
{
    if (s->scheduler == NULL) {
        return;
    }
    if (now < s->high_water) {
        s->alarm_pending = 0;
        s->base_ns = now;
    }
    if (now > s->high_water) {
        s->high_water = now;
    }
    if (s->alarm_pending) {
        if (semu_scheduler_cancel(s->scheduler, s->alarm_event) != 0) {
            s->alarm_pending = 0; /* vanished via a scheduler flush */
        }
    }
    rtc_reschedule(s, now);
}

static void rtc_reset(void *context);
static void rtc_ensure_init(void);

static uint32_t rtc_read_reg(sapporo_rtc_state *s, uint32_t offset,
                             uint64_t now)
{
    switch (offset) {
    case 0x000u: /* Control */
        return s->ctrl;
    case 0x004u: /* Status */
        return (uint32_t)s->write_busy;
    case 0x020u: /* CountersLower */
        rtc_refresh(s, now);
        s->lower_tick = rtc_tick(s, now);
        s->lower_valid = 1;
        s->cterr = 0;
        return s->f_hun | (s->f_sec << 8) | (s->f_min << 16) |
               (s->f_hr << 24);
    case 0x024u: /* CountersUpper */
        rtc_refresh(s, now);
        return s->f_day | (s->f_mon << 8) | (s->f_yr << 16) |
               (s->f_wd << 24) | (s->cb << 28) | (s->ceb << 29) |
               (uint32_t)(s->cterr ? 0x80000000u : 0u);
    case 0x030u: /* AlarmsLower */
        return s->a100 | (s->asec << 8) | (s->amin << 16) | (s->ahr << 24);
    case 0x034u: /* AlarmsUpper */
        return s->aday | (s->amon << 8) | (s->awd << 16);
    case 0x200u: /* InterruptEnable */
        return (uint32_t)s->inten;
    case 0x204u: /* InterruptStatus */
        return (uint32_t)s->stat;
    default:
        return 0u; /* 0x208/0x20c write-only and unmodelled read as 0 */
    }
}

static semu_status rtc_read(void *context, uint32_t offset, unsigned width,
                            uint32_t *value, semu_error *error)
{
    sapporo_rtc_state *s = &rtc_instance;
    uint64_t now;

    (void)context;
    if (value == NULL || width != 4u || offset >= RTC_WINDOW_SIZE ||
        (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Sapporo RTC read is not evidenced");
        return SEMU_ERR_UNSUPPORTED;
    }
    rtc_ensure_init();
    now = s->scheduler != NULL ? semu_scheduler_now(s->scheduler) : 0u;
    *value = rtc_read_reg(s, offset, now);
    return SEMU_OK;
}

static semu_status rtc_write(void *context, uint32_t offset, unsigned width,
                             uint32_t value, semu_error *error)
{
    sapporo_rtc_state *s = &rtc_instance;
    uint64_t now;
    unsigned year, yr;

    (void)context;
    if (width != 4u || offset >= RTC_WINDOW_SIZE || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Sapporo RTC write is not evidenced");
        return SEMU_ERR_UNSUPPORTED;
    }
    rtc_ensure_init();
    now = s->scheduler != NULL ? semu_scheduler_now(s->scheduler) : 0u;
    switch (offset) {
    case 0x000u: /* Control */ {
        int stopped = (int)((value >> 4) & 1u);
        int was = !s->running;
        s->ctrl = value & 0x1Fu;
        if (stopped != was) {
            if (stopped) {
                s->epoch_ns = rtc_clock_t(s, now);
                s->running = 0;
            } else {
                s->base_ns = now;
                s->running = 1;
            }
        }
        rtc_update_alarm(s, now); /* lane RPT changeCallback */
        break;
    }
    case 0x020u: /* CountersLower */
        if ((s->ctrl & 1u) == 0u) {
            break; /* WRTC gating drops the whole write (lane) */
        }
        s->f_hun = srtc_field(value, 0, 8, 0x99u, s->f_hun, 1);
        s->f_sec = srtc_field(value, 8, 7, 0x59u, s->f_sec, 1);
        s->f_min = srtc_field(value, 16, 7, 0x59u, s->f_min, 1);
        s->f_hr = srtc_field(value, 24, 6, 0x23u, s->f_hr, 1);
        s->write_busy = 1;
        break;
    case 0x024u: /* CountersUpper */
        if ((s->ctrl & 1u) == 0u) {
            break;
        }
        s->cterr = (s->lower_valid &&
                    s->lower_tick == rtc_tick(s, now)) ? 1 : 0;
        s->write_busy = 0;
        s->f_day = srtc_field(value, 0, 6, 0x31u, s->f_day, 0);
        s->f_mon = srtc_field(value, 8, 5, 0x12u, s->f_mon, 0);
        s->f_yr = srtc_field(value, 16, 8, 0x99u, s->f_yr, 1);
        s->f_wd = srtc_field(value, 24, 3, 6u, s->f_wd, 1);
        s->ceb = (int)((value >> 29) & 1u);
        /* lane CalculateYear(centuryBit, yearsOfCentury) */
        yr = srtc_unbcd(s->f_yr);
        year = s->cb ? (yr < 70u ? 2100u + yr : 1900u + yr) : 2000u + yr;
        if (srtc_dim(year, srtc_unbcd(s->f_mon)) >= (int)srtc_unbcd(s->f_day) &&
            s->f_mon != 0u) {
            s->epoch_ns = srtc_mk_t(year, srtc_unbcd(s->f_mon),
                                    srtc_unbcd(s->f_day), srtc_unbcd(s->f_hr),
                                    srtc_unbcd(s->f_min), srtc_unbcd(s->f_sec),
                                    srtc_unbcd(s->f_hun)) * SRTC_TICK_NS;
            s->base_ns = now;
            s->cb = (year < 2000u || year >= 2100u) ? 1 : 0;
            s->refresh_tick = UINT64_MAX; /* commit moves the value */
            rtc_update_alarm(s, now); /* SetDateTimeInternal */
        }
        break;
    case 0x030u: /* AlarmsLower */
        s->a100 = srtc_field(value, 0, 8, 0x99u, s->a100, 1);
        s->asec = srtc_field(value, 8, 7, 0x59u, s->asec, 1);
        s->amin = srtc_field(value, 16, 7, 0x59u, s->amin, 1);
        s->ahr = srtc_field(value, 24, 6, 0x23u, s->ahr, 1);
        break;
    case 0x034u: /* AlarmsUpper */
        s->aday = srtc_field(value, 0, 6, 0x31u, s->aday, 0);
        s->amon = srtc_field(value, 8, 5, 0x12u, s->amon, 0);
        s->awd = srtc_field(value, 16, 3, 6u, s->awd, 1);
        break;
    case 0x200u: /* InterruptEnable */
        s->inten = (int)(value & 1u);
        rtc_update_line(s);
        break;
    case 0x208u: /* InterruptClear */
        if ((value & 1u) != 0u) {
            s->stat = 0;
            rtc_update_line(s);
        }
        break;
    case 0x20cu: /* InterruptSet */
        if ((value & 1u) != 0u) {
            s->stat = 1;
            rtc_update_line(s);
        }
        break;
    default:
        break; /* lane drops unmodelled in-window writes silently */
    }
    return SEMU_OK;
}

static void rtc_init_state(void)
{
    memset(&rtc_instance, 0, sizeof(rtc_instance));
    rtc_instance.running = 1;
    rtc_instance.refresh_tick = UINT64_MAX;
    rtc_instance.f_day = 1u; rtc_instance.f_mon = 1u;
    rtc_instance.f_yr = 70u; rtc_instance.f_wd = 4u; /* 1970-01-01, Thu */
    rtc_instance.cb = 1;
}

/* The bus map does not dispatch ops->reset, so the cold field state
 * is established lazily on the first access (attach happens after the
 * map; the seams are preserved exactly as across a reset). */
static void rtc_ensure_init(void)
{
    if (!rtc_instance.init_done) { rtc_reset(NULL); }
}

static void rtc_reset(void *context)
{
    int was_high = rtc_instance.line_high;
    sapporo_rtc_state *s = &rtc_instance;
    semu_scheduler *sched = s->scheduler;
    semu_apollo4_irq_fn sink = s->irq_sink;
    void *ctx = s->irq_context;

    (void)context;
    if (sched != NULL && s->alarm_pending) {
        (void)semu_scheduler_cancel(sched, s->alarm_event);
    }
    rtc_init_state();
    s->scheduler = sched;
    s->irq_sink = sink;
    s->irq_context = ctx;
    s->init_done = 1;
    if (was_high && s->irq_sink != NULL) {
        s->irq_sink(s->irq_context, 2u, 0); /* lane Reset falls the line */
    }
    if (sched != NULL) {
        s->high_water = semu_scheduler_now(sched);
    }
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
    /* Map-side seam (ulsan_rtc.c analogue): forget the seams and the
     * state without touching a possibly-freed sink. */
    memset(&rtc_instance, 0, sizeof(rtc_instance));
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

uint32_t semu_sapporo_rtc_probe(uint32_t offset)
{
    sapporo_rtc_state *s = &rtc_instance;
    uint64_t now = s->scheduler != NULL ? semu_scheduler_now(s->scheduler)
                                        : 0u;
    return rtc_read_reg(s, offset, now);
}

int semu_sapporo_rtc_probe_pending(int *line_high)
{
    if (line_high != NULL) {
        *line_high = rtc_instance.line_high;
    }
    return rtc_instance.alarm_pending != 0 ? 1 : 0;
}
