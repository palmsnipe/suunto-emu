#ifndef SEMU_SAPPORO_235_OHR_H
#define SEMU_SAPPORO_235_OHR_H

#include "semu/compat.h"
#include "../devices/sapporo_ohr2.h"

extern const semu_layer_descriptor semu_sapporo_235_ohr_layer;

typedef struct {
    semu_layer_state *state;
    semu_logger *logger;
    semu_error refusal;
} semu_sapporo_235_ohr_context;

semu_transaction_result semu_sapporo_235_ohr_body_provider(
    void *context, semu_sapporo_ohr2_command command, uint16_t sequence,
    semu_sapporo_ohr2_state state,
    const uint8_t request[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE],
    uint8_t response[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE], semu_error *error);

#endif
