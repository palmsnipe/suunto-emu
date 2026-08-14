#include "armv7m_internal.h"

#include <stdlib.h>
#include <string.h>

static void sync_itstate(semu_cpu *cpu)
{
    uint32_t mask = (3u << 25) | (0x3fu << 10);
    cpu->state.xpsr &= ~mask;
    cpu->state.xpsr |= ((uint32_t)cpu->itstate & 3u) << 25;
    cpu->state.xpsr |= ((uint32_t)cpu->itstate >> 2) << 10;
}

static void advance_itstate(semu_cpu *cpu)
{
    if ((cpu->itstate & 7u) == 0u) {
        cpu->itstate = 0u;
    } else {
        cpu->itstate = (uint8_t)((cpu->itstate & 0xe0u) |
                                 ((cpu->itstate << 1) & 0x1fu));
    }
    sync_itstate(cpu);
}

static int instruction_is_32bit(uint16_t instruction)
{
    unsigned prefix = instruction >> 11;
    return prefix == 0x1du || prefix == 0x1eu || prefix == 0x1fu;
}

static int pending_irq(const semu_cpu *cpu)
{
    unsigned irq;

    if (cpu->state.primask != 0u || cpu->state.faultmask != 0u ||
        cpu->state.basepri != 0u || (cpu->state.xpsr & 0x1ffu) != 0u) {
        return -1;
    }
    for (irq = 0u; irq < ARMV7M_IRQ_COUNT; ++irq) {
        if (cpu->irq_level[irq] != 0u) {
            return (int)irq;
        }
    }
    return -1;
}

static semu_status finish_instruction(semu_cpu *cpu, semu_error *error)
{
    semu_status status;

    cpu->state.instructions++;
    if (cpu->scheduler == NULL) {
        return SEMU_OK;
    }
    status = semu_scheduler_advance(cpu->scheduler, 1u, error);
    if (status != SEMU_OK) {
        cpu->state.halted = 1;
        cpu->stop_reason = SEMU_STOP_DEVICE_REFUSED;
    }
    return status;
}

semu_cpu *semu_cpu_create(semu_bus *bus, semu_scheduler *scheduler,
                          semu_error *error)
{
    semu_cpu *cpu;

    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "CPU requires a memory bus");
        return NULL;
    }
    cpu = (semu_cpu *)calloc(1u, sizeof(*cpu));
    if (cpu == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate CPU");
        return NULL;
    }
    cpu->bus = bus;
    cpu->scheduler = scheduler;
    return cpu;
}

void semu_cpu_destroy(semu_cpu *cpu)
{
    free(cpu);
}

void semu_cpu_reset(semu_cpu *cpu, uint32_t vector_table, semu_error *error)
{
    uint32_t initial_sp;
    uint32_t initial_pc;

    if (cpu == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "cannot reset a null CPU");
        return;
    }
    memset(&cpu->state, 0, sizeof(cpu->state));
    memset(cpu->irq_level, 0, sizeof(cpu->irq_level));
    memset(cpu->irq_priority, 0, sizeof(cpu->irq_priority));
    cpu->stop_reason = SEMU_STOP_NONE;
    cpu->fault_instruction = 0u;
    cpu->fault_address = 0u;
    cpu->has_fault_address = 0u;
    cpu->vector_table = vector_table;
    cpu->itstate = 0u;
    cpu->event_register = 0u;
    cpu->stack_align = 1u;
    armv7m_clear_exclusive(cpu);
    if (armv7m_read(cpu, vector_table, 4u, &initial_sp, error) != SEMU_OK ||
        armv7m_read(cpu, vector_table + 4u, 4u, &initial_pc, error) != SEMU_OK) {
        return;
    }
    if ((initial_pc & 1u) == 0u) {
        cpu->state.halted = 1;
        cpu->stop_reason = SEMU_STOP_FIRMWARE_ASSERT;
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "reset vector 0x%08lx is not Thumb code",
                       (unsigned long)initial_pc);
        return;
    }
    cpu->state.msp = initial_sp;
    cpu->state.r[13] = initial_sp;
    cpu->state.r[15] = initial_pc & ~1u;
    cpu->state.xpsr = ARMV7M_XPSR_T;
}

