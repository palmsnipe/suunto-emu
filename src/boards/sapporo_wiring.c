/*
 * Sapporo 2.22.60 verified wiring table (ticket 400).
 * Evidence rows from plans/evidence/phase3-5.tsv:
 *   E-SAP-BUTTONS-001:   GPIO 57/58/59, active-low
 *   E-SAP-BACKLIGHT-001: CTIMER9
 *   E-SAP-LSM6DSL-001:   IOM0 (SPI), IRQ6
 *   E-SAP-HSPPAD143-001: IOM2 (I2C 0x48), IRQ8
 *   E-SAP-TLI493D-001:   IOM2 (I2C 0x35), IRQ8
 *   E-SAP-HAPTIC-001:    IOM4 (I2C 0x50), IRQ10
 *   E-SAP-OPT3007-001:   IOM3 (I2C 0x45), IRQ9
 *   E-SAP-MAX17050-001:  IOM4 (I2C 0x36), IRQ10
 *   E-SAP-CXD5610-001:   UART1 (0x4001d000)
 *   E-SAP-OHR2-001:      IOM2 (I2C 0x10), IRQ8, GPIO62 ready
 * Unverified (omitted): E-SAP-FLASH-001, E-SAP-PANEL-001
 */

#include "sapporo_wiring.h"

#include "semu/types.h"

static const semu_sapporo_wiring wiring_table[] = {
    { SEMU_SAPPORO_WIRE_BUTTON_UPPER, SEMU_SAPPORO_BUS_GPIO,
      57u, 0u, 0u, 1, "E-SAP-BUTTONS-001" },
    { SEMU_SAPPORO_WIRE_BUTTON_MIDDLE, SEMU_SAPPORO_BUS_GPIO,
      58u, 0u, 0u, 1, "E-SAP-BUTTONS-001" },
    { SEMU_SAPPORO_WIRE_BUTTON_LOWER, SEMU_SAPPORO_BUS_GPIO,
      59u, 0u, 0u, 1, "E-SAP-BUTTONS-001" },
    { SEMU_SAPPORO_WIRE_BACKLIGHT, SEMU_SAPPORO_BUS_CTIMER,
      9u, 0u, 0u, 0, "E-SAP-BACKLIGHT-001" },
    { SEMU_SAPPORO_WIRE_ACCELEROMETER, SEMU_SAPPORO_BUS_IOM,
      0u, 0x40050000u, 6u, 0, "E-SAP-LSM6DSL-001" },
    { SEMU_SAPPORO_WIRE_PRESSURE, SEMU_SAPPORO_BUS_IOM,
      2u, 0x40052000u, 8u, 0, "E-SAP-HSPPAD143-001" },
    { SEMU_SAPPORO_WIRE_MAGNETOMETER, SEMU_SAPPORO_BUS_IOM,
      2u, 0x40052000u, 8u, 0, "E-SAP-TLI493D-001" },
    { SEMU_SAPPORO_WIRE_HAPTIC, SEMU_SAPPORO_BUS_IOM,
      4u, 0x40054000u, 10u, 0, "E-SAP-HAPTIC-001" },
    { SEMU_SAPPORO_WIRE_AMBIENT_LIGHT, SEMU_SAPPORO_BUS_IOM,
      3u, 0x40053000u, 9u, 0, "E-SAP-OPT3007-001" },
    { SEMU_SAPPORO_WIRE_BATTERY_GAUGE, SEMU_SAPPORO_BUS_IOM,
      4u, 0x40054000u, 10u, 0, "E-SAP-MAX17050-001" },
    { SEMU_SAPPORO_WIRE_GPS, SEMU_SAPPORO_BUS_UART,
      1u, 0x4001d000u, 0u, 0, "E-SAP-CXD5610-001" },
    { SEMU_SAPPORO_WIRE_OPTICAL_HEART_RATE, SEMU_SAPPORO_BUS_IOM,
      2u, 0x40052000u, 8u, 0, "E-SAP-OHR2-001" }
};

#define WIRING_COUNT (sizeof(wiring_table) / sizeof(wiring_table[0]))

const semu_sapporo_wiring *semu_sapporo_wiring_get(size_t *count)
{
    if (count != NULL) {
        *count = WIRING_COUNT;
    }
    return wiring_table;
}

const semu_sapporo_wiring *semu_sapporo_wiring_find(
    semu_sapporo_wire_role role)
{
    size_t i;
    for (i = 0u; i < WIRING_COUNT; ++i) {
        if (wiring_table[i].role == role) {
            return &wiring_table[i];
        }
    }
    return NULL;
}

semu_status semu_sapporo_wiring_validate(semu_error *error)
{
    size_t i;
    size_t j;

    for (i = 0u; i < WIRING_COUNT; ++i) {
        if (wiring_table[i].evidence_id == NULL) {
            semu_error_set(error, SEMU_ERR_STATE,
                           "wiring role %u has no evidence ID",
                           (unsigned)wiring_table[i].role);
            return SEMU_ERR_STATE;
        }
        for (j = i + 1u; j < WIRING_COUNT; ++j) {
            if (wiring_table[i].role == wiring_table[j].role) {
                semu_error_set(error, SEMU_ERR_STATE,
                               "duplicate wiring role %u",
                               (unsigned)wiring_table[i].role);
                return SEMU_ERR_STATE;
            }
        }
    }

    semu_error_clear(error);
    return SEMU_OK;
}
