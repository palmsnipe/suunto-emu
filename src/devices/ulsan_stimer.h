#ifndef SEMU_DEVICES_ULSAN_STIMER_H
#define SEMU_DEVICES_ULSAN_STIMER_H

#include "semu/bus.h"
#include "semu/scheduler.h"

/*
 * Ulsan 2.35.36 SystemTimer at 0x40008800 (ticket 730; law from
 * E-ULS-0014/E-ULS-0020/E-ULS-0038, superseded in part by
 * E-ULS-0045/E-ULS-0046).
 *
 * The lane answers this window with the wrapper
 * Timers.Apollo4RetainedSystemTimer (lane SapporoApollo4Extensions.cs,
 * loaded by ulsan-2.35.36.resc after `sysbus Unregister stimer`),
 * delegating everything but the NVRAM plane to upstream
 * Timers.AmbiqApollo4_SystemTimer. E-ULS-0045 ran lane censuses
 * (lp47/lp48, each twice, identical value streams) plus the tree
 * access census (probe73, twice identical): the guest's tick getter
 * (0xc2b76 -> 0x9bf48 -> 0xc2bc8) performs three consecutive 32-bit
 * reads of +0x04 inside PRIMASK-masked scope and returns the middle
 * sample if the first two agree else the third, so counter semantics
 * are engine-visible. The lane counter is virtual time: CONFIG reset
 * 0x80000000 (FREEZE), frozen at 0 until boot's CONFIG write 0x303
 * (CLKSEL = 3), then exactly +0xA0 per 5 ms = 32000 Hz (the lane's
 * modeled XTAL_32KHZ rate; silicon is 32768 but the lane is the sole
 * oracle). The tree previously answered +0x04 with a +1-per-read
 * advance (E-ULS-0014, pinned from one lp40-era sample), which made
 * wake math (0xc315a elapsed = now - stamp) count getter calls, not
 * ticks, and kept the TIMER1 reprogram near the full period while the
 * lane re-anchored to 0x397 (E-ULS-0043/0044). E-ULS-0046 (human
 * adoption) replaced the law with the upstream one:
 *
 *   +0x00 CONFIG: plain 32-bit store, reset 0x80000000 (FREEZE bit31,
 *       CLKSEL=NOCLK). The E-ULS-0014 bit-31 store mask was a misread
 *       of lp47's final-state read-back; lp48 read 0x80000000 at
 *       virtual time 0. Gate: Enabled = !FREEZE && !CLEAR && CLKSEL
 *       has a frequency. CLKSEL table (upstream class): 1 = 6 MHz,
 *       2 = 375 kHz, 3 = 32000 Hz, 4 = 16 kHz, 5/6 = 1 kHz; 0 and
 *       invalid indices leave it not enabled.
 *   +0x04 STTMR: read-only; virtual-time derived at the CLKSEL rate
 *       while enabled (ticks = floor(elapsed_ns * hz / 1e9), checked),
 *       held while gated, truncated to 32 bits. Reads are pure: the
 *       base is captured only on CONFIG gate transitions, because the
 *       wake loop calls the getter every ~170 ns and a per-read
 *       re-anchor truncates each sub-tick remainder and pins the
 *       counter (found by the E-ULS-0046 freeze probe). Never written
 *       in-era (E-ULS-0045 census: 663111 reads; zero writes in every
 *       era census); writes refuse.
 *   +0x50..+0x5c NVRAM: the lane wrapper's retained uint[4];
 *       survive the machine reset (firmware carries its startup mode
 *       across AIRCR, E-ULS-0038); zeroed only on a fresh map.
 *   +0x100 CTL (upstream IEN): store-back; boot clears it to 0.
 *
 * Everything else refuses (fail-closed): the lane answers in-window
 * gaps with warnings + 0, but no 2.35.36-era epoch touches them
 * (probe73 census), so the engine keeps refusing. +0x108 INTCLR is
 * written only by the guest's IRQ40 vector handler (0xc2bc0), which
 * no engine raises in-era (no STIMER IRQ line is wired), so it stays
 * refused. Like the pre-E-ULS-0046 model, the state is process-static
 * and outside the machine snapshot plane (pre-existing footprint).
 */

semu_status semu_ulsan_stimer_map(semu_bus *bus, semu_error *error);

/* Feed the STTMR clock: without an attached scheduler the virtual-time
 * registers (+0x00 write, +0x04 read) refuse. */
void semu_ulsan_stimer_attach(semu_scheduler *scheduler);

#endif
