#include "armv7m_internal.h"

#define SLEEP_WAKE_NONE 0u
#define SLEEP_WAKE_EVENT 1u
#define SLEEP_WAKE_PENDING 2u
#define SLEEP_WAKE_EXCEPTION 3u
#define SCR_SLEEPONEXIT (1u << 1)

void armv7m_sleep_wfi(semu_cpu *cpu)
{
    if (cpu == NULL) return;
    cpu->sleep_mode = ARMV7M_SLEEP_WFI;
    cpu->sleep_wake_source = SLEEP_WAKE_NONE;
    cpu->state.waiting_for_interrupt = 1;
}

void armv7m_sleep_wfe(semu_cpu *cpu)
{
    if (cpu == NULL) return;
    if (cpu->event_register != 0u) {
        cpu->event_register = 0u;
        cpu->sleep_mode = ARMV7M_SLEEP_NONE;
        cpu->sleep_wake_source = SLEEP_WAKE_EVENT;
        cpu->state.waiting_for_interrupt = 0;
        return;
    }
    cpu->sleep_mode = ARMV7M_SLEEP_WFE;
    cpu->sleep_wake_source = SLEEP_WAKE_NONE;
    cpu->state.waiting_for_interrupt = 1;
}

void armv7m_sleep_event(semu_cpu *cpu)
{
    if (cpu == NULL) return;
    cpu->event_register = 1u;
    if (cpu->state.waiting_for_interrupt &&
        cpu->sleep_mode == ARMV7M_SLEEP_WFE) {
        cpu->event_register = 0u;
        cpu->state.waiting_for_interrupt = 0;
        cpu->sleep_mode = ARMV7M_SLEEP_NONE;
        cpu->sleep_wake_source = SLEEP_WAKE_EVENT;
    }
}

static void clear_sleep(semu_cpu *cpu, unsigned source)
{
    cpu->state.waiting_for_interrupt = 0;
    cpu->sleep_mode = ARMV7M_SLEEP_NONE;
    cpu->sleep_wake_source = (uint8_t)source;
}

semu_status armv7m_sleep_step(semu_cpu *cpu, semu_error *error)
{
    int exception;

    if (cpu == NULL) return SEMU_ERR_ARGUMENT;
    if (cpu->sleep_mode == ARMV7M_SLEEP_WFE && cpu->event_register != 0u) {
        cpu->event_register = 0u;
        clear_sleep(cpu, SLEEP_WAKE_EVENT);
        return SEMU_OK;
    }
    exception = armv7m_pending_exception(cpu);
    if (exception >= 0) {
        clear_sleep(cpu, SLEEP_WAKE_EXCEPTION);
        return armv7m_take_exception(cpu, (unsigned)exception, error);
    }
    if (cpu->sleep_mode == ARMV7M_SLEEP_WFI && armv7m_pending_wake(cpu)) {
        clear_sleep(cpu, SLEEP_WAKE_PENDING);
        return SEMU_OK;
    }
    if (cpu->scheduler != NULL && semu_scheduler_has_events(cpu->scheduler)) {
        semu_status status = semu_scheduler_run_next(cpu->scheduler, error);
        if (status != SEMU_OK) {
            cpu->state.halted = 1;
            cpu->stop_reason = SEMU_STOP_DEVICE_REFUSED;
        }
        return status;
    }
    cpu->state.halted = 1;
    cpu->stop_reason = SEMU_STOP_WFI_DEADLOCK;
    return SEMU_OK;
}

void armv7m_sleep_on_exception_return(semu_cpu *cpu)
{
    if (cpu == NULL || (cpu->scr & SCR_SLEEPONEXIT) == 0u ||
        (cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK) != 0u)
        return;
    armv7m_sleep_wfi(cpu);
}
