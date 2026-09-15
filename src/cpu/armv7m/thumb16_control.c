#include "armv7m_internal.h"
static int valid_it_mask(unsigned mask)
{
    return mask != 0u && mask <= 0x0fu;
}

static uint32_t reverse_word(uint32_t value)
{
    return (value >> 24u) | ((value >> 8u) & 0x0000ff00u) |
           ((value << 8u) & 0x00ff0000u) | (value << 24u);
}

static uint32_t reverse_halfwords(uint32_t value)
{
    return ((value & 0x00ff00ffu) << 8u) |
           ((value & 0xff00ff00u) >> 8u);
}

static semu_status special_data(semu_cpu *cpu, uint16_t instruction,
                                uint32_t pc, semu_error *error)
{
    unsigned op = (instruction >> 8) & 3u;
    unsigned rd = (instruction & 7u) | ((instruction >> 4) & 8u);
    unsigned rm = (instruction >> 3) & 15u;
    uint32_t left;
    uint32_t right;
    uint32_t result;

    if (op == 0u) {
        left = armv7m_reg(cpu, rd, pc);
        right = armv7m_reg(cpu, rm, pc);
        result = left + right;
        if (rd == 15u) {
            cpu->state.r[15] = result & ~1u;
        } else if (rd == 13u) {
            armv7m_set_sp(cpu, result);
        } else {
            cpu->state.r[rd] = result;
        }
        return SEMU_OK;
    }
    if (op == 1u) {
        if (rd == 15u || rm == 15u) {
            return armv7m_unsupported(cpu, instruction, error);
        }
        left = armv7m_reg(cpu, rd, pc);
        right = armv7m_reg(cpu, rm, pc);
        (void)armv7m_add(cpu, left, ~right, 1u, 1);
        return SEMU_OK;
    }
    if (op == 2u) {
        right = armv7m_reg(cpu, rm, pc);
        if (rd == 15u) {
            cpu->state.r[15] = right & ~1u;
        } else if (rd == 13u) {
            armv7m_set_sp(cpu, right);
        } else {
            cpu->state.r[rd] = right;
        }
        return SEMU_OK;
    }

    if ((instruction & 0x0080u) != 0u && rm == 15u) {
        return armv7m_unsupported(cpu, instruction, error);
    }
    right = armv7m_reg(cpu, rm, pc);
    if ((instruction & 0x0080u) != 0u) {
        cpu->state.r[14] = (pc + 2u) | 1u;
    }
    return armv7m_branch_exchange(cpu, right, error);
}

static semu_status miscellaneous(semu_cpu *cpu, uint16_t instruction,
                                 semu_error *error)
{
    unsigned rd;
    unsigned rm;
    unsigned operation;
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
        case 2u:
            value = cpu->state.r[rm] & 0xffffu;
            break;
        default:
            value = cpu->state.r[rm] & 0xffu;
            break;
        }
        cpu->state.r[rd] = value;
        return SEMU_OK;
    }
    if ((instruction & 0xff00u) == 0xba00u) {
        rd = instruction & 7u;
        rm = (instruction >> 3) & 7u;
        operation = (instruction >> 6) & 3u;
        value = cpu->state.r[rm];
        if (operation == 0u) {
            cpu->state.r[rd] = reverse_word(value);
        } else if (operation == 1u) {
            cpu->state.r[rd] = reverse_halfwords(value);
        } else if (operation == 3u) {
            value = ((value & 0xffu) << 8u) | ((value >> 8u) & 0xffu);
            cpu->state.r[rd] = (uint32_t)armv7m_sign_extend(value, 16u);
        } else {
            return armv7m_unsupported(cpu, instruction, error);
        }
        return SEMU_OK;
    }
    if ((instruction & 0xff00u) == 0xbe00u) {
        /*
         * BKPT #imm retires as a no-op while no debug session asserts
         * DBGEN (ARMv7-M: BKPT is a NOP in that state), and this
         * emulator never presents a debugger. The lane agrees on
         * machine evidence: a direct Renode 1.16.1 probe (BKPT #0 stub
         * with a NOP sled, RunFor) logs nothing at NOISY level and the
         * CPU advances through the sled, and the Ulsan 2.35.36 lane
         * log keeps servicing 194 interrupt acknowledgements plus
         * deep-sleep/wake cycles after the guest assert that ends in
         * BKPT #0 at 0x0006bda8 (E-ULS-0040). Halting here previously
         * stopped both Ulsan epochs before the lane-observed steady
         * era and every Sapporo production run at its BKPT sentinel.
         */
        return SEMU_OK;
    }
    if ((instruction & 0xff00u) == 0xbf00u) {
        unsigned low = instruction & 0xffu;
        unsigned condition;
        unsigned mask;

        if (low == 0x00u || low == 0x10u) {
            return SEMU_OK;
        }
        if (low == 0x20u) {
            armv7m_sleep_wfe(cpu);
            return SEMU_OK;
        }
        if (low == 0x30u) {
            armv7m_sleep_wfi(cpu);
            return SEMU_OK;
        }
        if (low == 0x40u) {
            armv7m_sleep_event(cpu);
            return SEMU_OK;
        }
        if ((low & 0x0fu) == 0u) {
            return armv7m_unsupported(cpu, instruction, error);
        }
        condition = low >> 4;
        mask = low & 0x0fu;
        if (condition >= 15u || !valid_it_mask(mask) ||
            cpu->itstate != 0u) {
            return armv7m_unsupported(cpu, instruction, error);
        }
        armv7m_set_itstate(cpu, (uint8_t)low);
        return SEMU_OK;
    }
    if ((instruction & 0xff00u) == 0xb600u &&
        ((instruction & 0x00f0u) == 0x0060u ||
         (instruction & 0x00f0u) == 0x0070u) &&
        (instruction & 0x000cu) == 0u) {
        unsigned mask = instruction & 3u;
        int disable = (instruction & 0x0010u) != 0u;

        if (mask == 0u ||
            ((cpu->state.control & 1u) != 0u &&
             (cpu->state.xpsr & 0x1ffu) == 0u)) {
            return armv7m_unsupported(cpu, instruction, error);
        }
        if ((mask & 2u) != 0u) {
            cpu->state.primask = disable ? 1u : 0u;
        }
        if ((mask & 1u) != 0u &&
            (cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK) != 2u &&
            (cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK) != 3u) {
            cpu->state.faultmask = disable ? 1u : 0u;
        }
        return SEMU_OK;
    }
    return armv7m_unsupported(cpu, instruction, error);
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
    if ((instruction & 0xf800u) == 0xe000u) {
        cpu->state.r[15] = pc + 4u +
            (uint32_t)armv7m_sign_extend((instruction & 0x7ffu) << 1, 12u);
        return SEMU_OK;
    }
    return armv7m_unsupported(cpu, instruction, error);
}

semu_status armv7m_exec16_control(semu_cpu *cpu, uint16_t instruction,
                                  uint32_t pc, semu_error *error)
{
    if ((instruction & 0xfc00u) == 0x4400u) {
        return special_data(cpu, instruction, pc, error);
    }
    if ((instruction & 0xf000u) == 0xa000u) {
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
    return armv7m_unsupported(cpu, instruction, error);
}
