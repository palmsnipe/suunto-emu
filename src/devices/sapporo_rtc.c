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
#include "../core/scheduler_internal.h"
#include "../core/snapshot_io.h"

#include <string.h>
#include <stdlib.h>

#define RTC_WINDOW_SIZE 0x210u /* lane IKnownSize */
#define SRTC_TICK_NS UINT64_C(10000000) /* 100 Hz peripheral clock */
#define SRTC_SEC_TICKS UINT64_C(100)
#define SRTC_DAY_TICKS UINT64_C(8640000)

struct semu_sapporo_rtc {
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
};
typedef semu_sapporo_rtc sapporo_rtc_state;

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
    if (semu_scheduler_schedule_tagged(s->scheduler,
                                      cand * SRTC_TICK_NS - clock,
                                      SEMU_SCHED_EVENT_SAP235_RTC_ALARM, 0u,
                                      rtc_alarm_cb, s,
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
    if (semu_scheduler_schedule_tagged(s->scheduler, t_ns - clock,
                                       SEMU_SCHED_EVENT_SAP235_RTC_ALARM, 0u,
                                       rtc_alarm_cb, s,
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
        if (semu_scheduler_cancel_owned(s->scheduler, s->alarm_event,
                                        rtc_alarm_cb, s) != 0) {
            s->alarm_pending = 0; /* vanished via a scheduler flush */
        }
    }
    rtc_reschedule(s, now);
}

static void rtc_reset(void *context);
static void rtc_ensure_init(sapporo_rtc_state *s);

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
    sapporo_rtc_state *s = context;
    uint64_t now;

    if (s == NULL || value == NULL || width != 4u || offset >= RTC_WINDOW_SIZE ||
        (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Sapporo RTC read is not evidenced");
        return SEMU_ERR_UNSUPPORTED;
    }
    rtc_ensure_init(s);
    now = s->scheduler != NULL ? semu_scheduler_now(s->scheduler) : 0u;
    *value = rtc_read_reg(s, offset, now);
    return SEMU_OK;
}

static semu_status rtc_write(void *context, uint32_t offset, unsigned width,
                             uint32_t value, semu_error *error)
{
    sapporo_rtc_state *s = context;
    uint64_t now;
    unsigned year, yr;

    if (s == NULL || width != 4u || offset >= RTC_WINDOW_SIZE ||
        (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Sapporo RTC write is not evidenced");
        return SEMU_ERR_UNSUPPORTED;
    }
    rtc_ensure_init(s);
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

static void rtc_init_state(sapporo_rtc_state *s)
{
    memset(s, 0, sizeof(*s));
    s->running = 1;
    s->refresh_tick = UINT64_MAX;
    s->f_day = 1u; s->f_mon = 1u;
    s->f_yr = 70u; s->f_wd = 4u; /* 1970-01-01, Thu */
    s->cb = 1;
}

/* The bus map does not dispatch ops->reset, so the cold field state
 * is established lazily on the first access. Instance bindings survive
 * this initialization exactly as they survive an explicit reset. */
static void rtc_ensure_init(sapporo_rtc_state *s)
{
    if (!s->init_done) { rtc_reset(s); }
}

static void rtc_reset(void *context)
{
    sapporo_rtc_state *s = context;
    int was_high = s->line_high;
    semu_scheduler *sched = s->scheduler;
    semu_apollo4_irq_fn sink = s->irq_sink;
    void *ctx = s->irq_context;

    if (sched != NULL && s->alarm_pending) {
        (void)semu_scheduler_cancel_owned(sched, s->alarm_event, rtc_alarm_cb, s);
    }
    rtc_init_state(s);
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

semu_sapporo_rtc *semu_sapporo_rtc_create(semu_scheduler *scheduler,
    semu_apollo4_irq_fn sink, void *context, semu_error *error)
{
    sapporo_rtc_state *s;
    if (scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Sapporo RTC requires a scheduler");
        return NULL;
    }
    s = calloc(1u, sizeof(*s));
    if (s == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate Sapporo RTC");
        return NULL;
    }
    s->scheduler = scheduler;
    s->irq_sink = sink;
    s->irq_context = context;
    semu_error_clear(error);
    return s;
}

void semu_sapporo_rtc_destroy(semu_sapporo_rtc *s)
{
    if (s == NULL) return;
    if (s->alarm_pending) {
        (void)semu_scheduler_cancel_owned(s->scheduler, s->alarm_event,
                                          rtc_alarm_cb, s);
    }
    free(s);
}

uint32_t semu_sapporo_rtc_probe(semu_sapporo_rtc *s, uint32_t offset)
{
    uint64_t now = s->scheduler != NULL ? semu_scheduler_now(s->scheduler)
                                        : 0u;
    return rtc_read_reg(s, offset, now);
}

int semu_sapporo_rtc_probe_pending(const semu_sapporo_rtc *s, int *line_high)
{
    if (line_high != NULL) {
        *line_high = s->line_high;
    }
    return s->alarm_pending != 0 ? 1 : 0;
}

/* --- Ticket 792: live RTC snapshot codec (E-SAP-0035 state only). --- */

semu_status semu_sapporo_rtc_snapshot_write(
    const semu_sapporo_rtc *rtc, semu_snapshot_writer *writer,
    semu_error *error)
{
    const sapporo_rtc_state *s = rtc;
    if (s == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo RTC snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    /* A never-touched instance serializes the same cold state a first
     * guest access would establish; the lazy init is idempotent and
     * cannot drive the sink (the cold line starts low). */
    if (!s->init_done) {
        rtc_ensure_init((sapporo_rtc_state *)s);
    }
    if (semu_snapshot_writer_u32(writer, s->ctrl, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)s->write_busy, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)s->cterr, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)s->lower_valid, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)s->running, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)s->cb, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)s->ceb, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)s->stat, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)s->inten, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)s->line_high, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)s->init_done, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)s->alarm_pending, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, s->f_hun, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, s->f_sec, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, s->f_min, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, s->f_hr, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, s->f_day, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, s->f_mon, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, s->f_yr, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, s->f_wd, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, s->a100, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, s->asec, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, s->amin, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, s->ahr, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, s->aday, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, s->amon, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, s->awd, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, s->base_ns, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, s->epoch_ns, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, s->refresh_tick, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, s->lower_tick, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, s->next_tick, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, s->interval_ticks, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, s->alarm_event, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, s->high_water, error) != SEMU_OK)
        return error->code;
    return SEMU_OK;
}

