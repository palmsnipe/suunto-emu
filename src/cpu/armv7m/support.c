#include "armv7m_internal.h"

semu_status armv7m_read(semu_cpu *cpu, uint32_t address, unsigned width,
                        uint32_t *value, semu_error *error)
{
    semu_status status = semu_bus_read(cpu->bus, address, width, value, error);
    if (status != SEMU_OK) {
        cpu->state.halted = 1;
        cpu->stop_reason = SEMU_STOP_UNMAPPED_ACCESS;
        cpu->fault_address = address;
        cpu->has_fault_address = 1u;
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
        cpu->fault_address = address;
        cpu->has_fault_address = 1u;
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
