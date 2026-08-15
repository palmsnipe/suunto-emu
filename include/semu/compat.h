#ifndef SEMU_COMPAT_H
#define SEMU_COMPAT_H

#include <stddef.h>

#include "semu/log.h"
#include "semu/types.h"

typedef enum semu_layer_kind {
    SEMU_LAYER_SYNTHETIC_STATE = 0,
    SEMU_LAYER_DEVICE_FIXTURE,
    SEMU_LAYER_FIRMWARE_HOOK
} semu_layer_kind;

typedef struct semu_layer_intervention {
    const char *trigger_id;
    const char *effect;
    const char *provenance;
    uint64_t max_hits;
    uint64_t hits;
} semu_layer_intervention;

typedef struct semu_layer_descriptor {
    const char *id;
    semu_layer_kind kind;
    const char *profile_id;
    const char *evidence;
    const char *const *component_hashes;
    size_t component_hash_count;
    const semu_layer_intervention *interventions;
    size_t intervention_count;
    uint64_t maximum_hits;
} semu_layer_descriptor;

typedef struct semu_layer_state {
    const semu_layer_descriptor *descriptor;
    uint64_t hits;
    int enabled;
} semu_layer_state;

semu_status semu_layer_enable(semu_layer_state *state,
                              const semu_layer_descriptor *descriptor,
                              const char *profile_id, semu_error *error);

/*
 * Extended enable: validates that every component_hash in the descriptor
 * matches the corresponding entry in actual_hashes.  When
 * component_hash_count is zero or hashes is NULL, behaves like
 * semu_layer_enable (profile-id check only).
 */
semu_status semu_layer_enable_checked(semu_layer_state *state,
    const semu_layer_descriptor *descriptor,
    const char *profile_id,
    const char *const *actual_hashes, size_t hash_count,
    semu_error *error);

semu_status semu_layer_hit(semu_layer_state *state, semu_logger *logger,
                           const char *effect, semu_error *error);

/*
 * Records a hit on the named intervention.  Emits a structured event
 * with layer/trigger/ordinal/effect fields.  Returns SEMU_ERR_STATE
 * when the layer is disabled, the intervention index is out of range,
 * or the intervention's per-trigger budget is exhausted.
 */
semu_status semu_layer_intervention_hit(semu_layer_state *state,
    semu_logger *logger, size_t intervention_index, semu_error *error);

#endif
