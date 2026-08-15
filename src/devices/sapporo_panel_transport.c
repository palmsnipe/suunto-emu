/*
 * Sapporo 2.22 panel transport (ticket 404).
 * Evidence: E-SAP-PANEL-001 is MISSING.  All panel transfers refuse
 * fail-closed.  No DCS/DSI commands are invented; no pixels are
 * fabricated from staging rows.  This stub exists so that wiring
 * (ticket 420) can attach the endpoint without guessing commands.
 */

#include "sapporo_panel_transport.h"

#include <stdlib.h>

struct semu_sapporo_panel_transport {
    int unused;
};

static semu_transaction_result panel_transfer(
    void *context, semu_serial_transaction *transaction, semu_error *error)
{
    (void)context;
    (void)transaction;
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "panel transport: E-SAP-PANEL-001 evidence is missing; "
                   "all panel commands refuse");
    return SEMU_TRANSACTION_REFUSE;
}

static const semu_serial_endpoint panel_endpoint = {
    .name = "sapporo.panel",
    .transfer = panel_transfer,
    .context = NULL
};

semu_sapporo_panel_transport *semu_sapporo_panel_transport_create(
    semu_error *error)
{
    semu_sapporo_panel_transport *panel;
    (void)error;
    panel = (semu_sapporo_panel_transport *)calloc(1u, sizeof(*panel));
    return panel;
}

void semu_sapporo_panel_transport_destroy(
    semu_sapporo_panel_transport *panel)
{
    free(panel);
}

void semu_sapporo_panel_transport_reset(
    semu_sapporo_panel_transport *panel)
{
    (void)panel;
}

semu_serial_endpoint semu_sapporo_panel_transport_endpoint(
    semu_sapporo_panel_transport *panel)
{
    (void)panel;
    return panel_endpoint;
}
