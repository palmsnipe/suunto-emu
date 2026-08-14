#ifndef SEMU_COMPAT_H
#define SEMU_COMPAT_H

#include "semu/log.h"
#include "semu/types.h"

typedef enum semu_layer_kind {
    SEMU_LAYER_SYNTHETIC_STATE = 0,
    SEMU_LAYER_DEVICE_FIXTURE,
    SEMU_LAYER_FIRMWARE_HOOK
} semu_layer_kind;

typedef struct semu_layer_descriptor {
    const char *id;
    semu_layer_kind kind;
    const char *profile_id;
    const char *evidence;
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
semu_status semu_layer_hit(semu_layer_state *state, semu_logger *logger,
                           const char *effect, semu_error *error);

#endif