static semu_status step_waiting_cpu(semu_cpu *cpu, semu_error *error)
{
    int irq = pending_irq(cpu);

    if (irq >= 0) {
        cpu->state.waiting_for_interrupt = 0;
        return armv7m_take_exception(cpu, 16u + (unsigned)irq, error);
    }
    if (cpu->scheduler != NULL && semu_scheduler_has_events(cpu->scheduler)) {
        return semu_scheduler_run_next(cpu->scheduler, error);
    }
    cpu->state.halted = 1;
    cpu->stop_reason = SEMU_STOP_WFI_DEADLOCK;
    return SEMU_OK;
}

semu_status semu_cpu_step(semu_cpu *cpu, semu_error *error)
{
    uint32_t first_value;
    uint32_t second_value;
    uint32_t pc;
    uint32_t packed;
    uint16_t first;
    uint16_t second = 0u;
    uint8_t old_itstate;
    int irq;
    int is_wide;
    semu_status status;

    if (cpu == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "cannot step a null CPU");
        return SEMU_ERR_ARGUMENT;
    }
    if (cpu->state.halted) {
        return SEMU_OK;
    }
    if (cpu->state.waiting_for_interrupt) {
        return step_waiting_cpu(cpu, error);
    }
    irq = pending_irq(cpu);
    if (irq >= 0) {
        return armv7m_take_exception(cpu, 16u + (unsigned)irq, error);
    }

    pc = cpu->state.r[15];
    if ((pc & 1u) != 0u) {
        return armv7m_unsupported(cpu, pc, error);
    }
    if (armv7m_read(cpu, pc, 2u, &first_value, error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_RANGE;
    }
    first = (uint16_t)first_value;
    is_wide = instruction_is_32bit(first);
    if (is_wide) {
        if (armv7m_read(cpu, pc + 2u, 2u, &second_value, error) != SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        }
        second = (uint16_t)second_value;
        packed = ((uint32_t)first << 16) | second;
    } else {
        packed = first;
    }
    cpu->fault_instruction = packed;

    old_itstate = cpu->itstate;
    if (old_itstate != 0u && !armv7m_condition_passed(cpu, old_itstate >> 4)) {
        cpu->state.r[15] = pc + (is_wide ? 4u : 2u);
        advance_itstate(cpu);
        return finish_instruction(cpu, error);
    }
    status = is_wide ? armv7m_exec32(cpu, first, second, pc, error)
                     : armv7m_exec16(cpu, first, pc, error);
    if (status != SEMU_OK) {
        return status;
    }
    if (old_itstate != 0u) {
        advance_itstate(cpu);
    }
    return finish_instruction(cpu, error);
}

const semu_cpu_state *semu_cpu_get_state(const semu_cpu *cpu)
{
    return cpu != NULL ? &cpu->state : NULL;
}

semu_cpu_state *semu_cpu_get_state_mutable(semu_cpu *cpu)
{
    return cpu != NULL ? &cpu->state : NULL;
}

void semu_cpu_set_irq(semu_cpu *cpu, unsigned irq, int level)
{
    if (cpu != NULL && irq < ARMV7M_IRQ_COUNT) {
        cpu->irq_level[irq] = level != 0 ? 1u : 0u;
    }
}

void semu_cpu_set_irq_priority(semu_cpu *cpu, unsigned irq, uint8_t priority)
{
    if (cpu != NULL && irq < ARMV7M_IRQ_COUNT) {
        cpu->irq_priority[irq] = priority;
    }
}

void semu_cpu_signal_event(semu_cpu *cpu)
{
    if (cpu != NULL) {
        cpu->event_register = 1u;
    }
}

semu_stop_reason semu_cpu_stop_reason(const semu_cpu *cpu)
{
    return cpu != NULL ? cpu->stop_reason : SEMU_STOP_FIRMWARE_ASSERT;
}

uint32_t semu_cpu_fault_instruction(const semu_cpu *cpu)
{
    return cpu != NULL ? cpu->fault_instruction : 0u;
}

int semu_cpu_fault_address(const semu_cpu *cpu, uint32_t *address)
{
    if (cpu == NULL || cpu->has_fault_address == 0u) {
        if (address != NULL) {
            *address = 0u;
        }
        return 0;
    }
    if (address != NULL) {
        *address = cpu->fault_address;
    }
    return 1;
}

void armv7m_set_itstate(semu_cpu *cpu, uint8_t value)
{
    cpu->itstate = value;
    sync_itstate(cpu);
}
