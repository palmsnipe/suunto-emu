#include "armv7m_internal.h"
static uint32_t shift(semu_cpu *cpu, uint32_t value, unsigned type,
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
static semu_status shifts_and_adds(semu_cpu *cpu, uint16_t instruction,
                                   semu_error *error)
{
    unsigned rd = instruction & 7u;
    unsigned rn = (instruction >> 3) & 7u;
    unsigned op;
    uint32_t operand;
    (void)error;
    if ((instruction & 0xf800u) != 0x1800u) {
        op = (instruction >> 11) & 3u;
        cpu->state.r[rd] = shift(cpu, cpu->state.r[rn], op,
                                 (instruction >> 6) & 0x1fu, 1);
        return SEMU_OK;
    }
    op = (instruction >> 9) & 3u;
    operand = (op & 2u) != 0u ? (instruction >> 6) & 7u
                              : cpu->state.r[(instruction >> 6) & 7u];
    if ((op & 1u) != 0u) {
        cpu->state.r[rd] = armv7m_add(cpu, cpu->state.r[rn], ~operand, 1u, 1);
    } else {
        cpu->state.r[rd] = armv7m_add(cpu, cpu->state.r[rn], operand, 0u, 1);
    }
    return SEMU_OK;
}
static semu_status immediate(semu_cpu *cpu, uint16_t instruction,
                             semu_error *error)
{
    unsigned op = (instruction >> 11) & 3u;
    unsigned rd = (instruction >> 8) & 7u;
    uint32_t value = instruction & 0xffu;
    (void)error;
    if (op == 0u) {
        cpu->state.r[rd] = value;
        armv7m_set_nz(cpu, value);
    } else if (op == 1u) {
        (void)armv7m_add(cpu, cpu->state.r[rd], ~value, 1u, 1);
    } else if (op == 2u) {
        cpu->state.r[rd] = armv7m_add(cpu, cpu->state.r[rd], value, 0u, 1);
    } else {
        cpu->state.r[rd] = armv7m_add(cpu, cpu->state.r[rd], ~value, 1u, 1);
    }
    return SEMU_OK;
}

static semu_status data_processing(semu_cpu *cpu, uint16_t instruction,
                                   semu_error *error)
{
    unsigned op = (instruction >> 6) & 15u;
    unsigned rm = (instruction >> 3) & 7u;
    unsigned rdn = instruction & 7u;
    uint32_t left = cpu->state.r[rdn];
    uint32_t right = cpu->state.r[rm];
    uint32_t result;

    (void)error;
    switch (op) {
    case 0u: result = left & right; break;
    case 1u: result = left ^ right; break;
    case 2u: result = shift(cpu, left, 0u, right & 0xffu, 0); goto stored;
    case 3u: result = shift(cpu, left, 1u, right & 0xffu, 0); goto stored;
    case 4u: result = shift(cpu, left, 2u, right & 0xffu, 0); goto stored;
    case 5u:
        result = armv7m_add(cpu, left, right,
                            (cpu->state.xpsr & ARMV7M_XPSR_C) != 0u, 1);
        goto stored;
    case 6u:
        result = armv7m_add(cpu, left, ~right,
                            (cpu->state.xpsr & ARMV7M_XPSR_C) != 0u, 1);
        goto stored;
    case 7u: result = shift(cpu, left, 3u, right & 0xffu, 0); goto stored;
    case 8u: armv7m_set_nz(cpu, left & right); return SEMU_OK;
    case 9u: result = armv7m_add(cpu, 0u, ~right, 1u, 1); goto stored;
    case 10u: (void)armv7m_add(cpu, left, ~right, 1u, 1); return SEMU_OK;
    case 11u: (void)armv7m_add(cpu, left, right, 0u, 1); return SEMU_OK;
    case 12u: result = left | right; break;
    case 13u: result = left * right; break;
    case 14u: result = left & ~right; break;
    default: result = ~right; break;
    }
    armv7m_set_nz(cpu, result);
stored:
    cpu->state.r[rdn] = result;
    return SEMU_OK;
}

static semu_status special_data(semu_cpu *cpu, uint16_t instruction,
                                uint32_t pc, semu_error *error)
{
    unsigned op = (instruction >> 8) & 3u;
    unsigned rd = (instruction & 7u) | ((instruction >> 4) & 8u);
    unsigned rm = (instruction >> 3) & 15u;
    uint32_t left = armv7m_reg(cpu, rd, pc);
    uint32_t right = armv7m_reg(cpu, rm, pc);
    uint32_t result;

    if (op == 0u) {
        result = left + right;
        if (rd == 15u) {
            return armv7m_branch_exchange(cpu, result | 1u, error);
        }
        cpu->state.r[rd] = result;
    } else if (op == 1u) {
        (void)armv7m_add(cpu, left, ~right, 1u, 1);
    } else if (op == 2u) {
        if (rd == 15u) {
            return armv7m_branch_exchange(cpu, right, error);
        }
        cpu->state.r[rd] = right;
        if (rd == 13u) {
            armv7m_set_sp(cpu, right);
        }
    } else {
        if ((instruction & 0x0080u) != 0u) {
            cpu->state.r[14] = (pc + 2u) | 1u;
        }
        return armv7m_branch_exchange(cpu, right, error);
    }
    return SEMU_OK;
}

static semu_status miscellaneous(semu_cpu *cpu, uint16_t instruction,
                                 semu_error *error)
{
    unsigned rd;
    unsigned rm;
    uint32_t value;

    if ((instruction & 0xff00u) == 0xb000u) {
        value = (instruction & 0x7fu) * 4u;
        armv7m_set_sp(cpu, (instruction & 0x80u) != 0u
                               ? cpu->state.r[13] - value
                               : cpu->state.r[13] + value);
        return SEMU_OK;
    }
    if ((instruction & 0xff00u) == 0xb200u) {
        rd = instruction & 7u;
        rm = (instruction >> 3) & 7u;
        switch ((instruction >> 6) & 3u) {
        case 0u:
            value = (uint32_t)armv7m_sign_extend(cpu->state.r[rm], 16u);
            break;
        case 1u:
            value = (uint32_t)armv7m_sign_extend(cpu->state.r[rm], 8u);
            break;
        case 2u: value = cpu->state.r[rm] & 0xffffu; break;
        default: value = cpu->state.r[rm] & 0xffu; break;
        }
        cpu->state.r[rd] = value;
        return SEMU_OK;
    }
    if ((instruction & 0xff00u) == 0xbe00u) {
        cpu->state.halted = 1;
        cpu->stop_reason = SEMU_STOP_HALT;
        return SEMU_OK;
    }
    if ((instruction & 0xff00u) == 0xbf00u) {
        unsigned low = instruction & 0xffu;
        if (low == 0x30u) {
            cpu->state.waiting_for_interrupt = 1;
        } else if ((low & 0x0fu) != 0u) {
            unsigned condition = low >> 4;
            if (condition >= 14u) {
                return armv7m_unsupported(cpu, instruction, error);
            }
            armv7m_set_itstate(cpu, (uint8_t)low);
        } else if (low != 0u && low != 0x10u && low != 0x20u &&
                   low != 0x40u) {
            return armv7m_unsupported(cpu, instruction, error);
        }
        return SEMU_OK;
    }
    if ((instruction & 0xffefu) == 0xb662u) {
        if ((instruction & 0x0010u) != 0u) {
            cpu->state.primask = 1u;
        } else {
            cpu->state.primask = 0u;
        }
        return SEMU_OK;
    }
    return armv7m_exec16_memory(cpu, instruction, cpu->state.r[15] - 2u,
                                error);
}

static semu_status branches(semu_cpu *cpu, uint16_t instruction, uint32_t pc,
                            semu_error *error)
{
    if ((instruction & 0xff00u) == 0xdf00u) {
        return armv7m_take_exception(cpu, 11u, error);
    }
    if ((instruction & 0xf000u) == 0xd000u) {
        unsigned condition = (instruction >> 8) & 15u;
        if (condition >= 14u) {
            return armv7m_unsupported(cpu, instruction, error);
        }
        if (armv7m_condition_passed(cpu, condition)) {
            cpu->state.r[15] = pc + 4u +
                (uint32_t)armv7m_sign_extend((instruction & 0xffu) << 1, 9u);
        }
        return SEMU_OK;
    }
    cpu->state.r[15] = pc + 4u +
        (uint32_t)armv7m_sign_extend((instruction & 0x7ffu) << 1, 12u);
    return SEMU_OK;
}

semu_status armv7m_exec16(semu_cpu *cpu, uint16_t instruction,
                          uint32_t pc, semu_error *error)
{
    cpu->state.r[15] = pc + 2u;
    if ((instruction & 0xe000u) == 0u ||
        (instruction & 0xf800u) == 0x1800u) {
        return shifts_and_adds(cpu, instruction, error);
    }
    if ((instruction & 0xe000u) == 0x2000u) {
        return immediate(cpu, instruction, error);
    }
    if ((instruction & 0xfc00u) == 0x4000u) {
        return data_processing(cpu, instruction, error);
    }
    if ((instruction & 0xfc00u) == 0x4400u) {
        return special_data(cpu, instruction, pc, error);
    }
    if ((instruction & 0xf800u) == 0xa000u) {
        unsigned rd = (instruction >> 8) & 7u;
        uint32_t base = (instruction & 0x0800u) != 0u
                            ? cpu->state.r[13] : ((pc + 4u) & ~3u);
        cpu->state.r[rd] = base + (instruction & 0xffu) * 4u;
        return SEMU_OK;
    }
    if ((instruction & 0xf500u) == 0xb100u) {
        unsigned rn = instruction & 7u;
        int nonzero = (instruction & 0x0800u) != 0u;
        if ((cpu->state.r[rn] != 0u) == nonzero) {
            uint32_t offset = ((instruction >> 3) & 0x1fu) << 1;
            offset |= ((instruction >> 9) & 1u) << 6;
            cpu->state.r[15] = pc + 4u + offset;
        }
        return SEMU_OK;
    }
    if ((instruction & 0xf000u) == 0xd000u ||
        (instruction & 0xf800u) == 0xe000u) {
        return branches(cpu, instruction, pc, error);
    }
    if ((instruction & 0xf000u) == 0xb000u) {
        return miscellaneous(cpu, instruction, error);
    }
    return armv7m_exec16_memory(cpu, instruction, pc, error);
}
