#ifndef SEMU_DEVICES_ULSAN_STIMER_H
#define SEMU_DEVICES_ULSAN_STIMER_H

#include "semu/bus.h"

/*
 * Ulsan 2.35.36 SystemTimer registers at 0x40008800 (ticket 730,
 * E-ULS-0014).
 *
 * The reference lane runs the upstream AmbiqApollo4_SystemTimer device;
 * its observable boot behavior was captured by lane probes (CNT/LOAD/CTL
 * at guest PC hooks and idle dumps) and an in-tree scratch access trace.
 * Boot performs, per delta-table pass: two 32-bit OR read-modify-writes
 * to +0x00 (LOAD: constants 0x303 and 0x80000000), one clear
 * read-modify-write to +0x100 (CTL, result 0), and a three-sample
 * stability read of +0x04 (CNT) at guest helper 0x000c2bc8. The lane
 * reads back LOAD = 0x303 after the 0x80000000 write, so that write's
 * bit 31 is not stored; CTL reads 0; CNT increases monotonically with
 * executed work (probed 3..0xA7ED across boot, exceeding LOAD 0x303, so
 * it free-runs rather than wrapping at LOAD) even with CTL = 0.
 *
 * The exact CNT value cannot be byte-compared across engines (the lane
 * counter tracks its own virtual time), so this device serves a
 * monotonic free-running 32-bit tick that advances on each CNT read;
 * LOAD and CTL hold written state with the observed bit-31 LOAD mask.
 * Only the observed registers answer (LOAD, CTL, CNT, and the comparator
 * words at +0x58/+0x54 that read 0); everything else refuses.
 */

semu_status semu_ulsan_stimer_map(semu_bus *bus, semu_error *error);

#endif
