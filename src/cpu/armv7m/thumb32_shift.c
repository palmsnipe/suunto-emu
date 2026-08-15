#include "armv7m_internal.h"

#include <stdint.h>

static int data_register(unsigned reg)
{
    return reg < 13u || reg == 14u;
}

static semu_status refuse(semu_cpu *cpu, uint16_t first, uint16_t second,
                          uint32_t pc, semu_error *error)
{
    cpu->state.r[15] = pc;
    return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second, error);
}

static uint32_t ror32(uint32_t value, unsigned amount)
{
    amount &= 31u;
    if (amount == 0u) return value;
    return (value >> amount) | (value << (32u - amount));
}

static uint32_t asr32(uint32_t value, unsigned amount)
{
    uint32_t result;
    if (amount == 0u) return value;
    result = value >> amount;
    if ((value & 0x80000000u) != 0u)
        result |= 0xffffffffu << (32u - amount);
    return result;
}

semu_status armv7m_exec32_shift(semu_cpu *cpu, uint16_t first,
                                uint16_t second, uint32_t pc,
                                semu_error *error)
{
    unsigned type = (first >> 5u) & 3u;
    unsigned rn = first & 15u;
    unsigned rd = (second >> 8u) & 15u;
    unsigned rm = second & 15u;
    unsigned s = (first >> 10u) & 1u;
    unsigned carry = (cpu->state.xpsr & ARMV7M_XPSR_C) != 0u;
    uint32_t value = cpu->state.r[rn];
    uint32_t result = value;
    unsigned amount = cpu->state.r[rm] & 0xffu;

    if (!data_register(rn) || !data_register(rm) || !data_register(rd))
        return refuse(cpu, first, second, pc, error);
    if (amount != 0u) {
        if (type == 0u) {
            if (amount < 32u) {
                carry = (value >> (32u - amount)) & 1u;
                result = value << amount;
            } else if (amount == 32u) {
                carry = value & 1u;
                result = 0u;
            } else {
                carry = 0u;
                result = 0u;
            }
        } else if (type == 1u) {
            if (amount < 32u) {
                carry = (value >> (amount - 1u)) & 1u;
                result = value >> amount;
            } else if (amount == 32u) {
                carry = (value >> 31u) & 1u;
                result = 0u;
            } else {
                carry = 0u;
                result = 0u;
            }
        } else if (type == 2u) {
            if (amount < 32u) {
                carry = (value >> (amount - 1u)) & 1u;
                result = asr32(value, amount);
            } else {
                carry = (value >> 31u) & 1u;
                result = (value & 0x80000000u) ? 0xffffffffu : 0u;
            }
        } else {
            {
                unsigned rot = amount & 0x1fu;
                if (rot == 0u) {
                    carry = (value >> 31u) & 1u;
                    result = value;
                } else {
                    result = ror32(value, rot);
                    carry = (result >> 31u) & 1u;
                }
            }
        }
    }
    cpu->state.r[rd] = result;
    if (s != 0u) {
        armv7m_set_nz(cpu, result);
        cpu->state.xpsr = (cpu->state.xpsr & ~ARMV7M_XPSR_C) |
                          (carry != 0u ? ARMV7M_XPSR_C : 0u);
    }
    return SEMU_OK;
}