semu_status semu_sapporo_rtc_snapshot_read(
    semu_sapporo_rtc *rtc, semu_snapshot_reader *reader, semu_error *error)
{
    sapporo_rtc_state *s = rtc;
    sapporo_rtc_state candidate;
    if (s == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo RTC snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *s;
    if (semu_snapshot_reader_u32(reader, &candidate.ctrl, error) != SEMU_OK)
        return error->code;
    {
        uint8_t flags[11];
        size_t index;
        for (index = 0u; index < sizeof(flags); ++index) {
            if (semu_snapshot_reader_u8(reader, &flags[index], error) != SEMU_OK)
                return error->code;
        }
        candidate.write_busy = flags[0];
        candidate.cterr = flags[1];
        candidate.lower_valid = flags[2];
        candidate.running = flags[3];
        candidate.cb = flags[4];
        candidate.ceb = flags[5];
        candidate.stat = flags[6];
        candidate.inten = flags[7];
        candidate.line_high = flags[8];
        candidate.init_done = flags[9];
        candidate.alarm_pending = flags[10];
    }
    if (semu_snapshot_reader_u32(reader, &candidate.f_hun, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.f_sec, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.f_min, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.f_hr, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.f_day, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.f_mon, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.f_yr, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.f_wd, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.a100, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.asec, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.amin, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.ahr, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.aday, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.amon, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.awd, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &candidate.base_ns, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &candidate.epoch_ns, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &candidate.refresh_tick, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &candidate.lower_tick, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &candidate.next_tick, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &candidate.interval_ticks, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &candidate.alarm_event, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &candidate.high_water, error) != SEMU_OK)
        return error->code;
    /* Only states the E-SAP-0035 write law can produce round-trip; the
     * field maxima are the lane hex-compare bounds and the line/pending
     * identities are the register law's own invariants. */
    if (candidate.ctrl > 0x1fu ||
        candidate.write_busy > 1 || candidate.cterr > 1 ||
        candidate.lower_valid > 1 || candidate.running > 1 ||
        candidate.cb > 1 || candidate.ceb > 1 ||
        candidate.stat > 1 || candidate.inten > 1 ||
        candidate.line_high > 1 || candidate.init_done > 1 ||
        candidate.alarm_pending > 1 ||
        candidate.f_hun > 0x99u || candidate.f_sec > 0x59u ||
        candidate.f_min > 0x59u || candidate.f_hr > 0x23u ||
        candidate.f_day > 0x31u || candidate.f_day == 0u ||
        candidate.f_mon > 0x12u || candidate.f_mon == 0u ||
        candidate.f_yr > 0x99u || candidate.f_wd > 6u ||
        candidate.a100 > 0x99u || candidate.asec > 0x59u ||
        candidate.amin > 0x59u || candidate.ahr > 0x23u ||
        candidate.aday > 0x31u || candidate.amon > 0x12u ||
        candidate.awd > 6u ||
        (candidate.line_high != 0) !=
            (candidate.inten != 0 && candidate.stat != 0) ||
        (candidate.alarm_pending != 0 &&
         (candidate.running == 0 || candidate.alarm_event == 0u ||
          candidate.interval_ticks !=
              rtc_period_ticks((candidate.ctrl >> 1) & 7u)))) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "Sapporo RTC snapshot state is unreachable");
        return SEMU_ERR_FORMAT;
    }
    *s = candidate;
    return SEMU_OK;
}

