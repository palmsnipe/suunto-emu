#ifndef SEMU_DEVICES_SAPPORO_IOM4_GAUGE_H
#define SEMU_DEVICES_SAPPORO_IOM4_GAUGE_H

#include <stddef.h>
#include <stdint.h>

/*
 * E-SAP-0036 MAX17050 protocol used by the Sapporo-2.35.34 IOM4 mirror:
 * byte-level register access with little-endian 16-bit pairs and
 * auto-increment, mirroring lane SapporoApollo4Iom4.cs:SapporoMax17050
 * (probe pair 854d2ebd.. proves register write-back and pointer reset;
 * the reset values are the deterministic host-side battery fixture, the
 * same one E-SAP-MAX17050-001 records for the Ulsan-side endpoint).
 */

typedef struct {
    uint16_t registers[256];
    uint8_t reg;
    int expecting_register;
    unsigned read_index;
    unsigned write_index;
} semu_sapporo_iom4_gauge;

void semu_sapporo_iom4_gauge_reset(semu_sapporo_iom4_gauge *g);
void semu_sapporo_iom4_gauge_write(semu_sapporo_iom4_gauge *g,
                                   const uint8_t *data, size_t n);
void semu_sapporo_iom4_gauge_read(semu_sapporo_iom4_gauge *g, uint8_t *out,
                                  size_t n);
void semu_sapporo_iom4_gauge_finish(semu_sapporo_iom4_gauge *g);

#endif
