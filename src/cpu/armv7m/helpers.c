#include "armv7m_internal.h"

unsigned armv7m_bit_count(uint32_t value)
{
    unsigned count = 0u;
    while (value != 0u) {
        count += value & 1u;
        value >>= 1;
    }
    return count;
}

uint32_t armv7m_shift(semu_cpu *cpu, uint32_t value, unsigned type,
                      unsigned amount, int immediate)
{
    uint32_t result = value;
    unsigned carry = (cpu->state.xpsr & ARMV7M_XPSR_C) != 0u;

    if (type == 0u) {
        if (amount != 0u) {
            carry = amount <= 32u ? (value >> (32u - amount)) & 1u : 0u;
            result = amount < 32u ? value << amount : 0u;
        }
    } else if (type == 1u) {
        if (immediate && amount == 0u) {
            amount = 32u;
        }
        if (amount != 0u) {
            carry = amount <= 32u ? (value >> (amount - 1u)) & 1u : 0u;
            result = amount < 32u ? value >> amount : 0u;
        }
    } else if (type == 2u) {
        if (immediate && amount == 0u) {
            amount = 32u;
        }
        if (amount != 0u) {
            if (amount >= 32u) {
                carry = value >> 31;
                result = carry != 0u ? 0xffffffffu : 0u;
            } else {
                carry = (value >> (amount - 1u)) & 1u;
                result = value >> amount;
                if ((value & 0x80000000u) != 0u) {
                    result |= 0xffffffffu << (32u - amount);
                }
            }
        }
    } else if (amount != 0u) {
        amount &= 31u;
        if (amount == 0u) {
            carry = value >> 31;
        } else {
            result = (value >> amount) | (value << (32u - amount));
            carry = result >> 31;
        }
    }
    armv7m_set_nz(cpu, result);
    cpu->state.xpsr &= ~ARMV7M_XPSR_C;
    if (carry != 0u) {
        cpu->state.xpsr |= ARMV7M_XPSR_C;
    }
    return result;
}

static uint32_t rotate_right(uint32_t value, unsigned amount)
{
    amount &= 31u;
    return amount == 0u ? value : (value >> amount) | (value << (32u - amount));
}

uint32_t armv7m_expand_modified_immediate(uint16_t first, uint16_t second)
{
    uint32_t imm12 = ((uint32_t)(first & 0x0400u) << 1u) |
                     ((uint32_t)(second & 0x7000u) >> 4u) |
                     (second & 0x00ffu);
    uint32_t imm8 = imm12 & 0xffu;

    if ((imm12 & 0xc00u) == 0u) {
        switch ((imm12 >> 8u) & 3u) {
        case 0u: return imm8;
        case 1u: return (imm8 << 16u) | imm8;
        case 2u: return (imm8 << 24u) | (imm8 << 8u);
        default: return imm8 * 0x01010101u;
        }
    }
    return rotate_right(0x80u | (imm12 & 0x7fu), (imm12 >> 7u) & 31u);
}

uint32_t armv7m_shifted_register(uint32_t value, uint16_t second)
{
    unsigned amount = ((second >> 10u) & 0x1cu) | ((second >> 6u) & 3u);
    unsigned type = (second >> 4u) & 3u;

    if (amount == 0u) {
        return value;
    }
    if (type == 0u) {
        return amount < 32u ? value << amount : 0u;
    }
    if (type == 1u) {
        return amount < 32u ? value >> amount : 0u;
    }
    if (type == 2u) {
        if (amount >= 32u) {
            return (value & 0x80000000u) != 0u ? 0xffffffffu : 0u;
        }
        return (value >> amount) |
               ((value & 0x80000000u) != 0u
                    ? 0xffffffffu << (32u - amount) : 0u);
    }
    amount &= 31u;
    return amount == 0u ? value : rotate_right(value, amount);
}
