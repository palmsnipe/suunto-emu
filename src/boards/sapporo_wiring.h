#ifndef SEMU_BOARDS_SAPPORO_WIRING_H
#define SEMU_BOARDS_SAPPORO_WIRING_H

#include <stddef.h>
#include <stdint.h>

/*
 * Sapporo 2.22.60 verified wiring table (ticket 400).
 * Each record cites a verified device evidence ID.  The external-flash
 * endpoint is owned by the Sapporo device factory, while the still-
 * unverified panel role is omitted, not zero-filled.  The table maps no
 * memory and owns no device.
 */

typedef enum {
    SEMU_SAPPORO_WIRE_BUTTON_UPPER,
    SEMU_SAPPORO_WIRE_BUTTON_MIDDLE,
    SEMU_SAPPORO_WIRE_BUTTON_LOWER,
    SEMU_SAPPORO_WIRE_BACKLIGHT,
    SEMU_SAPPORO_WIRE_ACCELEROMETER,
    SEMU_SAPPORO_WIRE_PRESSURE,
    SEMU_SAPPORO_WIRE_MAGNETOMETER,
    SEMU_SAPPORO_WIRE_HAPTIC,
    SEMU_SAPPORO_WIRE_AMBIENT_LIGHT,
    SEMU_SAPPORO_WIRE_BATTERY_GAUGE,
    SEMU_SAPPORO_WIRE_GPS,
    SEMU_SAPPORO_WIRE_OPTICAL_HEART_RATE,
    SEMU_SAPPORO_WIRE_COUNT
} semu_sapporo_wire_role;

typedef enum {
    SEMU_SAPPORO_BUS_GPIO,
    SEMU_SAPPORO_BUS_CTIMER,
    SEMU_SAPPORO_BUS_IOM,
    SEMU_SAPPORO_BUS_UART
} semu_sapporo_bus_type;

typedef struct {
    semu_sapporo_wire_role role;
    semu_sapporo_bus_type bus;
    uint32_t controller_instance;
    uint32_t address;
    uint32_t irq;
    int active_low;
    const char *evidence_id;
} semu_sapporo_wiring;

const semu_sapporo_wiring *semu_sapporo_wiring_get(size_t *count);

/*
 * Returns the wiring record for role, or NULL if the role is unknown
 * or has no verified evidence (omitted from the table).
 */
const semu_sapporo_wiring *semu_sapporo_wiring_find(
    semu_sapporo_wire_role role);

#include "semu/types.h"
/*
 * Validates that no two wiring records share the same role and that
 * every record has a non-NULL evidence_id.  Returns SEMU_OK on success.
 */
semu_status semu_sapporo_wiring_validate(semu_error *error);

#endif
