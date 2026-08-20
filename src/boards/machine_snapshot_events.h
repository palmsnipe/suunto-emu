#ifndef SEMU_MACHINE_SNAPSHOT_EVENTS_H
#define SEMU_MACHINE_SNAPSHOT_EVENTS_H

#include "semu/machine.h"
#include "../core/scheduler_internal.h"

semu_status semu_machine_snapshot_validate_event_id(
    const semu_machine *machine, const semu_scheduled_event_state *state,
    semu_error *error);
semu_status semu_machine_snapshot_validate_scheduler_events(
    const semu_machine *machine, semu_error *error);
semu_status semu_machine_snapshot_validate_event_links(
    const semu_machine *machine, const semu_scheduled_event_state *events,
    size_t count, semu_error *error);

#endif
