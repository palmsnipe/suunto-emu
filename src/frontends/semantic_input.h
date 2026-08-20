#ifndef SEMU_FRONTENDS_SEMANTIC_INPUT_H
#define SEMU_FRONTENDS_SEMANTIC_INPUT_H

#include "semu/input.h"
#include "semu/types.h"

/*
 * Default key codes for the built-in map.  These are frontend defaults,
 * not hardware evidence (E-SAP-BUTTONS-001 proves GPIO 57/58/59 map to
 * firmware IDs 0/1/2, not host key choices).
 */
#define SEMU_INPUT_KEY_UPPER   0u
#define SEMU_INPUT_KEY_MIDDLE  1u
#define SEMU_INPUT_KEY_LOWER   2u
#define SEMU_INPUT_KEY_COUNT   3u

/*
 * Normalized key event consumed by the mapper.  Frontend-agnostic:
 * the SDL adapter (sdl_input.c) and the replay parser both produce
 * this struct.
 */
typedef struct semu_normalized_key {
    uint32_t key;       /* frontend key code */
    int32_t down;       /* 1 = press edge, 0 = release edge */
    int32_t repeat;     /* 1 if auto-repeat, 0 if physical edge */
    uint32_t sequence;  /* monotonic ordering for determinism */
} semu_normalized_key;

/* Key-to-button map entry for custom configuration. */
typedef struct semu_input_key_map_entry {
    uint32_t key_code;
    semu_button_id button;
} semu_input_key_map_entry;

/* Opaque mapper state.  Tracks three pressed bits internally. */
typedef struct semu_input_mapper semu_input_mapper;

/* Lifecycle.  Default map: key 0->UPPER, 1->MIDDLE, 2->LOWER. */
semu_input_mapper *semu_input_mapper_create(semu_error *error);
void semu_input_mapper_destroy(semu_input_mapper *mapper);
void semu_input_mapper_reset(semu_input_mapper *mapper);

/* Resolve a host key without changing pressed state. */
int semu_input_mapper_button_for_key(const semu_input_mapper *mapper,
    uint32_t key_code, semu_button_id *out_button);

/*
 * Set a custom key map.  Rejects duplicate key codes, duplicate buttons, and
 * out-of-range button IDs.  Unspecified buttons are unmapped.  Resets pressed
 * state on success.
 */
semu_status semu_input_mapper_set_map(semu_input_mapper *mapper,
    const semu_input_key_map_entry *entries, uint32_t count,
    semu_error *error);

/*
 * Process one normalized key event.  Emits zero or one semu_input_event.
 * Repeats emit nothing.  Duplicate press/release suppressed.
 * Sets *out_has_event = 0 when no event is emitted.
 * Emitted events use kind=SEMU_INPUT_BUTTON, code=button_id,
 * value=0 for press (active-low), value=1 for release.
 */
semu_status semu_input_mapper_process(semu_input_mapper *mapper,
    const semu_normalized_key *key, semu_input_event *out_event,
    int32_t *out_has_event, semu_error *error);

/*
 * Emit release events for all currently pressed buttons in
 * upper/middle/lower order.  Clears pressed state.
 */
semu_status semu_input_mapper_focus_loss(semu_input_mapper *mapper,
    semu_input_event *out_events, uint32_t max_events,
    uint32_t *out_count, semu_error *error);

#endif
