#include "armv7m_internal.h"

#include "../../core/scheduler_internal.h"

semu_status semu_cpu_snapshot_event_links_match(
    const semu_cpu *cpu, const semu_scheduled_event_state *events,
    size_t count, semu_error *error)
{
    size_t index;
    if (cpu == NULL || (events == NULL && count != 0u)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "CPU snapshot event linkage arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (cpu->systick_event_valid == 0u) return SEMU_OK;
    for (index = 0u; index < count; ++index) {
        if (events[index].kind == SEMU_SCHED_EVENT_SYSTICK &&
            events[index].subject == 0u &&
            events[index].id == cpu->systick_event)
            return SEMU_OK;
    }
    semu_error_set(error, SEMU_ERR_FORMAT,
                   "CPU SysTick state has no scheduler event");
    return SEMU_ERR_FORMAT;
}
