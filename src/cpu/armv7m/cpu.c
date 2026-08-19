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

static int thumb16_it_instruction_preserves_flags(uint16_t instruction)
{
    unsigned operation;

    /* In an IT block, 16-bit instructions do not update APSR except for
       CMP, CMN, and TST. */
    if ((instruction & 0xf800u) == 0x2800u) {
        return 0;
    }
    if ((instruction & 0xfc00u) == 0x4000u) {
        operation = (instruction >> 6u) & 15u;
        return operation != 8u && operation != 10u && operation != 11u;
    }
    if ((instruction & 0xfc00u) == 0x4400u) {
        return ((instruction >> 8u) & 3u) != 1u;
    }
    return 1;
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
    {
        static const semu_bus_device_ops scs_ops = {
            armv7m_scs_read, armv7m_scs_write, armv7m_scs_reset
        };
        if (semu_bus_map_overlay(bus, "armv7m.scs", ARMV7M_SCS_BASE,
                                 ARMV7M_SCS_SIZE, &scs_ops, cpu, error) !=
            SEMU_OK) {
            free(cpu);
            return NULL;
        }
    }
    return cpu;
}

void semu_cpu_destroy(semu_cpu *cpu)
{
    if (cpu != NULL) semu_bus_unmap_overlay(cpu->bus, cpu);
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
    armv7m_scs_reset(cpu);
    cpu->stop_reason = SEMU_STOP_NONE;
    cpu->fault_instruction = 0u;
    cpu->fault_address = 0u;
    cpu->has_fault_address = 0u;
    cpu->vector_table = vector_table & ~0x7fu;
    cpu->prigroup = 0u;
    cpu->pending_source_count = 0u;
    cpu->ccr = 1u << 9;
    cpu->itstate = 0u;
    cpu->event_register = 0u;
    cpu->sleep_mode = ARMV7M_SLEEP_NONE;
    cpu->sleep_wake_source = 0u;
    cpu->stack_align = 1u;
    armv7m_clear_exclusive(cpu);
    if (armv7m_read(cpu, cpu->vector_table, 4u, &initial_sp, error) != SEMU_OK ||
        armv7m_read(cpu, cpu->vector_table + 4u, 4u, &initial_pc, error) !=
            SEMU_OK) {
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
    return armv7m_sleep_step(cpu, error);
}

semu_status semu_cpu_step(semu_cpu *cpu, semu_error *error)
{
    uint32_t first_value;
    uint32_t second_value;
    uint32_t pc;
    uint32_t packed;
    uint32_t it_flags;
    uint16_t first;
    uint16_t second = 0u;
    uint8_t old_itstate;
    int irq;
    int is_wide;
    int preserve_it_flags;
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
    if ((cpu->state.xpsr & ARMV7M_XPSR_T) == 0u) {
        return armv7m_request_fault(cpu, 6u,
                                     ARMV7M_CFSR_UFSR_INVSTATE, 0u, 0, error);
    }
    irq = armv7m_pending_exception(cpu);
    if (irq >= 0) {
        return armv7m_take_exception(cpu, (unsigned)irq, error);
    }

    pc = cpu->state.r[15];
    if ((pc & 1u) != 0u) {
        return armv7m_unsupported(cpu, pc, error);
    }
    if (armv7m_read(cpu, pc, 2u, &first_value, error) != SEMU_OK) {
        if (cpu->state.halted)
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        return finish_instruction(cpu, error);
    }
    first = (uint16_t)first_value;
    is_wide = instruction_is_32bit(first);
    if (is_wide) {
        if (armv7m_read(cpu, pc + 2u, 2u, &second_value, error) != SEMU_OK) {
            if (cpu->state.halted)
                return error != NULL ? error->code : SEMU_ERR_RANGE;
            return finish_instruction(cpu, error);
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
    it_flags = cpu->state.xpsr & (ARMV7M_XPSR_N | ARMV7M_XPSR_Z |
                                  ARMV7M_XPSR_C | ARMV7M_XPSR_V);
    preserve_it_flags = old_itstate != 0u && !is_wide &&
                        thumb16_it_instruction_preserves_flags(first);
    status = is_wide ? armv7m_exec32(cpu, first, second, pc, error)
                     : armv7m_exec16(cpu, first, pc, error);
    if (status != SEMU_OK) {
        if (cpu->state.halted) return status;
        /*
         * A fault exception was taken (BusFault or HardFault) without
         * halting. The handler has updated the PC and cleared the IT
         * state. Finish the instruction so the run loop can step into
         * the handler on the next call.
        */
        return finish_instruction(cpu, error);
    }
    /* Thumb MOVS-immediate is flag-preserving when it is non-final in IT. */
    if (preserve_it_flags) {
        cpu->state.xpsr = (cpu->state.xpsr &
                           ~(ARMV7M_XPSR_N | ARMV7M_XPSR_Z |
                             ARMV7M_XPSR_C | ARMV7M_XPSR_V)) | it_flags;
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
        int was_pending = cpu->irq_level[irq] != 0u ||
                          cpu->irq_pending[irq] != 0u;
        cpu->irq_level[irq] = level != 0 ? 1u : 0u;
        if (cpu->irq_level[irq] != 0u) {
            cpu->irq_source_bits[irq / 64u] |=
                UINT64_C(1) << (irq % 64u);
        } else if (cpu->irq_pending[irq] == 0u) {
            cpu->irq_source_bits[irq / 64u] &=
                ~(UINT64_C(1) << (irq % 64u));
        }
        if (!was_pending && cpu->irq_level[irq] != 0u) {
            if (cpu->pending_source_count != UINT16_MAX) {
                ++cpu->pending_source_count;
            }
        } else if (was_pending && cpu->irq_level[irq] == 0u &&
                   cpu->irq_pending[irq] == 0u &&
                   cpu->pending_source_count != 0u) {
            --cpu->pending_source_count;
        }
        if (!was_pending && level != 0)
            armv7m_signal_pending_event(cpu, 16u + irq);
    }
}

void semu_cpu_set_irq_priority(semu_cpu *cpu, unsigned irq, uint8_t priority)
{
    if (cpu != NULL && irq < ARMV7M_IRQ_COUNT) {
        cpu->irq_priority[irq] = priority & ARMV7M_NVIC_PRIORITY_MASK;
    }
}

void semu_cpu_signal_event(semu_cpu *cpu)
{
    armv7m_sleep_event(cpu);
}

int semu_cpu_reset_requested(const semu_cpu *cpu)
{
    return cpu != NULL && cpu->reset_requested != 0u;
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
