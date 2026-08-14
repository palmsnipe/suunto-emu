#include "armv7m_internal.h"

semu_status armv7m_read(semu_cpu *cpu, uint32_t address, unsigned width,
                        uint32_t *value, semu_error *error)
{
    semu_status status = semu_bus_read(cpu->bus, address, width, value, error);
    if (status != SEMU_OK) {
        cpu->state.halted = 1;
        cpu->stop_reason = SEMU_STOP_UNMAPPED_ACCESS;
    }
    return status;
}

semu_status armv7m_write(semu_cpu *cpu, uint32_t address, unsigned width,
                         uint32_t value, semu_error *error)
{
    semu_status status = semu_bus_write(cpu->bus, address, width, value, error);
    if (status != SEMU_OK) {
        cpu->state.halted = 1;
        cpu->stop_reason = SEMU_STOP_UNMAPPED_ACCESS;
    }
    return status;
}

semu_status armv7m_unsupported(semu_cpu *cpu, uint32_t instruction,
                              semu_error *error)
{
    cpu->fault_instruction = instruction;
    cpu->state.halted = 1;
    cpu->stop_reason = SEMU_STOP_UNSUPPORTED_INSTRUCTION;
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "unsupported Thumb instruction 0x%08lx at 0x%08lx",
                   (unsigned long)instruction,
                   (unsigned long)cpu->state.r[15]);
    return SEMU_ERR_UNSUPPORTED;
}

uint32_t armv7m_reg(const semu_cpu *cpu, unsigned reg, uint32_t pc)
{
    if (reg == 15u) {
        return pc + 4u;
    }
    return cpu->state.r[reg];
}

void armv7m_set_sp(semu_cpu *cpu, uint32_t value)
{
    cpu->state.r[13] = value;
    if ((cpu->state.control & 2u) != 0u &&
        (cpu->state.xpsr & 0x1ffu) == 0u) {
        cpu->state.psp = value;
    } else {
        cpu->state.msp = value;
    }
}

void armv7m_set_nz(semu_cpu *cpu, uint32_t value)
{
    cpu->state.xpsr &= ~(ARMV7M_XPSR_N | ARMV7M_XPSR_Z);
    if ((value & 0x80000000u) != 0u) {
        cpu->state.xpsr |= ARMV7M_XPSR_N;
    }
    if (value == 0u) {
        cpu->state.xpsr |= ARMV7M_XPSR_Z;
    }
}

uint32_t armv7m_add(semu_cpu *cpu, uint32_t left, uint32_t right,
                    unsigned carry, int update_flags)
{
    uint64_t wide = (uint64_t)left + (uint64_t)right + (uint64_t)carry;
    uint32_t result = (uint32_t)wide;
    uint32_t overflow = (~(left ^ right) & (left ^ result)) >> 31;

    if (update_flags) {
        armv7m_set_nz(cpu, result);
        cpu->state.xpsr &= ~(ARMV7M_XPSR_C | ARMV7M_XPSR_V);
        if ((wide >> 32) != 0u) {
            cpu->state.xpsr |= ARMV7M_XPSR_C;
        }
        if (overflow != 0u) {
            cpu->state.xpsr |= ARMV7M_XPSR_V;
        }
    }
    return result;
}

int32_t armv7m_sign_extend(uint32_t value, unsigned bits)
{
    uint32_t sign = 1u << (bits - 1u);
    uint32_t mask = (1u << bits) - 1u;

    value &= mask;
    if ((value & sign) != 0u) {
        uint32_t magnitude = ((~value) & mask) + 1u;
        return -(int32_t)magnitude;
    }
    return (int32_t)value;
}

int armv7m_condition_passed(const semu_cpu *cpu, unsigned condition)
{
    int n = (cpu->state.xpsr & ARMV7M_XPSR_N) != 0u;
    int z = (cpu->state.xpsr & ARMV7M_XPSR_Z) != 0u;
    int c = (cpu->state.xpsr & ARMV7M_XPSR_C) != 0u;
    int v = (cpu->state.xpsr & ARMV7M_XPSR_V) != 0u;

    switch (condition & 15u) {
    case 0u: return z;
    case 1u: return !z;
    case 2u: return c;
    case 3u: return !c;
    case 4u: return n;
    case 5u: return !n;
    case 6u: return v;
    case 7u: return !v;
    case 8u: return c && !z;
    case 9u: return !c || z;
    case 10u: return n == v;
    case 11u: return n != v;
    case 12u: return !z && (n == v);
    case 13u: return z || (n != v);
    case 14u: return 1;
    default: return 0;
    }
}

