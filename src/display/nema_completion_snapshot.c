#include "nema_completion_internal.h"

semu_status nema_completion_snapshot_write(
    const nema_completion *comp, semu_snapshot_writer *writer,
    semu_error *error)
{
    size_t index;
    if (comp == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "completion snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (comp->count > UINT32_MAX ||
        comp->active_count > NEMA_COMPLETION_MAX_EVENTS) {
        semu_error_set(error, SEMU_ERR_RANGE, "invalid completion counters");
        return SEMU_ERR_RANGE;
    }
    if (semu_snapshot_writer_u32(writer, (uint32_t)comp->count, error) != SEMU_OK)
        return error->code;
    for (index = 0u; index < NEMA_COMPLETION_MAX_EVENTS; ++index) {
        const completion_entry *entry = &comp->entries[index];
        if (semu_snapshot_writer_u32(writer, entry->list_id, error) != SEMU_OK ||
            semu_snapshot_writer_u64(writer, entry->event_id, error) != SEMU_OK ||
            semu_snapshot_writer_u8(writer, (uint8_t)(entry->active != 0), error) != SEMU_OK)
            return error->code;
    }
    return SEMU_OK;
}

semu_status nema_completion_snapshot_read(
    nema_completion *comp, semu_snapshot_reader *reader, semu_error *error)
{
    nema_completion candidate;
    uint32_t count;
    size_t active_count = 0u;
    size_t index;
    if (comp == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "completion snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *comp;
    if (semu_snapshot_reader_u32(reader, &count, error) != SEMU_OK) {
        return error->code;
    }
    candidate.count = count;
    for (index = 0u; index < NEMA_COMPLETION_MAX_EVENTS; ++index) {
        uint8_t active;
        completion_entry *entry = &candidate.entries[index];
        if (semu_snapshot_reader_u32(reader, &entry->list_id, error) != SEMU_OK ||
            semu_snapshot_reader_u64(reader, &entry->event_id, error) != SEMU_OK ||
            semu_snapshot_reader_u8(reader, &active, error) != SEMU_OK)
            return error->code;
        if (active > 1u) {
            semu_error_set(error, SEMU_ERR_FORMAT, "invalid completion active flag");
            return SEMU_ERR_FORMAT;
        }
        entry->active = active;
        if (active == 0u) {
            continue;
        }
        if (entry->event_id == 0u) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                           "active completion has no event id");
            return SEMU_ERR_FORMAT;
        }
        ++active_count;
        if (active_count > count) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                           "active completions exceed count");
            return SEMU_ERR_FORMAT;
        }
        {
            size_t prior;
            for (prior = 0u; prior < index; ++prior) {
                const completion_entry *previous = &candidate.entries[prior];
                if (previous->active != 0 &&
                    (previous->list_id == entry->list_id ||
                     previous->event_id == entry->event_id)) {
                    semu_error_set(error, SEMU_ERR_FORMAT,
                                   "duplicate active completion identity");
                    return SEMU_ERR_FORMAT;
                }
            }
        }
    }
    candidate.active_count = active_count;
    *comp = candidate;
    for (index = 0u; index < NEMA_COMPLETION_MAX_EVENTS; ++index) {
        comp->entries[index].owner = comp;
    }
    return SEMU_OK;
}

semu_status nema_completion_snapshot_resolve_event(
    nema_completion *comp, uint32_t subject, semu_event_callback *callback,
    void **context, semu_error *error)
{
    if (comp == NULL || callback == NULL || context == NULL ||
        subject >= NEMA_COMPLETION_MAX_EVENTS ||
        comp->entries[subject].active == 0) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "completion snapshot event is not present");
        return SEMU_ERR_CONFLICT;
    }
    *callback = nema_completion_callback;
    *context = &comp->entries[subject];
    return SEMU_OK;
}

semu_status nema_completion_snapshot_event_id_matches(
    const nema_completion *comp, uint32_t subject, semu_event_id event_id,
    semu_error *error)
{
    if (comp != NULL && subject < NEMA_COMPLETION_MAX_EVENTS &&
        comp->entries[subject].active != 0 &&
        comp->entries[subject].event_id == event_id)
        return SEMU_OK;
    semu_error_set(error, SEMU_ERR_FORMAT,
                   "NEMA snapshot event identity does not match completion");
    return SEMU_ERR_FORMAT;
}

semu_status nema_completion_snapshot_event_links_match(
    const nema_completion *comp, const semu_scheduled_event_state *events,
    size_t count, semu_error *error)
{
    size_t index;
    size_t event_index;
    if (comp == NULL || (events == NULL && count != 0u)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "NEMA snapshot event linkage arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    for (index = 0u; index < NEMA_COMPLETION_MAX_EVENTS; ++index) {
        const completion_entry *entry = &comp->entries[index];
        if (entry->active == 0) continue;
        for (event_index = 0u; event_index < count; ++event_index) {
            if (events[event_index].kind == SEMU_SCHED_EVENT_NEMA_COMPLETION &&
                events[event_index].subject == index &&
                events[event_index].id == entry->event_id)
                break;
        }
        if (event_index == count) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                           "NEMA completion state has no scheduler event");
            return SEMU_ERR_FORMAT;
        }
    }
    return SEMU_OK;
}
