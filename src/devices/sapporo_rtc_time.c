/*
 * Pure helpers for the Sapporo-2.35.34 live RTC (E-SAP-0035); the
 * register law and semantics live in src/devices/sapporo_rtc.c, the
 * lane-peripheral citations are in its header and the evidence row.
 */
#include "sapporo_rtc_time.h"

unsigned srtc_bcd(unsigned dec) { return ((dec / 10u) << 4) | (dec % 10u); }
unsigned srtc_unbcd(unsigned bcd) { return ((bcd >> 4) & 0xFu) * 10u + (bcd & 0xFu); }

/* lane BCDValueField.BCDSet: reject above max BCD (hex compare, so
 * non-BCD nibbles fail too) and reject zero when not allowed */
int srtc_bcd_ok(unsigned bcd, unsigned max_bcd, int zero_allowed)
{
    return bcd <= max_bcd && (zero_allowed || bcd != 0u);
}

int srtc_dim(unsigned y, unsigned m)
{
    static const unsigned t[12] = {31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u,
                                   30u, 31u, 30u, 31u};
    unsigned leap = (y % 4u == 0u && (y % 100u != 0u || y % 400u == 0u))
                        ? 1u : 0u;
    if (m < 1u || m > 12u) {
        return 0;
    }
    return (int)((m == 2u) ? 28u + leap : t[m - 1u]);
}

/* days_from_civil / civil_from_days (Howard Hinnant), 1970-01-01 = 0 */
int64_t srtc_days_from_civil(int64_t y, int64_t m, int64_t d)
{
    int64_t era, yoe, doy, doe;
    y -= (m <= 2);
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

void srtc_civil_from_days(int64_t z, unsigned *y, unsigned *m, unsigned *d)
{
    int64_t era, doe, yoe, yr, doy, mp;
    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    yr = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    *d = (unsigned)(doy - (153 * mp + 2) / 5 + 1);
    *m = (unsigned)(mp + (mp < 10 ? 3 : -9));
    *y = (unsigned)(yr + ((*m <= 2) ? 1 : 0));
}

unsigned srtc_tick_wd(uint64_t tick)
{
    return (unsigned)((tick / SRTC_DAY_TICKS + 4u) % 7u); /* epoch: Thu=4 */
}

void srtc_split_tick(uint64_t tick, unsigned *hun, unsigned *sec,
                           unsigned *min, unsigned *hr)
{
    uint64_t tod = tick % SRTC_DAY_TICKS;
    *hun = (unsigned)(tod % 100u);
    tod /= 100u;
    *sec = (unsigned)(tod % 60u);
    tod /= 60u;
    *min = (unsigned)(tod % 60u);
    *hr = (unsigned)(tod / 60u);
}

uint64_t srtc_mk_t(unsigned year, unsigned mon, unsigned day,
                        unsigned hr, unsigned min, unsigned sec,
                        unsigned hun)
{
    return (uint64_t)srtc_days_from_civil((int64_t)year, (int64_t)mon,
                                         (int64_t)day) * SRTC_DAY_TICKS +
           (uint64_t)(hr * 3600u + min * 60u + sec) * SRTC_SEC_TICKS + hun;
}

unsigned srtc_field(unsigned v, unsigned mask_shift, unsigned bits,
                    unsigned max_bcd, unsigned old, int zero_allowed)
{
    unsigned raw = (v >> mask_shift) & ((1u << bits) - 1u);
    return srtc_bcd_ok(raw, max_bcd, zero_allowed) ? raw : old;
}
