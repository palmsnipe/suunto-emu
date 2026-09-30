#include "machine_snapshot_events.h"

#include "machine_internal.h"

#include "../cpu/armv7m/armv7m_internal.h"
#include "../devices/sapporo_devices_internal.h"
#include "../soc/apollo4/apollo4_internal.h"

semu_status semu_machine_snapshot_validate_event_id(
    const semu_machine *machine, const semu_scheduled_event_state *state,
    semu_error *error)
{
    if (state->kind == SEMU_SCHED_EVENT_SYSTICK)
        return semu_cpu_snapshot_event_id_matches(
            machine->cpu, state->kind, state->subject, state->id, error);
    if (state->kind == SEMU_SCHED_EVENT_CTIMER ||
        state->kind == SEMU_SCHED_EVENT_STIMER ||
        state->kind == SEMU_SCHED_EVENT_UART_RX ||
        state->kind == SEMU_SCHED_EVENT_UART_TX ||
        state->kind == SEMU_SCHED_EVENT_SAP235_RTC_ALARM)
        return semu_apollo4_snapshot_event_id_matches(
            machine->soc, state->kind, state->subject, state->id, error);
    if (state->kind == SEMU_SCHED_EVENT_CXD_RX ||
        state->kind == SEMU_SCHED_EVENT_CXD_AWAKE)
        return semu_sapporo_devices_snapshot_event_id_matches(
            machine->devices, state->subject, state->id, error);
    if (state->kind == SEMU_SCHED_EVENT_NEMA_COMPLETION)
        return semu_nema_gpu_snapshot_event_id_matches(
            machine->nema_gpu, state->subject, state->id, error);
    semu_error_set(error, SEMU_ERR_FORMAT,
                   "snapshot event kind is unsupported");
    return SEMU_ERR_FORMAT;
}

semu_status semu_machine_snapshot_validate_scheduler_events(
    const semu_machine *machine, semu_error *error)
{
    size_t index;
    size_t count = semu_scheduler_event_count(machine->scheduler);
    for (index = 0u; index < count; ++index) {
        const semu_scheduled_event_state *event =
            semu_scheduler_event_get(machine->scheduler, index);
        if (event == NULL) {
            semu_error_set(error, SEMU_ERR_STATE,
                           "scheduler snapshot event is missing");
            return SEMU_ERR_STATE;
        }
        if (semu_machine_snapshot_validate_event_id(machine, event, error) !=
            SEMU_OK)
            return error->code;
    }
    return SEMU_OK;
}

semu_status semu_machine_snapshot_validate_event_links(
    const semu_machine *machine, const semu_scheduled_event_state *events,
    size_t count, semu_error *error)
{
    semu_status status;
    status = semu_cpu_snapshot_event_links_match(
        machine->cpu, events, count, error);
    if (status != SEMU_OK) return status;
    status = semu_apollo4_snapshot_event_links_match(
        machine->soc, events, count, error);
    if (status != SEMU_OK) return status;
    status = semu_sapporo_devices_snapshot_event_links_match(
        machine->devices, events, count, error);
    if (status != SEMU_OK) return status;
    return semu_nema_gpu_snapshot_event_links_match(
        machine->nema_gpu, events, count, error);
}