static semu_status exception_return(semu_cpu *cpu, uint32_t token,
                                    semu_error *error)
{
    uint32_t frame[8];
    uint32_t sp;
    unsigned index;

    if (token != 0xfffffff9u && token != 0xfffffffdu) {
        return armv7m_unsupported(cpu, token, error);
    }
    sp = token == 0xfffffffdu ? cpu->state.psp : cpu->state.msp;
    for (index = 0u; index < 8u; ++index) {
        if (armv7m_read(cpu, sp + index * 4u, 4u, &frame[index], error) !=
            SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        }
    }
    if ((frame[7] & ARMV7M_XPSR_T) == 0u) {
        return armv7m_unsupported(cpu, token, error);
    }
    cpu->state.r[0] = frame[0];
    cpu->state.r[1] = frame[1];
    cpu->state.r[2] = frame[2];
    cpu->state.r[3] = frame[3];
    cpu->state.r[12] = frame[4];
    cpu->state.r[14] = frame[5];
    cpu->state.r[15] = frame[6] & ~1u;
    cpu->state.xpsr = frame[7];
    cpu->itstate = (uint8_t)(((frame[7] >> 25) & 3u) |
                             ((frame[7] >> 8) & 0xfcu));
    if (token == 0xfffffffdu) {
        cpu->state.psp = sp + 32u;
        cpu->state.r[13] = cpu->state.psp;
    } else {
        cpu->state.msp = sp + 32u;
        cpu->state.r[13] = cpu->state.msp;
    }
    return SEMU_OK;
}

semu_status armv7m_branch_exchange(semu_cpu *cpu, uint32_t target,
                                   semu_error *error)
{
    if ((target & 0xfffffff0u) == 0xfffffff0u) {
        return exception_return(cpu, target, error);
    }
    if ((target & 1u) == 0u) {
        return armv7m_unsupported(cpu, target, error);
    }
    cpu->state.r[15] = target & ~1u;
    return SEMU_OK;
}

semu_status armv7m_take_exception(semu_cpu *cpu, unsigned exception,
                                  semu_error *error)
{
    uint32_t values[8];
    uint32_t handler;
    uint32_t sp;
    unsigned index;
    int used_psp;

    if (exception >= 16u + ARMV7M_IRQ_COUNT) {
        return armv7m_unsupported(cpu, exception, error);
    }
    used_psp = (cpu->state.xpsr & 0x1ffu) == 0u &&
               (cpu->state.control & 2u) != 0u;
    sp = (used_psp ? cpu->state.psp : cpu->state.msp) - 32u;
    values[0] = cpu->state.r[0];
    values[1] = cpu->state.r[1];
    values[2] = cpu->state.r[2];
    values[3] = cpu->state.r[3];
    values[4] = cpu->state.r[12];
    values[5] = cpu->state.r[14];
    values[6] = cpu->state.r[15];
    values[7] = cpu->state.xpsr;
    for (index = 0u; index < 8u; ++index) {
        if (armv7m_write(cpu, sp + index * 4u, 4u, values[index], error) !=
            SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        }
    }
    if (armv7m_read(cpu, cpu->vector_table + exception * 4u, 4u, &handler,
                    error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_RANGE;
    }
    if ((handler & 1u) == 0u) {
        cpu->state.halted = 1;
        cpu->stop_reason = SEMU_STOP_FIRMWARE_ASSERT;
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "exception %u has invalid vector 0x%08lx", exception,
                       (unsigned long)handler);
        return SEMU_ERR_FORMAT;
    }
    if (used_psp) {
        cpu->state.psp = sp;
    } else {
        cpu->state.msp = sp;
    }
    cpu->state.r[13] = cpu->state.msp;
    cpu->state.r[14] = used_psp ? 0xfffffffdu : 0xfffffff9u;
    cpu->state.r[15] = handler & ~1u;
    cpu->state.xpsr = (cpu->state.xpsr & ~0x1ffu) | exception;
    cpu->state.xpsr |= ARMV7M_XPSR_T;
    cpu->itstate = 0u;
    return SEMU_OK;
}
