#include "sapporo_222.h"

#include <string.h>

static int intervention_is_unused(const semu_layer_state *state,
                                   size_t intervention_index)
{
    if (state == NULL || state->descriptor == NULL ||
        intervention_index >= state->descriptor->intervention_count ||
        state->descriptor->interventions == NULL) {
        return 0;
    }
    return state->descriptor->interventions[intervention_index].hits == 0u;
}

semu_transaction_result semu_sapporo_222_ohr_body_provider(
    void *context, semu_sapporo_ohr2_command command, uint16_t sequence,
    semu_sapporo_ohr2_state state,
    const uint8_t request_payload[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE],
    uint8_t response_payload[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE],
    semu_error *error)
{
    semu_sapporo_222_fixture_context *ctx =
        (semu_sapporo_222_fixture_context *)context;
    (void)sequence;

    switch (command) {
    case SEMU_SAPPORO_OHR2_COMMAND_IDENTITY:
        memset(response_payload, 0, SEMU_SAPPORO_OHR2_PAYLOAD_SIZE);
        if (state == SEMU_SAPPORO_OHR2_MAIN) {
            memcpy(response_payload + 9u, "MAIN", 4u);
        } else {
            memcpy(response_payload + 9u, "BSL", 3u);
        }
        response_payload[13u] = 0u;
        break;
    case SEMU_SAPPORO_OHR2_COMMAND_CONFIGURE:
    case SEMU_SAPPORO_OHR2_COMMAND_RESULT_13:
    case SEMU_SAPPORO_OHR2_COMMAND_RESULT_14:
        memset(response_payload, 0, SEMU_SAPPORO_OHR2_PAYLOAD_SIZE);
        break;
    case SEMU_SAPPORO_OHR2_COMMAND_ECHO:
        memset(response_payload, 0, SEMU_SAPPORO_OHR2_PAYLOAD_SIZE);
        memcpy(response_payload + 4u, request_payload + 4u,
               SEMU_SAPPORO_OHR2_PAYLOAD_SIZE - 4u);
        break;
    default:
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "OHR fixture: unexpected command %u",
                       (unsigned)command);
        return SEMU_TRANSACTION_REFUSE;
    }
    if (ctx != NULL && ctx->state != NULL && ctx->logger != NULL &&
        intervention_is_unused(ctx->state, SEMU_SAPPORO_222_IV_OHR_STARTUP)) {
        if (semu_layer_intervention_hit(ctx->state, ctx->logger,
                SEMU_SAPPORO_222_IV_OHR_STARTUP, error) != SEMU_OK) {
            return SEMU_TRANSACTION_REFUSE;
        }
    }
    return SEMU_TRANSACTION_OK;
}
