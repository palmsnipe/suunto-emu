#include "armv7m_internal.h"

static semu_status transfer(semu_cpu *cpu, uint16_t instruction, uint32_t pc,
                            semu_error *error)
{
    unsigned op = (instruction >> 9) & 7u;
    unsigned rm = (instruction >> 6) & 7u;
    unsigned rn = (instruction >> 3) & 7u;
    unsigned rt = instruction & 7u;
    uint32_t address = cpu->state.r[rn] + cpu->state.r[rm];
    uint32_t value;
    semu_status status;

    (void)pc;
    switch (op) {
    case 0u: return armv7m_write(cpu, address, 4u, cpu->state.r[rt], error);
    case 1u: return armv7m_write(cpu, address, 2u, cpu->state.r[rt], error);
    case 2u: return armv7m_write(cpu, address, 1u, cpu->state.r[rt], error);
    case 3u:
        status = armv7m_read(cpu, address, 1u, &value, error);
        if (status == SEMU_OK) {
            cpu->state.r[rt] = (uint32_t)armv7m_sign_extend(value, 8u);
        }
        return status;
    case 4u:
        status = armv7m_read(cpu, address, 4u, &value, error);
        break;
    case 5u:
        status = armv7m_read(cpu, address, 2u, &value, error);
        break;
    case 6u:
        status = armv7m_read(cpu, address, 1u, &value, error);
        break;
    default:
        status = armv7m_read(cpu, address, 2u, &value, error);
        if (status == SEMU_OK) {
            value = (uint32_t)armv7m_sign_extend(value, 16u);
        }
        break;
    }
    if (status == SEMU_OK) {
        cpu->state.r[rt] = value;
    }
    return status;
}

static semu_status immediate_transfer(semu_cpu *cpu, uint16_t instruction,
                                      semu_error *error)
{
    unsigned rt = instruction & 7u;
    unsigned rn = (instruction >> 3) & 7u;
    unsigned imm5 = (instruction >> 6) & 0x1fu;
    unsigned width;
    int load;
    uint32_t address;
    uint32_t value;
    semu_status status;

    if ((instruction & 0xe000u) == 0x6000u) {
        width = (instruction & 0x1000u) != 0u ? 1u : 4u;
        load = (instruction & 0x0800u) != 0u;
    } else {
        width = 2u;
        load = (instruction & 0x0800u) != 0u;
    }
    address = cpu->state.r[rn] + imm5 * width;
    if (!load) {
        return armv7m_write(cpu, address, width, cpu->state.r[rt], error);
    }
    status = armv7m_read(cpu, address, width, &value, error);
    if (status == SEMU_OK) {
        cpu->state.r[rt] = value;
    }
    return status;
}

static semu_status sp_or_literal(semu_cpu *cpu, uint16_t instruction,
                                 uint32_t pc, semu_error *error)
{
    unsigned rt = (instruction >> 8) & 7u;
    uint32_t address;
    uint32_t value;
    semu_status status;

    if ((instruction & 0xf800u) == 0x4800u) {
        address = ((pc + 4u) & ~3u) + (instruction & 0xffu) * 4u;
        status = armv7m_read(cpu, address, 4u, &value, error);
    } else {
        address = cpu->state.r[13] + (instruction & 0xffu) * 4u;
        if ((instruction & 0x0800u) == 0u) {
            return armv7m_write(cpu, address, 4u, cpu->state.r[rt], error);
        }
        status = armv7m_read(cpu, address, 4u, &value, error);
    }
    if (status == SEMU_OK) {
        cpu->state.r[rt] = value;
    }
    return status;
}

static unsigned bit_count(uint32_t value)
{
    unsigned count = 0u;
    while (value != 0u) {
        count += value & 1u;
        value >>= 1;
    }
    return count;
}

