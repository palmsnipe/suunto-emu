#ifndef SEMU_SAPPORO_PANEL_TRANSPORT_H
#define SEMU_SAPPORO_PANEL_TRANSPORT_H

#include "semu/peripheral.h"
#include "semu/types.h"

/*
 * Sapporo 2.22 panel transport (ticket 404).
 * Evidence: E-SAP-PANEL-001 is MISSING.  All panel commands refuse
 * fail-closed until physical MSPI evidence is verified.  The module
 * exposes a serial endpoint that always returns TRANSACTION_REFUSE.
 */

typedef struct semu_sapporo_panel_transport semu_sapporo_panel_transport;

semu_sapporo_panel_transport *semu_sapporo_panel_transport_create(
    semu_error *error);
void semu_sapporo_panel_transport_destroy(
    semu_sapporo_panel_transport *panel);
void semu_sapporo_panel_transport_reset(
    semu_sapporo_panel_transport *panel);
semu_serial_endpoint semu_sapporo_panel_transport_endpoint(
    semu_sapporo_panel_transport *panel);

#endif