semu_status semu_sapporo_rtc_snapshot_resolve_event(
    semu_sapporo_rtc *rtc, uint32_t subject,
    semu_event_callback *callback, void **context, semu_error *error)
{
    sapporo_rtc_state *s = rtc;
    if (s == NULL || callback == NULL || context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo RTC snapshot event arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (subject == 0u && s->alarm_pending && s->alarm_event != 0u) {
        *callback = rtc_alarm_cb;
        *context = s;
        return SEMU_OK;
    }
    semu_error_set(error, SEMU_ERR_CONFLICT,
                   "Sapporo RTC snapshot event is not present");
    return SEMU_ERR_CONFLICT;
}

semu_status semu_sapporo_rtc_snapshot_event_id_matches(
    const semu_sapporo_rtc *rtc, uint32_t subject, semu_event_id event_id,
    semu_error *error)
{
    const sapporo_rtc_state *s = rtc;
    if (s != NULL && subject == 0u && s->alarm_pending &&
        s->alarm_event == event_id && event_id != 0u)
        return SEMU_OK;
    semu_error_set(error, SEMU_ERR_FORMAT,
                   "Sapporo RTC snapshot event identity does not match device");
    return SEMU_ERR_FORMAT;
}

semu_status semu_sapporo_rtc_snapshot_event_links_match(
    const semu_sapporo_rtc *rtc, const semu_scheduled_event_state *events,
    size_t count, semu_error *error)
{
    const sapporo_rtc_state *s = rtc;
    const semu_scheduled_event_state *found = NULL;
    uint64_t due;
    size_t index;
    if (s == NULL || (events == NULL && count != 0u)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo RTC snapshot linkage arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (!s->alarm_pending) {
        return SEMU_OK;
    }
    for (index = 0u; index < count; ++index) {
        if (events[index].kind == SEMU_SCHED_EVENT_SAP235_RTC_ALARM &&
            events[index].subject == 0u &&
            events[index].id == s->alarm_event) {
            found = &events[index];
            break;
        }
    }
    if (found == NULL) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "Sapporo RTC alarm has no scheduler event");
        return SEMU_ERR_FORMAT;
    }
    /* The armed delay is cand*TICK - clock, so the absolute due time is
     * exactly next_tick*TICK + base_ns - epoch_ns; require it. */
    if (s->next_tick > UINT64_MAX / SRTC_TICK_NS) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "Sapporo RTC alarm tick overflows");
        return SEMU_ERR_FORMAT;
    }
    due = s->next_tick * SRTC_TICK_NS;
    if (due > UINT64_MAX - s->base_ns || due + s->base_ns < s->epoch_ns) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "Sapporo RTC alarm due time is unreachable");
        return SEMU_ERR_FORMAT;
    }
    due += s->base_ns;
    due -= s->epoch_ns;
    if (found->due_ns != due) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "Sapporo RTC alarm due time disagrees with the event");
        return SEMU_ERR_FORMAT;
    }
    return SEMU_OK;
}
