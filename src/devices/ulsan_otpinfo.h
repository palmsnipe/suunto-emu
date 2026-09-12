#ifndef SEMU_DEVICES_ULSAN_OTPINFO_H
#define SEMU_DEVICES_ULSAN_OTPINFO_H

#include "semu/bus.h"

/*
 * Ulsan 2.35.36 NVM OTP INFO1 reads at 0x42003240 and 0x42003310
 * (ticket 730, E-ULS-0012).
 *
 * The reference lane has no peripheral behind the NVM_OTP/INFO1 tag
 * range, and its sysbus warning lines - identical in both 2.35.36 runs -
 * record these two reads as "ReadDoubleWord from non existing peripheral
 * ..., returning 0x00000000" (reader at 0x000c056c). The scratch access
 * trace shows the boot phase touches no other NVM_OTP address through
 * instruction 200,000,000 and performs no write. This device answers
 * exactly these two words with 0 and refuses every other access in the
 * window.
 */

semu_status semu_ulsan_otpinfo_map(semu_bus *bus, semu_error *error);

#endif
