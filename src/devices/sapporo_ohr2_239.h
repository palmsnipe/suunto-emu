#ifndef SEMU_SAPPORO_OHR2_239_H
#define SEMU_SAPPORO_OHR2_239_H

#include "sapporo_ohr2.h"

semu_transaction_result semu_sapporo_239_ohr_body_provider(
    semu_sapporo_ohr2_command command, uint16_t sequence,
    semu_sapporo_ohr2_state state,
    const uint8_t request_payload[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE],
    uint8_t response_payload[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE],
    semu_error *error);

#endif
