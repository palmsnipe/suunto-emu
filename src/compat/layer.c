#include "semu/compat.h"

#include <string.h>

semu_status semu_layer_enable(semu_layer_state *state,
                              const semu_layer_descriptor *descriptor,
                              const char *profile_id, semu_error *error)
{
    if (state == NULL || descriptor == NULL || profile_id == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid compatibility layer");
        return SEMU_ERR_ARGUMENT;
    }
    if (strcmp(descriptor->profile_id, profile_id) != 0) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "layer %s is pinned to profile %s",
                       descriptor->id, descriptor->profile_id);
        return SEMU_ERR_CONFLICT;
    }
    state->descriptor = descriptor;
    state->hits = 0u;
    state->enabled = 1;
    return SEMU_OK;
}

semu_status semu_layer_hit(semu_layer_state *state, semu_logger *logger,
                           const char *effect, semu_error *error)
{
    if (state == NULL || state->descriptor == NULL || !state->enabled) {
        semu_error_set(error, SEMU_ERR_STATE, "disabled layer was invoked");
        return SEMU_ERR_STATE;
    }
    if (state->hits >= state->descriptor->maximum_hits) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "layer %s exceeded its hit budget",
                       state->descriptor->id);
        return SEMU_ERR_STATE;
    }
    state->hits++;
    semu_log_write(logger, SEMU_LOG_WARNING, "compat", "layer-hit",
                   "layer=%s hit=%llu maximum=%llu effect=%s evidence=%s",
                   state->descriptor->id,
                   (unsigned long long)state->hits,
                   (unsigned long long)state->descriptor->maximum_hits,
                   effect != NULL ? effect : "unspecified",
                   state->descriptor->evidence);
    return SEMU_OK;
}
