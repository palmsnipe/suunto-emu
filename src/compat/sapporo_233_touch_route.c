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
        /* Idempotent re-install after a guest reset: the layer is
         * already seeded, keep the same synthetic values. */
        return semu_layer_hit(c->state, c->logger,
            "trigger=route-seed-reinstall name=hw_touch_1 routeType=12 "
            "routeE=1 touchE=1 already-installed idempotent E-SAP-0127",
            error);
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
    /* The parser loop's trailing byte load 0x0fa928 computes
     * r4 + (count-1)*2. When the parsed count is zero the firmware still
     * reaches the load with r6 = 0xffffffff, which overflows the address
     * (armv7m_add_address) and halts the run. The firmware's own guard
     * (0x0fa922: cbnz r6) is skipped on this path, so the layer restores
     * the intended skip to 0x0fa946 and logs the bounded recovery. */
    if (cpu->r[15] == UINT32_C(0x000fa928) &&
        cpu->r[6] == UINT32_C(0xffffffff)) {
        if (c->hits >= SEMU_SAPPORO_233_TOUCH_ROUTE_MAX_HITS) {
            return refuse(c, error, "2.33 touch-route hit budget exhausted");
        }
        cpu->r[15] = UINT32_C(0x000fa946);
        ++c->hits;
        log_line(c, "parser-recover",
                 "trigger=empty-count-load pc=000fa928 r6=ffffffff "
                 "skip=000fa946 synthetic=bounded-recovery evidence=E-SAP-0127");
        return SEMU_OK;
    }
    /* The touch manager constructor 0x100a66: capture the manager
     * object and its return address, so the layer can redirect the
     * return into the manager's own 0x2205 event handler. */
    if (cpu->r[15] == SEMU_SAPPORO_233_TOUCH_ROUTE_MANAGER_PC) {
        if (c->hits >= SEMU_SAPPORO_233_TOUCH_ROUTE_MAX_HITS) {
            return refuse(c, error, "2.33 touch-route hit budget exhausted");
        }
        c->manager = cpu->r[0];
        c->return_pc = cpu->r[14];
        c->pending = 1u;
        return SEMU_OK;
    }
    /* Before the event dispatch, start the manager's activity through
     * its own start handler 0x100b07 (r0 = manager + 8), so the
     * firmware's periodic tick owns the manager and can drive the
     * TMA5xx driver's IOM3 poll. */
    if (c->pending == 1u && cpu->r[15] == c->return_pc) {
        if (c->hits >= SEMU_SAPPORO_233_TOUCH_ROUTE_MAX_HITS) {
            return refuse(c, error, "2.33 touch-route hit budget exhausted");
        }
        cpu->r[0] = c->manager + 8u;
        cpu->r[14] = c->return_pc;
        cpu->r[15] = UINT32_C(0x00100b06);
        c->pending = 3u;
        ++c->hits;
        log_line(c, "touch-event",
                 "trigger=manager-start r0=manager+8 handler=100b07 "
                 "synthetic=activity-start evidence=E-SAP-0127");
        return SEMU_OK;
    }
    /* The redirect: at the start handler's return, run the manager's
     * 0x2205 handler 0x100b68 with the owner-authorized synthetic touch
     * values. The handler calls the uiEngine queue 0xa712e and the
     * TMA5xx driver, so the firmware's own queue -> uiEngine ->
     * touchBuffer path runs. The stack argument holds the X/Y value. */
    if (c->pending == 3u && cpu->r[15] == c->return_pc) {
        uint32_t sp;
        if (c->hits >= SEMU_SAPPORO_233_TOUCH_ROUTE_MAX_HITS) {
            return refuse(c, error, "2.33 touch-route hit budget exhausted");
        }
        sp = cpu->r[13] - 4u;
        if (semu_bus_write(bus, sp, 4u, 120u, error) != SEMU_OK) {
            return refuse(c, error,
                          "2.33 touch-route redirect stack write failed");
        }
        cpu->r[13] = sp;
        cpu->r[14] = c->return_pc;
        cpu->r[0] = c->manager + 0x1cu;
        cpu->r[1] = 120u;
        cpu->r[2] = 0u;
        cpu->r[3] = 0u;
        cpu->r[15] = SEMU_SAPPORO_233_TOUCH_ROUTE_EVENT_PC;
        c->pending = 2u;
        ++c->hits;
        log_line(c, "touch-event",
                 "trigger=manager-redirect x=120 y=120 type=0 kind=0x2205 "
                 "synthetic=event-values evidence=E-SAP-0127");
        return SEMU_OK;
    }
    if (c->pending == 2u && cpu->r[15] == c->return_pc) {
        cpu->r[13] += 4u;
        c->pending = 0u;
        return SEMU_OK;
    }
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
    /* The uiEngine queue seam: the touch manager's event handler calls
     * the queue 0xa712e with the E-SAP-0120 12-byte record. The layer
     * injects the owner-authorized synthetic event values so the
     * firmware's own queue -> uiEngine -> touchBuffer path runs. */
    if (cpu->r[15] == SEMU_SAPPORO_233_TOUCH_ROUTE_QUEUE_PC) {
        uint32_t record = cpu->r[2];
        if (c->hits >= SEMU_SAPPORO_233_TOUCH_ROUTE_MAX_HITS) {
            return refuse(c, error, "2.33 touch-route hit budget exhausted");
        }
        if (semu_bus_write(bus, record + 0u, 2u, 120u, error) != SEMU_OK ||
            semu_bus_write(bus, record + 2u, 2u, 120u, error) != SEMU_OK ||
            semu_bus_write(bus, record + 4u, 4u, 0u, error) != SEMU_OK ||
            semu_bus_write(bus, record + 8u, 4u, 0u, error) != SEMU_OK ||
            semu_bus_write(bus, record + 12u, 1u, 1u, error) != SEMU_OK) {
            return refuse(c, error, "2.33 touch-route queue write failed");
        }
        ++c->hits;
        log_line(c, "touch-event",
                 "trigger=uiengine-queue x=120 y=120 type=1 bytes=12 "
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
    /* 0x164a8e is the iterator constructor: r0 is the iterator and the
     * first statement stores the container (the route record) into
     * iter[0] (E-SAP-0127). The entry array view sits at container+0x98
     * (data pointer, byte size, cursor, count). */
    list = cpu->r[0];
    if (list == 0u) {
        return refuse(c, error, "2.33 touch-route seam has a null list");
    }
    /* The hook runs before the instruction executes, so the container
     * is still in r1 (the constructor's param_2). */
    list = cpu->r[1];
    if (list == 0u) {
        return refuse(c, error,
                      "2.33 touch-route seam has a null container");
    }
    /* The route record's entry array view sits at +0x98: data pointer,
     * byte size (a multiple of the 0x30-byte entry stride), cursor
     * (= data) and the entry count (E-SAP-0127). Read it so the layer
     * can verify the derived layout against the live guest before it
     * ever writes an entry. */
    {
        uint32_t data = 0u;
        uint32_t size = 0u;
        uint32_t cursor = 0u;
        uint32_t count = 0u;
        char detail[200];
        if (semu_bus_read(bus, list + 0x98u, 4u, &data, error) != SEMU_OK ||
            semu_bus_read(bus, list + 0x9cu, 4u, &size, error) != SEMU_OK ||
            semu_bus_read(bus, list + 0xa0u, 4u, &cursor, error) != SEMU_OK ||
            semu_bus_read(bus, list + 0xa4u, 4u, &count, error) != SEMU_OK) {
            return refuse(c, error,
                          "2.33 touch-route entry array read failed");
        }
        if (size % 0x30u != 0u) {
            return refuse(c, error,
                          "2.33 touch-route entry array size is not a "
                          "multiple of 0x30");
        }
        if (cursor != data || count * 0x30u > size) {
            return refuse(c, error,
                          "2.33 touch-route entry array view is "
                          "inconsistent");
        }
        /* Owner-authorized (2026-10-20): append the synthetic
         * hw_touch_1 entry. The firmware's find-or-create appends at
         * data + count*0x30, writes the filter's high u16 into the
         * entry's first word (FUN_0017168c copies src[0]; FUN_00164a5e
         * matches on that word) and clears the sub-object's +0x24
         * (FUN_0017167c). The observed filters are all
         * (id << 16) | 0x000f, so the touch filter is
         * 0x000c000f - route type 12 (E-SAP-0120) in the high u16 and
         * the same 0x000f tag as the provisioned routes. */
        {
            unsigned k;
            int seen = 0;
            for (k = 0u; k < c->written_count; ++k) {
                if (c->written_lists[k] == list) seen = 1;
            }
            if (!seen && c->written_count < 4u &&
                count < size / 0x30u) {
            uint32_t entry = data + count * 0x30u;
            unsigned i;
            if (semu_bus_write(bus, entry, 2u, c->route_id, error)
                    != SEMU_OK) {
                return refuse(c, error,
                              "2.33 touch-route entry write failed");
            }
            for (i = 2u; i < 0x30u; ++i) {
                if (semu_bus_write(bus, entry + i, 1u, 0u, error)
                        != SEMU_OK) {
                    return refuse(c, error,
                                  "2.33 touch-route entry write failed");
                }
            }
            if (semu_bus_write(bus, list + 0xa4u, 4u, count + 1u, error)
                    != SEMU_OK) {
                return refuse(c, error,
                              "2.33 touch-route entry count write failed");
            }
            c->written_lists[c->written_count++] = list;
            c->entry_written = 1u;
            ++c->hits;
            (void)snprintf(detail, sizeof(detail),
                "trigger=route-entry-write list=%08x entry=%08x "
                "type=%u filter=%08x name=%s routeType=%u routeE=%u "
                "touchE=%u synthetic=enablement-only evidence=E-SAP-0127",
                (unsigned)list, (unsigned)entry, (unsigned)c->route_id,
                (unsigned)(c->route_id << 16 | 0x000fu),
                SEMU_SAPPORO_233_TOUCH_ROUTE_NAME,
                (unsigned)c->route_id, (unsigned)c->route_e,
                (unsigned)c->touch_e);
            log_line(c, "touch-route-entry", detail);
            return SEMU_OK;
            }
        }
        ++c->hits;
        (void)snprintf(detail, sizeof(detail),
            "trigger=route-entry-seam list=%08x data=%08x size=%u "
            "count=%u filter=%08x index=%u name=%s routeType=%u "
            "routeE=%u touchE=%u synthetic=enablement-only "
            "evidence=E-SAP-0127",
            (unsigned)list, (unsigned)data, (unsigned)size,
            (unsigned)count, (unsigned)cpu->r[2], (unsigned)cpu->r[3],
            SEMU_SAPPORO_233_TOUCH_ROUTE_NAME,
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
