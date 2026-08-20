#include "semantic_input.h"

#include <stdlib.h>
#include <string.h>

#define MAPPER_BUTTON_COUNT 3u

struct semu_input_mapper {
    uint32_t key_map[MAPPER_BUTTON_COUNT]; /* key_code per button slot */
    int valid[MAPPER_BUTTON_COUNT];        /* configured map slots */
    int pressed[MAPPER_BUTTON_COUNT];       /* pressed state per button */
};

semu_input_mapper *semu_input_mapper_create(semu_error *error)
{
    semu_input_mapper *m;
    uint32_t i;
    m = (semu_input_mapper *)calloc(1u, sizeof(*m));
    if (m == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate input mapper");
        return NULL;
    }
    m->key_map[SEMU_BUTTON_UPPER] = SEMU_INPUT_KEY_UPPER;
    m->key_map[SEMU_BUTTON_MIDDLE] = SEMU_INPUT_KEY_MIDDLE;
    m->key_map[SEMU_BUTTON_LOWER] = SEMU_INPUT_KEY_LOWER;
    for (i = 0u; i < MAPPER_BUTTON_COUNT; ++i) {
        m->valid[i] = 1;
    }
    return m;
}

void semu_input_mapper_destroy(semu_input_mapper *mapper)
{
    free(mapper);
}

void semu_input_mapper_reset(semu_input_mapper *mapper)
{
    if (mapper == NULL) {
        return;
    }
    memset(mapper->pressed, 0, sizeof(mapper->pressed));
}

semu_status semu_input_mapper_set_map(semu_input_mapper *mapper,
    const semu_input_key_map_entry *entries, uint32_t count,
    semu_error *error)
{
    uint32_t i;
    uint32_t j;
    if (mapper == NULL || entries == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "mapper set_map: null argument");
        return SEMU_ERR_ARGUMENT;
    }
    if (count > MAPPER_BUTTON_COUNT) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "key map count exceeds %u", MAPPER_BUTTON_COUNT);
        return SEMU_ERR_ARGUMENT;
    }
    for (i = 0u; i < count; ++i) {
        if (entries[i].button >= MAPPER_BUTTON_COUNT) {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                           "key map button %u out of range",
                           entries[i].button);
            return SEMU_ERR_ARGUMENT;
        }
        for (j = i + 1u; j < count; ++j) {
            if (entries[i].key_code == entries[j].key_code) {
                semu_error_set(error, SEMU_ERR_ARGUMENT,
                               "duplicate key code %u in map",
                               entries[i].key_code);
                return SEMU_ERR_ARGUMENT;
            }
            if (entries[i].button == entries[j].button) {
                semu_error_set(error, SEMU_ERR_ARGUMENT,
                               "duplicate button %u in map",
                               entries[i].button);
                return SEMU_ERR_ARGUMENT;
            }
        }
    }
    memset(mapper->key_map, 0, sizeof(mapper->key_map));
    memset(mapper->valid, 0, sizeof(mapper->valid));
    memset(mapper->pressed, 0, sizeof(mapper->pressed));
    for (i = 0u; i < count; ++i) {
        mapper->key_map[entries[i].button] = entries[i].key_code;
        mapper->valid[entries[i].button] = 1;
    }
    return SEMU_OK;
}

static int find_button(const semu_input_mapper *mapper, uint32_t key_code,
                       uint32_t *out_button)
{
    uint32_t i;
    for (i = 0u; i < MAPPER_BUTTON_COUNT; ++i) {
        if (mapper->valid[i] && mapper->key_map[i] == key_code) {
            *out_button = i;
            return 1;
        }
    }
    return 0;
}

int semu_input_mapper_button_for_key(const semu_input_mapper *mapper,
    uint32_t key_code, semu_button_id *out_button)
{
    uint32_t button;
    if (mapper == NULL || out_button == NULL ||
        !find_button(mapper, key_code, &button)) {
        return 0;
    }
    *out_button = (semu_button_id)button;
    return 1;
}

semu_status semu_input_mapper_process(semu_input_mapper *mapper,
    const semu_normalized_key *key, semu_input_event *out_event,
    int32_t *out_has_event, semu_error *error)
{
    uint32_t button;
    if (mapper == NULL || key == NULL || out_event == NULL ||
        out_has_event == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "mapper process: null argument");
        return SEMU_ERR_ARGUMENT;
    }
    *out_has_event = 0;
    if (key->repeat) {
        return SEMU_OK;
    }
    if (!find_button(mapper, key->key, &button)) {
        return SEMU_OK;
    }
    if (key->down) {
        if (mapper->pressed[button]) {
            return SEMU_OK;
        }
    } else {
        if (!mapper->pressed[button]) {
            return SEMU_OK;
        }
    }
    mapper->pressed[button] = key->down ? 1 : 0;
    out_event->kind = SEMU_INPUT_BUTTON;
    out_event->code = button;
    out_event->value = key->down ? 0 : 1;
    out_event->x = 0;
    out_event->y = 0;
    *out_has_event = 1;
    return SEMU_OK;
}

semu_status semu_input_mapper_focus_loss(semu_input_mapper *mapper,
    semu_input_event *out_events, uint32_t max_events,
    uint32_t *out_count, semu_error *error)
{
    uint32_t count = 0u;
    uint32_t i;
    if (mapper == NULL || out_events == NULL || out_count == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "mapper focus_loss: null argument");
        return SEMU_ERR_ARGUMENT;
    }
    *out_count = 0u;
    for (i = 0u; i < MAPPER_BUTTON_COUNT; ++i) {
        if (mapper->pressed[i]) {
            if (count >= max_events) {
                semu_error_set(error, SEMU_ERR_RANGE,
                               "focus_loss event overflow at button %u", i);
                return SEMU_ERR_RANGE;
            }
            ++count;
        }
    }
    count = 0u;
    for (i = 0u; i < MAPPER_BUTTON_COUNT; ++i) {
        if (mapper->pressed[i]) {
            out_events[count].kind = SEMU_INPUT_BUTTON;
            out_events[count].code = i;
            out_events[count].value = 1;
            out_events[count].x = 0;
            out_events[count].y = 0;
            mapper->pressed[i] = 0;
            ++count;
        }
    }
    *out_count = count;
    return SEMU_OK;
}
