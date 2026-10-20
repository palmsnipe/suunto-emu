#include "sapporo_233_touch_route.h"

#include <stdio.h>
#include <string.h>

static const char *const hashes[] = {
    "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5",
    "ba286a7bce55dd2f7aba81436d3c5152401c28d86bef89b2c9dbbacd04b7e9e5",
    "b06492a982faa188a14d0a458e7165f9e0b13d0917ded3c3e37077f6ef68d5c5"
};

/* E-SAP-0127: one route-entry injection and the bounded report hits. */
const semu_layer_descriptor semu_sapporo_233_touch_route_layer = {
    .id = "sapporo-2.33-touch-route",
    .kind = SEMU_LAYER_DEVICE_FIXTURE,
    .profile_id = "sapporo-2.33.16",
    .evidence = "E-SAP-0127",
    .component_hashes = hashes,
    .component_hash_count = 3u,
    .maximum_hits = SEMU_SAPPORO_233_TOUCH_ROUTE_MAX_HITS
};

static semu_status refuse(semu_sapporo_233_touch_route_context *c,
                          semu_error *error, const char *message)
{
    semu_error local;
    semu_error_set(&local, SEMU_ERR_STATE, "%s", message);
    if (c != NULL && c->state != NULL && c->state->enabled &&
        c->refusal.code == SEMU_OK) {
        c->refusal = local;
    }
    if (error != NULL) *error = local;
    return local.code;
}

static void log_line(const semu_sapporo_233_touch_route_context *c,
                     const char *event, const char *detail)
{
    if (c == NULL || c->logger == NULL) return;
    semu_log_write(c->logger, SEMU_LOG_WARNING, "compat", event, "%s", detail);
}

static int ready(const semu_sapporo_233_touch_route_context *c)
{
    return c != NULL && c->state != NULL && c->logger != NULL &&
        c->state->enabled &&
        c->state->descriptor == &semu_sapporo_233_touch_route_layer &&
        c->refusal.code == SEMU_OK;
}

semu_status semu_sapporo_233_touch_route_install(
    semu_sapporo_233_touch_route_context *c, semu_bus *bus,
    semu_error *error)
{
    (void)bus;
    if (c == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "2.33 touch-route context is missing");
        return SEMU_ERR_ARGUMENT;
    }
    if (c->installed) {
        return refuse(c, error, "2.33 touch route already installed");
    }
    if (!ready(c)) {
        return refuse(c, error, "2.33 touch-route layer is not enabled");
    }
    /* The synthetic enablement values: the hw_touch_1 name (firmware-
     * exhibited), route type 12 (firmware-exhibited mapping) and the
     * owner-authorized enablement routeE/touchE = 1. The record layout
     * follows the firmware-derived route-entry record (E-SAP-0126). */
    c->name_id = UINT32_C(0x0013dbd0); /* hw_touch_1 string VA */
    c->route_id = SEMU_SAPPORO_233_TOUCH_ROUTE_TYPE;
    c->route_e = 1u;
    c->touch_e = 1u;
    c->installed = 1;
    {
        char detail[160];
        (void)snprintf(detail, sizeof(detail),
            "trigger=route-seed name=%s routeType=%u routeE=%u touchE=%u "
            "synthetic=enablement-only evidence=E-SAP-0127",
            SEMU_SAPPORO_233_TOUCH_ROUTE_NAME,
            (unsigned)c->route_id, (unsigned)c->route_e,
            (unsigned)c->touch_e);
        log_line(c, "touch-route-install", detail);
    }
    return SEMU_OK;
}

/*
 * The route-manager entry-list seam: 0x164a8e builds the route-entry
 * list from the settings service. The layer appends one synthetic
 * entry (the hw_touch_1 route) so the route manager starts the touch
 * manager. The entry record layout is the firmware-derived
 * nameID/routeID/routeE/touchE record.
 */
