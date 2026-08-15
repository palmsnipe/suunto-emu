/*
 * CPU fault report population (ticket 605).
 * Borrows immutable CPU inspection state; never reads guest memory
 * after the fault or mutates stop state.
 */

#include "semu/cpu.h"
#include "semu/trace.h"

#include <string.h>

void semu_report_fault_from_cpu(semu_report_fault *report,
    const semu_cpu *cpu)
{
    const semu_cpu_state *state;

    if (report == NULL || cpu == NULL) {
        return;
    }
    state = semu_cpu_get_state(cpu);
    if (state == NULL) {
        return;
    }
    report->stop_reason = semu_cpu_stop_reason(cpu);
    report->fault_instruction = semu_cpu_fault_instruction(cpu);
    report->has_fault_address = semu_cpu_fault_address(cpu,
        &report->fault_address);
    memcpy(report->r, state->r, sizeof(report->r));
    report->xpsr = state->xpsr;
    report->primask = state->primask;
    report->basepri = state->basepri;
    report->faultmask = state->faultmask;
    report->control = state->control;
    report->fpscr = state->fpscr;
    report->instructions = state->instructions;
}
