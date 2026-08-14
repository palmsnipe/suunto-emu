#include "armv7m_internal.h"

static semu_status shifts_and_adds(semu_cpu *cpu, uint16_t instruction)
{
    unsigned rd = instruction & 7u;
    unsigned rn = (instruction >> 3) & 7u;
    unsigned op;
    uint32_t operand;

    if ((instruction & 0xf800u) != 0x1800u) {
        op = (instruction >> 11) & 3u;
        cpu->state.r[rd] = armv7m_shift(cpu, cpu->state.r[rn], op,
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

static semu_status immediate(semu_cpu *cpu, uint16_t instruction)
{
    unsigned op = (instruction >> 11) & 3u;
    unsigned rd = (instruction >> 8) & 7u;
    uint32_t value = instruction & 0xffu;

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

static semu_status data_processing(semu_cpu *cpu, uint16_t instruction)
{
    unsigned op = (instruction >> 6) & 15u;
    unsigned rm = (instruction >> 3) & 7u;
    unsigned rdn = instruction & 7u;
    uint32_t left = cpu->state.r[rdn];
    uint32_t right = cpu->state.r[rm];
    uint32_t result;

    switch (op) {
    case 0u: result = left & right; break;
    case 1u: result = left ^ right; break;
    case 2u: result = armv7m_shift(cpu, left, 0u, right & 0xffu, 0); goto stored;
    case 3u: result = armv7m_shift(cpu, left, 1u, right & 0xffu, 0); goto stored;
    case 4u: result = armv7m_shift(cpu, left, 2u, right & 0xffu, 0); goto stored;
    case 5u:
        result = armv7m_add(cpu, left, right,
                            (cpu->state.xpsr & ARMV7M_XPSR_C) != 0u, 1);
        goto stored;
    case 6u:
        result = armv7m_add(cpu, left, ~right,
                            (cpu->state.xpsr & ARMV7M_XPSR_C) != 0u, 1);
        goto stored;
    case 7u: result = armv7m_shift(cpu, left, 3u, right & 0xffu, 0); goto stored;
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

semu_status armv7m_exec16_arith(semu_cpu *cpu, uint16_t instruction,
                                uint32_t pc, semu_error *error)
{
    if ((instruction & 0xe000u) == 0u ||
        (instruction & 0xf800u) == 0x1800u) {
        return shifts_and_adds(cpu, instruction);
    }
    if ((instruction & 0xe000u) == 0x2000u) {
        return immediate(cpu, instruction);
    }
    if ((instruction & 0xfc00u) == 0x4000u) {
        return data_processing(cpu, instruction);
    }
    /* The dispatcher normally prevents this path; keep the family entry
       point fail-closed for direct callers and preserve the faulting PC. */
    cpu->state.r[15] = pc;
    return armv7m_unsupported(cpu, instruction, error);
}
