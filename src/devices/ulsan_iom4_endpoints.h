/*
 * I2C endpoints behind the Ulsan IOM4 doorbell (ticket 730).
 *
 * Byte truth from the lane: the recorder at device address 0x28
 * (E-ULS-0029, Apollo4IomDma.cs line 656) and the fuel gauge at device
 * address 0x36 (E-ULS-0032, Apollo4IomDma.cs line 546). Calls model one
 * complete II2CPeripheral phase exactly as the lane's inner IOMaster
 * delivers them: a whole-block Write, then Read bytes, then
 * FinishTransmission at transaction end.
 */
#ifndef SEMU_DEVICES_ULSAN_IOM4_ENDPOINTS_H
#define SEMU_DEVICES_ULSAN_IOM4_ENDPOINTS_H

#include <stdint.h>

/* Device address selectors the engine dispatches on. */
#define ULSAN_IOM4_DEV_RECORDER 0x28u
#define ULSAN_IOM4_DEV_FUEL_GAUGE 0x36u

/* 1 when the engine may transfer to this endpoint; other endpoints are
 * beyond the observed traffic and transfer nothing (lane refusal shape). */
int ulsan_iom4_endpoint_known(unsigned device);

void ulsan_iom4_endpoints_reset(void);

/* Whole-block write phase (length >= 1; a length of 0 does nothing). */
void ulsan_iom4_endpoint_write(unsigned device, const uint8_t *data,
                               unsigned length);

/* Read phase: fills out[0..count) from the endpoint. */
void ulsan_iom4_endpoint_read(unsigned device, uint8_t *out,
                              unsigned count);

/* I2C FinishTransmission for the transaction that just ended. */
void ulsan_iom4_endpoint_finish(unsigned device);

#endif /* SEMU_DEVICES_ULSAN_IOM4_ENDPOINTS_H */
