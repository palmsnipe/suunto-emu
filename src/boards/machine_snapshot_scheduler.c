#include "machine_snapshot_scheduler.h"

#include <stdlib.h>

semu_status semu_machine_snapshot_write_scheduler(
    const semu_scheduler *scheduler, semu_snapshot_writer *writer,
    semu_error *error)
{
    size_t index;
    size_t count = semu_scheduler_event_count(scheduler);
    if (count > UINT32_MAX) {
        semu_error_set(error, SEMU_ERR_RANGE, "scheduler snapshot has too many events");
        return SEMU_ERR_RANGE;
    }
    for (index = 0u; index < count; ++index) {
        const semu_scheduled_event_state *event =
            semu_scheduler_event_get(scheduler, index);
        if (event == NULL) {
            semu_error_set(error, SEMU_ERR_STATE,
                           "scheduler snapshot event is missing");
            return SEMU_ERR_STATE;
        }
        if (event->kind == SEMU_SCHED_EVENT_NONE ||
            event->kind > SEMU_SCHED_EVENT_NEMA_COMPLETION) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                           "scheduler snapshot event kind is unsupported");
            return SEMU_ERR_FORMAT;
        }
    }
    if (semu_snapshot_writer_u64(writer, scheduler->now_ns, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, scheduler->next_sequence, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, scheduler->next_id, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, (uint32_t)count, error) != SEMU_OK)
        return error->code;
    for (index = 0u; index < count; ++index) {
        const semu_scheduled_event_state *event =
            semu_scheduler_event_get(scheduler, index);
        if (event == NULL ||
            semu_snapshot_writer_u64(writer, event->due_ns, error) != SEMU_OK ||
            semu_snapshot_writer_u64(writer, event->sequence, error) != SEMU_OK ||
            semu_snapshot_writer_u64(writer, event->id, error) != SEMU_OK ||
            semu_snapshot_writer_u32(writer, event->kind, error) != SEMU_OK ||
            semu_snapshot_writer_u32(writer, event->subject, error) != SEMU_OK)
            return error->code != SEMU_OK ? error->code : SEMU_ERR_STATE;
    }
    return SEMU_OK;
}

semu_status semu_machine_snapshot_read_scheduler(
    semu_snapshot_reader *reader, semu_machine_scheduler_image *image,
    semu_error *error)
{
    uint32_t count;
    size_t index;
    if (semu_snapshot_reader_u64(reader, &image->now, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &image->next_sequence, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &image->next_id, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &count, error) != SEMU_OK)
        return error->code;
    if (image->next_id == 0u || image->next_id == UINT64_MAX ||
        image->next_sequence == UINT64_MAX || count > SEMU_SNAPSHOT_MAX_SECTION_SIZE / 32u) {
        semu_error_set(error, SEMU_ERR_FORMAT, "invalid scheduler snapshot header");
        return SEMU_ERR_FORMAT;
    }
    image->events = count == 0u ? NULL :
        (semu_machine_snapshot_event *)calloc(count, sizeof(*image->events));
    if (count != 0u && image->events == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate scheduler snapshot");
        return SEMU_ERR_NOMEM;
    }
    image->count = count;
    for (index = 0u; index < image->count; ++index) {
        semu_scheduled_event_state *event = &image->events[index].state;
        if (semu_snapshot_reader_u64(reader, &event->due_ns, error) != SEMU_OK ||
            semu_snapshot_reader_u64(reader, &event->sequence, error) != SEMU_OK ||
            semu_snapshot_reader_u64(reader, &event->id, error) != SEMU_OK ||
            semu_snapshot_reader_u32(reader, &event->kind, error) != SEMU_OK ||
            semu_snapshot_reader_u32(reader, &event->subject, error) != SEMU_OK) {
            free(image->events);
            image->events = NULL;
            image->count = 0u;
            return error->code;
        }
        if (event->id == 0u || event->sequence >= image->next_sequence ||
            event->id >= image->next_id || event->due_ns < image->now ||
            event->kind == SEMU_SCHED_EVENT_NONE ||
            event->kind > SEMU_SCHED_EVENT_NEMA_COMPLETION) {
            free(image->events);
            image->events = NULL;
            image->count = 0u;
            semu_error_set(error, SEMU_ERR_FORMAT, "invalid scheduler snapshot event");
            return SEMU_ERR_FORMAT;
        }
    }
    return SEMU_OK;
}

void semu_machine_snapshot_free_scheduler_image(
    semu_machine_scheduler_image *image)
{
    if (image != NULL) {
        free(image->events);
        image->events = NULL;
        image->count = 0u;
    }
}
