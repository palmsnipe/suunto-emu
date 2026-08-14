#include "armv7m_internal.h"

static semu_status take_exception_internal(semu_cpu *cpu,
                                            unsigned exception,
                                            semu_error *error);

static int valid_exception_return(uint32_t token)
{
    return token == 0xffffffe1u || token == 0xffffffe9u ||
           token == 0xffffffedu || token == 0xfffffff1u ||
           token == 0xfffffff9u || token == 0xfffffffdu;
}

static int extended_exception_return(uint32_t token)
{
    return (token & 0x10u) == 0u;
}

static semu_status request_usage_fault(semu_cpu *cpu, uint32_t token,
                                       semu_error *error)
{
    cpu->state.r[14] = token;
    return take_exception_internal(cpu, 6u, error);
}

static semu_status exception_return(semu_cpu *cpu, uint32_t token,
                                    semu_error *error)
{
    uint32_t frame[8];
    uint32_t sp;
    uint32_t frame_end;
    uint32_t restored_sp;
    unsigned current_exception;
    unsigned index;
    int extended;
    int to_process;
    int to_handler;
    semu_status status;

    if (!valid_exception_return(token))
        return request_usage_fault(cpu, token, error);
    if ((cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK) == 0u)
        return request_usage_fault(cpu, token, error);
    extended = extended_exception_return(token);
    to_handler = (token & 0xfu) == 1u;
    to_process = (token & 0xfu) == 0xdu;
    sp = to_process ? cpu->state.psp : cpu->state.msp;
    for (index = 0u; index < 8u; ++index) {
        uint32_t address;
        if (armv7m_add_address(cpu, sp, index * 4u, &address, error) !=
                SEMU_OK ||
            armv7m_read(cpu, address, 4u, &frame[index], error) != SEMU_OK)
            return error != NULL ? error->code : SEMU_ERR_RANGE;
    }
    if ((frame[7] & ARMV7M_XPSR_T) == 0u || (frame[6] & 1u) != 0u ||
        (to_handler == ((frame[7] & ARMV7M_XPSR_IPSR_MASK) == 0u)))
        return request_usage_fault(cpu, token, error);
    current_exception = cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK;
    status = armv7m_fpu_context_unstack(cpu, sp, extended, error);
    if (status != SEMU_OK) return status;
    if ((cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK) != current_exception)
        return SEMU_OK;
    if (armv7m_add_address(cpu, sp, extended ? 0x68u : 0x20u, &frame_end,
                           error) != SEMU_OK)
        return error != NULL ? error->code : SEMU_ERR_RANGE;
    restored_sp = frame_end;
    if ((extended || cpu->stack_align) != 0u &&
        (frame[7] & ARMV7M_XPSR_STACK_ALIGN) != 0u) {
        if (armv7m_add_address(cpu, restored_sp, 4u, &restored_sp, error) !=
            SEMU_OK)
            return error != NULL ? error->code : SEMU_ERR_RANGE;
    }
    cpu->state.r[0] = frame[0];
    cpu->state.r[1] = frame[1];
    cpu->state.r[2] = frame[2];
    cpu->state.r[3] = frame[3];
    cpu->state.r[12] = frame[4];
    cpu->state.r[14] = frame[5];
    cpu->state.r[15] = frame[6] & ~1u;
    cpu->state.xpsr = frame[7] & ARMV7M_XPSR_LIVE_MASK;
    cpu->itstate = (uint8_t)(((frame[7] >> 25) & 3u) |
                             ((frame[7] >> 8) & 0xfcu));
    cpu->fpca = extended ? 1u : 0u;
    if (to_process) {
        cpu->state.psp = restored_sp;
        cpu->state.r[13] = cpu->state.psp;
        cpu->state.control = (cpu->state.control & ~2u) | 2u;
    } else {
        cpu->state.msp = restored_sp;
        cpu->state.r[13] = cpu->state.msp;
        cpu->state.control &= ~2u;
    }
    armv7m_exception_returned(cpu, current_exception);
    armv7m_sleep_on_exception_return(cpu);
    return SEMU_OK;
}

semu_status armv7m_branch_exchange(semu_cpu *cpu, uint32_t target,
                                   semu_error *error)
{
    if ((cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK) != 0u &&
        (target & 0xffffffe0u) == 0xffffffe0u)
        return exception_return(cpu, target, error);
    cpu->state.r[15] = target & ~1u;
    cpu->state.xpsr = (cpu->state.xpsr & ~ARMV7M_XPSR_T) |
                      (target & 1u ? ARMV7M_XPSR_T : 0u);
    return SEMU_OK;
}

