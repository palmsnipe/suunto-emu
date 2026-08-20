#ifndef SEMU_MACHINE_SNAPSHOT_SCHEDULER_H
#define SEMU_MACHINE_SNAPSHOT_SCHEDULER_H

#include "../core/scheduler_internal.h"
#include "../core/snapshot_io.h"

typedef struct semu_machine_scheduler_image {
    uint64_t now;
    uint64_t next_sequence;
    semu_event_id next_id;
    semu_scheduled_event_state *events;
    size_t count;
} semu_machine_scheduler_image;

semu_status semu_machine_snapshot_write_scheduler(
    const semu_scheduler *scheduler, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_machine_snapshot_read_scheduler(
    semu_snapshot_reader *reader, semu_machine_scheduler_image *image,
    semu_error *error);
void semu_machine_snapshot_free_scheduler_image(
    semu_machine_scheduler_image *image);

#endif
