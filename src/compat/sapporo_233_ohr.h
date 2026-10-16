#ifndef SEMU_SAPPORO_233_OHR_H
#define SEMU_SAPPORO_233_OHR_H

#include "semu/bus.h"
#include "semu/compat.h"

#include "../devices/sapporo_ohr2.h"

/* Ticket 710 instance-36: the 2.33.16 OHR2 startup fixture. The
 * E-SAP-0074 next-refusal boundary is the guest's first IOM2 address-0x10
 * exchange; the lane answered the captured request frame with the shared
 * transport law (header echo, synthetic identity/zero bodies, CRC32) and
 * the reboot command advances BSL -> MAIN without a response. The fixture
 * is a body provider behind the existing sapporo OHR2 device transport;
 * framing, ready GPIO and sequence handling stay the device's law. */

typedef struct {
    semu_layer_state *state;
    semu_logger *logger;
    semu_error refusal;
} semu_sapporo_233_ohr_context;

extern const semu_layer_descriptor semu_sapporo_233_ohr_layer;

/* The OHR2 device body provider for the 2.33.16 layer. Admits exactly the
 * staged-window request order (each once), answers with the lane law, and
 * refusals are sticky until semu_sapporo_233_ohr_reset. */
semu_transaction_result semu_sapporo_233_ohr_body_provider(
    void *context, semu_sapporo_ohr2_command command, uint16_t sequence,
    semu_sapporo_ohr2_state state,
    const uint8_t request_payload[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE],
    uint8_t response_payload[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE],
    semu_error *error);

/* Clears the sticky refusal and restarts the response round. The layer
 * state pointer is preserved. */
void semu_sapporo_233_ohr_reset(semu_sapporo_233_ohr_context *context);

#endif