static semu_status stack_fault(semu_cpu *cpu, uint32_t address,
                               unsigned status_bits, semu_error *error)
{
    semu_status status;

    if (cpu->stack_fault_active != 0u) {
        cpu->state.halted = 1;
        cpu->stop_reason = SEMU_STOP_UNMAPPED_ACCESS;
        cpu->fault_address = address;
        cpu->has_fault_address = 1u;
        cpu->cfsr |= status_bits;
        return SEMU_ERR_RANGE;
    }
    cpu->stack_fault_active = 1u;
    status = armv7m_request_fault(cpu, 5u, status_bits, address, 1, error);
    cpu->stack_fault_active = 0u;
    return status == SEMU_OK ? SEMU_ERR_STATE : status;
}

static semu_status take_exception_internal(semu_cpu *cpu, unsigned exception,
                                            semu_error *error)
{
    uint32_t values[8];
    uint32_t handler;
    uint32_t vector_address;
    uint32_t frame_sp;
    uint32_t aligned_xpsr;
    uint32_t current_sp;
    uint32_t address;
    unsigned frame_size;
    unsigned index;
    int from_handler;
    int used_psp;
    int extended;
    int force_align;
    semu_status status;

    if (exception >= 16u + ARMV7M_IRQ_COUNT)
        return armv7m_unsupported(cpu, exception, error);
    from_handler = (cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK) != 0u;
    used_psp = !from_handler && (cpu->state.control & 2u) != 0u;
    extended = cpu->fpca != 0u;
    frame_size = extended ? 0x68u : 0x20u;
    force_align = extended || cpu->stack_align != 0u;
    current_sp = used_psp ? cpu->state.psp : cpu->state.msp;
    if (current_sp < frame_size)
        return armv7m_address_fault(cpu, current_sp, error);
    frame_sp = current_sp - frame_size;
    aligned_xpsr = cpu->state.xpsr & ARMV7M_XPSR_LIVE_MASK;
    if (force_align && (current_sp & 7u) != 0u) {
        if (frame_sp < 4u)
            return armv7m_address_fault(cpu, frame_sp, error);
        frame_sp &= ~7u;
        aligned_xpsr |= ARMV7M_XPSR_STACK_ALIGN;
    }
    values[0] = cpu->state.r[0];
    values[1] = cpu->state.r[1];
    values[2] = cpu->state.r[2];
    values[3] = cpu->state.r[3];
    values[4] = cpu->state.r[12];
    values[5] = cpu->state.r[14];
    values[6] = cpu->state.r[15];
    values[7] = aligned_xpsr;
    if (armv7m_add_address(cpu, cpu->vector_table, exception * 4u,
                           &vector_address, error) != SEMU_OK ||
        armv7m_read(cpu, vector_address, 4u, &handler, error) != SEMU_OK)
        return error != NULL ? error->code : SEMU_ERR_RANGE;
    for (index = 0u; index < frame_size / 4u; ++index) {
        status = armv7m_add_address(cpu, frame_sp, index * 4u, &address,
                                    error);
        if (status != SEMU_OK) return status;
        status = semu_bus_validate_write(cpu->bus, address, 4u, error);
        if (status != SEMU_OK)
            return stack_fault(cpu, address, ARMV7M_CFSR_BFSR_STKERR, error);
    }
    for (index = 0u; index < 8u; ++index) {
        status = armv7m_add_address(cpu, frame_sp, index * 4u, &address,
                                    error);
        if (status != SEMU_OK) return status;
        status = semu_bus_write(cpu->bus, address, 4u, values[index], error);
        if (status != SEMU_OK)
            return stack_fault(cpu, address, ARMV7M_CFSR_BFSR_STKERR, error);
    }
    status = armv7m_fpu_context_stack(cpu, frame_sp, extended, error);
    if (status != SEMU_OK) return status;
    armv7m_clear_exclusive(cpu);
    if (used_psp) cpu->state.psp = frame_sp;
    else cpu->state.msp = frame_sp;
    cpu->state.r[13] = cpu->state.msp;
    if (extended) {
        cpu->state.r[14] = from_handler ? 0xffffffe1u :
                           (used_psp ? 0xffffffedu : 0xffffffe9u);
    } else {
        cpu->state.r[14] = from_handler ? 0xfffffff1u :
                           (used_psp ? 0xfffffffdu : 0xfffffff9u);
    }
    cpu->state.r[15] = handler & ~1u;
    cpu->state.xpsr = (cpu->state.xpsr & ARMV7M_XPSR_APSR_MASK) |
                      (exception & ARMV7M_XPSR_IPSR_MASK) |
                      ((handler & 1u) != 0u ? ARMV7M_XPSR_T : 0u);
    cpu->state.control &= ~2u;
    cpu->fpca = 0u;
    cpu->itstate = 0u;
    armv7m_exception_entered(cpu, exception);
    return SEMU_OK;
}

semu_status armv7m_take_exception(semu_cpu *cpu, unsigned exception,
                                  semu_error *error)
{
    return take_exception_internal(cpu, exception, error);
}
