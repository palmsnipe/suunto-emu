/*
 * Ulsan IOM4 doorbell endpoints (ticket 730, E-ULS-0029, E-ULS-0032).
 *
 * Recorder (0x28): the lane's ObservedI2cDevice is a 256-byte register
 * file where a whole-block Write selects on its first byte and stores
 * the remainder from the selector; Reads echo registers from the
 * selector unchanged.
 *
 * Fuel gauge (0x36): the lane's Max17050 is a 256-entry 16-bit register
 * file read little-endian byte-pair-wise. A block Write while the
 * pointer expectation is armed takes its first byte as the register
 * pointer and writes the rest as byte pairs; each completed pair
 * advances the pointer. Reads emit low byte then high byte and advance
 * on the pair boundary. FinishTransmission re-arms the pointer
 * expectation and resets both byte phases. Reset values are exactly
 * the five the lane class installs.
 */

#include "ulsan_iom4_endpoints.h"

#include <string.h>

typedef struct {
    uint8_t selected;
    uint8_t regs[256];
} recorder_state;

typedef struct {
    uint16_t regs[256];
    uint8_t register_pointer;
    unsigned read_byte_index;
    unsigned write_byte_index;
    int expecting_register;
} gauge_state;

static recorder_state recorder;
static gauge_state gauge;

#define GAUGE_REG_STATUS 0x00u
#define GAUGE_REG_SOC 0x06u
#define GAUGE_REG_TEMPERATURE 0x08u
#define GAUGE_REG_VCELL 0x09u
#define GAUGE_REG_AVG_VCELL 0x19u

int ulsan_iom4_endpoint_known(unsigned device)
{
    return device == ULSAN_IOM4_DEV_RECORDER ||
           device == ULSAN_IOM4_DEV_FUEL_GAUGE;
}

void ulsan_iom4_endpoints_reset(void)
{
    memset(&recorder, 0, sizeof(recorder));
    memset(&gauge, 0, sizeof(gauge));
    gauge.regs[GAUGE_REG_STATUS] = 0x0000u;
    gauge.regs[GAUGE_REG_SOC] = 0x3200u;
    gauge.regs[GAUGE_REG_TEMPERATURE] = 0x1900u;
    gauge.regs[GAUGE_REG_VCELL] = 0xC000u;
    gauge.regs[GAUGE_REG_AVG_VCELL] = 0xC000u;
    gauge.expecting_register = 1;
}

static void gauge_write_byte(uint8_t value)
{
    uint16_t current = gauge.regs[gauge.register_pointer];
    if ((gauge.write_byte_index++ & 1u) == 0u) {
        gauge.regs[gauge.register_pointer] =
            (uint16_t)((current & 0xFF00u) | value);
        return;
    }
    gauge.regs[gauge.register_pointer] =
        (uint16_t)((current & 0x00FFu) | ((uint16_t)value << 8));
    gauge.register_pointer++;
}

void ulsan_iom4_endpoint_write(unsigned device, const uint8_t *data,
                               unsigned length)
{
    unsigned index;

    if (data == NULL || length == 0u) {
        return;
    }
    if (device == ULSAN_IOM4_DEV_RECORDER) {
        recorder.selected = data[0];
        for (index = 1u; index < length; index++) {
            recorder.regs[(uint8_t)(recorder.selected + index - 1u)] =
                data[index];
        }
        return;
    }
    /* Fuel gauge. */
    index = 0u;
    if (gauge.expecting_register) {
        gauge.register_pointer = data[0];
        gauge.expecting_register = 0;
        index = 1u;
    }
    for (; index < length; index++) {
        gauge_write_byte(data[index]);
    }
}

void ulsan_iom4_endpoint_read(unsigned device, uint8_t *out,
                              unsigned count)
{
    unsigned index;

    if (out == NULL) {
        return;
    }
    if (device == ULSAN_IOM4_DEV_RECORDER) {
        for (index = 0u; index < count; index++) {
            out[index] = recorder.regs[(uint8_t)(recorder.selected + index)];
        }
        return;
    }
    /* Fuel gauge: low byte then high byte of the selected 16-bit
     * register, pointer advances on each completed pair. */
    for (index = 0u; index < count; index++) {
        uint16_t value = gauge.regs[gauge.register_pointer];
        if ((gauge.read_byte_index++ & 1u) == 0u) {
            out[index] = (uint8_t)value;
        } else {
            out[index] = (uint8_t)(value >> 8);
        }
        if ((gauge.read_byte_index & 1u) == 0u) {
            gauge.register_pointer++;
        }
    }
}

void ulsan_iom4_endpoint_finish(unsigned device)
{
    if (device != ULSAN_IOM4_DEV_FUEL_GAUGE) {
        return;
    }
    gauge.expecting_register = 1;
    gauge.read_byte_index = 0u;
    gauge.write_byte_index = 0u;
}