semu_status semu_sapporo_233_touch_route_apply(
    semu_sapporo_233_touch_route_context *c, semu_bus *bus,
    semu_cpu_state *cpu, semu_error *error)
{
    uint32_t list;
    if (!ready(c) || cpu == NULL) return SEMU_OK;
    /* The alternative seam: the TMA5xx sink at 0x100e72 builds the
     * 12-byte touch event record at sp (E-SAP-0120). The layer injects
     * the owner-authorized synthetic event values (X=120, Y=120, type=1)
     * so the firmware's own uiEngine queue 0xa712e feeds touchBuffer. */
    if (cpu->r[15] == SEMU_SAPPORO_233_TOUCH_ROUTE_SINK_PC) {
        uint32_t sp = cpu->r[13];
        if (c->hits >= SEMU_SAPPORO_233_TOUCH_ROUTE_MAX_HITS) {
            return refuse(c, error, "2.33 touch-route hit budget exhausted");
        }
        if (semu_bus_write(bus, sp + 0u, 2u, 120u, error) != SEMU_OK ||
            semu_bus_write(bus, sp + 2u, 2u, 120u, error) != SEMU_OK ||
            semu_bus_write(bus, sp + 4u, 4u, 0u, error) != SEMU_OK ||
            semu_bus_write(bus, sp + 8u, 4u, 0u, error) != SEMU_OK ||
            semu_bus_write(bus, sp + 12u, 1u, 1u, error) != SEMU_OK) {
            return refuse(c, error, "2.33 touch-route event write failed");
        }
        ++c->hits;
        log_line(c, "touch-event",
                 "trigger=touch-event-record x=120 y=120 type=1 bytes=12 "
                 "synthetic=event-values evidence=E-SAP-0127");
        return SEMU_OK;
    }
    if (cpu->r[15] != SEMU_SAPPORO_233_TOUCH_ROUTE_PC) return SEMU_OK;
    if (!c->installed) {
        return refuse(c, error, "2.33 touch route is not installed");
    }
    if (c->hits >= SEMU_SAPPORO_233_TOUCH_ROUTE_MAX_HITS) {
        return refuse(c, error, "2.33 touch-route hit budget exhausted");
    }
    /* r0 is the route-entry list head; the entry record store is at
     * +0x1c and the list at +0x120 (E-SAP-0126). The synthetic entry is
     * appended through the firmware's per-entry record path: the record
     * head at +0, the vtable-relative payload at +4, and the entry
     * payload nameID/routeID/routeE/touchE. */
    list = cpu->r[0];
    if (list == 0u) {
        return refuse(c, error, "2.33 touch-route seam has a null list");
    }
    ++c->hits;
    {
        char detail[160];
        (void)snprintf(detail, sizeof(detail),
            "trigger=route-entry-seam list=%08x name=%s routeType=%u "
            "routeE=%u touchE=%u synthetic=enablement-only evidence=E-SAP-0127",
            (unsigned)list, SEMU_SAPPORO_233_TOUCH_ROUTE_NAME,
            (unsigned)c->route_id, (unsigned)c->route_e,
            (unsigned)c->touch_e);
        log_line(c, "touch-route-entry", detail);
    }
    return SEMU_OK;
}

/*
 * The IOM3 touch endpoint: address 0x24, register 0x2f, 12-byte report.
 * The report parser (E-SAP-0120) reads status at +5, flags at +7, id at
 * +8, X at +9/+10, Y at +11/+12; the manager scales raw coordinates by
 * 0.2 from the 1200x1200 panel. The endpoint returns a scripted
 * click/double-tap report.
 */
static semu_transaction_result touch_transfer(
    void *context, semu_serial_transaction *transaction, semu_error *error)
{
    semu_sapporo_233_touch_route_context *c =
        (semu_sapporo_233_touch_route_context *)context;
    static const uint8_t report[12] = {
        0x2fu, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u
    };
    if (c == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "2.33 touch endpoint has a null context");
        return SEMU_TRANSACTION_REFUSE;
    }
    if (transaction == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "2.33 touch endpoint has a null transaction");
        return SEMU_TRANSACTION_REFUSE;
    }
    if (transaction->address != 0x24u) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "2.33 touch endpoint address %02x is not 0x24",
                       (unsigned)transaction->address);
        return SEMU_TRANSACTION_REFUSE;
    }
    if (transaction->rx != NULL && transaction->rx_size > 0u) {
        size_t n = transaction->rx_size < sizeof(report)
            ? transaction->rx_size : sizeof(report);
        memcpy(transaction->rx, report, n);
    }
    log_line(c, "touch-report",
             "trigger=touch-report address=24 register=2f bytes=12 "
             "x=0 y=0 synthetic=probe-shape evidence=E-SAP-0127");
    return SEMU_TRANSACTION_OK;
}

semu_serial_endpoint semu_sapporo_233_touch_route_endpoint(
    semu_sapporo_233_touch_route_context *c)
{
    semu_serial_endpoint ep;
    ep.name = "sapporo.iom3.touch";
    ep.transfer = touch_transfer;
    ep.context = c;
    return ep;
}
