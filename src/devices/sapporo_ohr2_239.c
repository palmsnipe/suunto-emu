#include "sapporo_ohr2_239.h"

#include <string.h>

static int payload_fill_matches(const uint8_t *payload, size_t first,
                                uint8_t value)
{
    size_t index;

    for (index = first; index < SEMU_SAPPORO_OHR2_PAYLOAD_SIZE; ++index) {
        if (payload[index] != value) return 0;
    }
    return 1;
}

static semu_transaction_result refuse(semu_error *error, const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Sapporo 2.39 OHR2 refuses %s", reason);
    return SEMU_TRANSACTION_REFUSE;
}

semu_transaction_result semu_sapporo_239_ohr_body_provider(
    semu_sapporo_ohr2_command command, uint16_t sequence,
    semu_sapporo_ohr2_state state,
    const uint8_t request_payload[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE],
    uint8_t response_payload[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE],
    semu_error *error)
{
    (void)sequence;
    if (request_payload == NULL || response_payload == NULL) {
        return refuse(error, "null body buffer");
    }
    if (command == SEMU_SAPPORO_OHR2_COMMAND_BOOT_MODE) {
        if (request_payload[4u] != 1u ||
            !payload_fill_matches(request_payload, 5u, 0xffu)) {
            return refuse(error, "boot-mode body");
        }
        memset(response_payload, 0, SEMU_SAPPORO_OHR2_PAYLOAD_SIZE);
        semu_error_clear(error);
        return SEMU_TRANSACTION_OK;
    }
    if (command == SEMU_SAPPORO_OHR2_COMMAND_RESULT_13) {
        if (state != SEMU_SAPPORO_OHR2_MAIN ||
            !payload_fill_matches(request_payload, 4u, 0xffu)) {
            return refuse(error, "result-13 body");
        }
        memset(response_payload, 0, SEMU_SAPPORO_OHR2_PAYLOAD_SIZE);
        semu_error_clear(error);
        return SEMU_TRANSACTION_OK;
    }
    if (command != SEMU_SAPPORO_OHR2_COMMAND_IDENTITY ||
        (state != SEMU_SAPPORO_OHR2_BSL &&
         state != SEMU_SAPPORO_OHR2_MAIN) ||
        !payload_fill_matches(request_payload, 4u, 0xffu)) {
        return refuse(error, "unimplemented identity body");
    }
    memset(response_payload, 0, SEMU_SAPPORO_OHR2_PAYLOAD_SIZE);
    if (state == SEMU_SAPPORO_OHR2_MAIN) {
        memcpy(response_payload + 9u, "MAIN", 4u);
    } else {
        memcpy(response_payload + 9u, "BSL", 3u);
    }
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}
