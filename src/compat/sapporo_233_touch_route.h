#ifndef SEMU_SAPPORO_233_TOUCH_ROUTE_H
#define SEMU_SAPPORO_233_TOUCH_ROUTE_H

#include "semu/bus.h"
#include "semu/compat.h"
#include "semu/cpu.h"
#include "semu/peripheral.h"

/*
 * E-SAP-0127 (owner-authorized 2026-10-20): the synthetic 2.33 touch
 * route. The owner asked to try making the touch screen work, which is
 * the explicit owner decision E-SAP-0122 named as the only unblock path
 * besides a provisioned lane image.
 *
 * The layer injects a synthetic hw_touch_1 route entry at the
 * firmware-derived route-manager entry-list seam (0x164a8e, E-SAP-0126)
 * and provides the IOM3 touch endpoint (0x40053000, address 0x24,
 * register 0x2f, E-SAP-0120 probe shape). The record layout and the
 * probe shape are firmware-derived; only the enablement values are
 * synthetic and every synthetic value is named and logged as such.
 * The layer is opt-in, hash-pinned, hit-bounded and logged.
 */
#define SEMU_SAPPORO_233_TOUCH_ROUTE_PC UINT32_C(0x00164a8e)
/* The TMA5xx sink that builds the 12-byte touch event record at sp
 * (E-SAP-0120): +0 X u16, +2 Y u16, +4 float, +8 float, +12 type. */
#define SEMU_SAPPORO_233_TOUCH_ROUTE_SINK_PC UINT32_C(0x00100e72)
/* The uiEngine event queue (E-SAP-0120): the firmware's own
 * queue -> uiEngine -> touchBuffer path. */
#define SEMU_SAPPORO_233_TOUCH_ROUTE_QUEUE_PC UINT32_C(0x000a712e)
#define SEMU_SAPPORO_233_TOUCH_ROUTE_MAX_HITS UINT64_C(64)

/* The synthetic route entry: the firmware-exhibited hw_touch_1 name and
 * the firmware-exhibited route type 12, plus the synthetic enablement
 * (routeE/touchE = 1). */
#define SEMU_SAPPORO_233_TOUCH_ROUTE_NAME "hw_touch_1"
#define SEMU_SAPPORO_233_TOUCH_ROUTE_TYPE 12u

typedef struct {
    semu_layer_state *state;
    semu_logger *logger;
    semu_error refusal;
    int installed;
    uint64_t hits;
    /* The synthetic route entry payload: nameID/routeID/routeE/touchE.
     * Only the enablement is synthetic; the layout follows the
     * firmware-derived route-entry record. */
    uint32_t name_id;
    uint32_t route_id;
    uint32_t route_e;
    uint32_t touch_e;
} semu_sapporo_233_touch_route_context;

extern const semu_layer_descriptor semu_sapporo_233_touch_route_layer;

/* Installs the synthetic route entry and logs the install line. */
semu_status semu_sapporo_233_touch_route_install(
    semu_sapporo_233_touch_route_context *c, semu_bus *bus,
    semu_error *error);

/* Applies the route-manager entry-list seam: at 0x164a8e the synthetic
 * entry is appended to the firmware's route-entry list. Returns
 * SEMU_OK when the PC does not match the layer. */
semu_status semu_sapporo_233_touch_route_apply(
    semu_sapporo_233_touch_route_context *c, semu_bus *bus,
    semu_cpu_state *cpu, semu_error *error);

/* The IOM3 touch endpoint (address 0x24, register 0x2f, 12-byte report,
 * 1200x1200 raw, 0.2 scale). */
semu_serial_endpoint semu_sapporo_233_touch_route_endpoint(
    semu_sapporo_233_touch_route_context *c);

#endif
