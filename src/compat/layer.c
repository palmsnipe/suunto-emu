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
    if (descriptor->interventions != NULL) {
        size_t i;
        for (i = 0u; i < descriptor->intervention_count; ++i) {
            ((semu_layer_intervention *)
                &descriptor->interventions[i])->hits = 0u;
        }
    }
    return SEMU_OK;
}

semu_status semu_layer_enable_checked(semu_layer_state *state,
    const semu_layer_descriptor *descriptor,
    const char *profile_id,
    const char *const *actual_hashes, size_t hash_count,
    semu_error *error)
{
    semu_status status;
    size_t i;

    status = semu_layer_enable(state, descriptor, profile_id, error);
    if (status != SEMU_OK) {
        return status;
    }
    if (descriptor->component_hashes == NULL ||
        descriptor->component_hash_count == 0u ||
        actual_hashes == NULL) {
        return SEMU_OK;
    }
    if (hash_count < descriptor->component_hash_count) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "layer %s requires %zu component hashes but %zu provided",
                       descriptor->id, descriptor->component_hash_count,
                       hash_count);
        state->enabled = 0;
        state->descriptor = NULL;
        return SEMU_ERR_STATE;
    }
    for (i = 0u; i < descriptor->component_hash_count; ++i) {
        if (descriptor->component_hashes[i] == NULL ||
            actual_hashes[i] == NULL ||
            strcmp(descriptor->component_hashes[i],
                   actual_hashes[i]) != 0) {
            semu_error_set(error, SEMU_ERR_CONFLICT,
                           "layer %s component hash %zu mismatch",
                           descriptor->id, i);
            state->enabled = 0;
            state->descriptor = NULL;
            return SEMU_ERR_CONFLICT;
        }
    }
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

semu_status semu_layer_intervention_hit(semu_layer_state *state,
    semu_logger *logger, size_t intervention_index, semu_error *error)
{
    const semu_layer_descriptor *desc;
    semu_layer_intervention *iv;

    if (state == NULL || state->descriptor == NULL || !state->enabled) {
        semu_error_set(error, SEMU_ERR_STATE, "disabled layer was invoked");
        return SEMU_ERR_STATE;
    }
    desc = state->descriptor;
    if (intervention_index >= desc->intervention_count ||
        desc->interventions == NULL) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "intervention %zu out of range for layer %s",
                       intervention_index, desc->id);
        return SEMU_ERR_RANGE;
    }
    iv = (semu_layer_intervention *)&desc->interventions[intervention_index];
    if (iv->hits >= iv->max_hits) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "layer %s trigger %s exceeded budget",
                       desc->id, iv->trigger_id);
        return SEMU_ERR_STATE;
    }
    iv->hits++;
    state->hits++;
    semu_log_write(logger, SEMU_LOG_WARNING, "compat", "intervention-hit",
                   "layer=%s trigger=%s ordinal=%llu effect=%s provenance=%s",
                   desc->id, iv->trigger_id,
                   (unsigned long long)iv->hits,
                   iv->effect != NULL ? iv->effect : "unspecified",
                   iv->provenance != NULL ? iv->provenance : "none");
    return SEMU_OK;
}