static semu_status push(semu_cpu *cpu, uint16_t instruction,
                        semu_error *error)
{
    uint32_t list = instruction & 0xffu;
    uint32_t address;
    unsigned reg;

    if ((instruction & 0x0100u) != 0u) {
        list |= 1u << 14;
    }
    if (list == 0u) {
        return armv7m_unsupported(cpu, instruction, error);
    }
    address = cpu->state.r[13] - bit_count(list) * 4u;
    for (reg = 0u; reg < 15u; ++reg) {
        if ((list & (1u << reg)) != 0u) {
            if (armv7m_write(cpu, address, 4u, cpu->state.r[reg], error) !=
                SEMU_OK) {
                return error != NULL ? error->code : SEMU_ERR_RANGE;
            }
            address += 4u;
        }
    }
    armv7m_set_sp(cpu, cpu->state.r[13] - bit_count(list) * 4u);
    return SEMU_OK;
}

static semu_status pop(semu_cpu *cpu, uint16_t instruction,
                       semu_error *error)
{
    uint32_t list = instruction & 0xffu;
    uint32_t values[9];
    uint32_t address = cpu->state.r[13];
    unsigned count = 0u;
    unsigned reg;

    if ((instruction & 0x0100u) != 0u) {
        list |= 1u << 15;
    }
    for (reg = 0u; reg < 16u; ++reg) {
        if ((list & (1u << reg)) != 0u) {
            if (armv7m_read(cpu, address, 4u, &values[count], error) !=
                SEMU_OK) {
                return error != NULL ? error->code : SEMU_ERR_RANGE;
            }
            address += 4u;
            count++;
        }
    }
    count = 0u;
    for (reg = 0u; reg < 15u; ++reg) {
        if ((list & (1u << reg)) != 0u) {
            cpu->state.r[reg] = values[count++];
        }
    }
    armv7m_set_sp(cpu, address);
    if ((list & (1u << 15)) != 0u) {
        return armv7m_branch_exchange(cpu, values[count], error);
    }
    return SEMU_OK;
}

static semu_status multiple(semu_cpu *cpu, uint16_t instruction,
                            semu_error *error)
{
    uint32_t list = instruction & 0xffu;
    unsigned rn = (instruction >> 8) & 7u;
    uint32_t address = cpu->state.r[rn];
    uint32_t value;
    unsigned reg;
    int load = (instruction & 0x0800u) != 0u;

    if (list == 0u) {
        return armv7m_unsupported(cpu, instruction, error);
    }
    for (reg = 0u; reg < 8u; ++reg) {
        if ((list & (1u << reg)) != 0u) {
            if (load) {
                if (armv7m_read(cpu, address, 4u, &value, error) != SEMU_OK) {
                    return error != NULL ? error->code : SEMU_ERR_RANGE;
                }
                cpu->state.r[reg] = value;
            } else if (armv7m_write(cpu, address, 4u, cpu->state.r[reg],
                                     error) != SEMU_OK) {
                return error != NULL ? error->code : SEMU_ERR_RANGE;
            }
            address += 4u;
        }
    }
    if (!load || (list & (1u << rn)) == 0u) {
        cpu->state.r[rn] += bit_count(list) * 4u;
    }
    return SEMU_OK;
}

semu_status armv7m_exec16_memory(semu_cpu *cpu, uint16_t instruction,
                                 uint32_t pc, semu_error *error)
{
    if ((instruction & 0xf000u) == 0x5000u) {
        return transfer(cpu, instruction, pc, error);
    }
    if ((instruction & 0xe000u) == 0x6000u ||
        (instruction & 0xf000u) == 0x8000u) {
        return immediate_transfer(cpu, instruction, error);
    }
    if ((instruction & 0xf800u) == 0x4800u ||
        (instruction & 0xf000u) == 0x9000u) {
        return sp_or_literal(cpu, instruction, pc, error);
    }
    if ((instruction & 0xfe00u) == 0xb400u) {
        return push(cpu, instruction, error);
    }
    if ((instruction & 0xfe00u) == 0xbc00u) {
        return pop(cpu, instruction, error);
    }
    if ((instruction & 0xf000u) == 0xc000u) {
        return multiple(cpu, instruction, error);
    }
    return armv7m_unsupported(cpu, instruction, error);
}
