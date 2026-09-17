#ifndef SEMU_DEVICES_SAPPORO_RTC_TIME_H
#define SEMU_DEVICES_SAPPORO_RTC_TIME_H

#include "semu/types.h"

#include <stdint.h>

/*
 * Calendar and BCD helpers for the Sapporo-2.35.34 live RTC module
 * (E-SAP-0035): lane-peripheral field validation, epoch-calendar
 * arithmetic (1970-01-01 epoch, 100 Hz ticks, Thursday weekday), and
 * per-field validated capture. Pure; no device state. See
 * src/devices/sapporo_rtc.c for the register law they serve.
 */

#define SRTC_TICK_NS UINT64_C(10000000) /* 100 Hz peripheral clock */
#define SRTC_SEC_TICKS UINT64_C(100)
#define SRTC_DAY_TICKS UINT64_C(8640000)

unsigned srtc_bcd(unsigned dec);
unsigned srtc_unbcd(unsigned bcd);
int srtc_bcd_ok(unsigned bcd, unsigned max_bcd, int zero_allowed);
int srtc_dim(unsigned y, unsigned m);
int64_t srtc_days_from_civil(int64_t y, int64_t m, int64_t d);
void srtc_civil_from_days(int64_t z, unsigned *y, unsigned *m, unsigned *d);
unsigned srtc_tick_wd(uint64_t tick);
void srtc_split_tick(uint64_t tick, unsigned *hun, unsigned *sec,
                     unsigned *min, unsigned *hr);
uint64_t srtc_mk_t(unsigned year, unsigned mon, unsigned day, unsigned hr,
                   unsigned min, unsigned sec, unsigned hun);
unsigned srtc_field(unsigned v, unsigned mask_shift, unsigned bits,
                    unsigned max_bcd, unsigned old, int zero_allowed);

#endif
