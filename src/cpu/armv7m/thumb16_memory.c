#include "armv7m_internal.h"

static semu_status refuse(semu_cpu *cpu, uint16_t instruction, semu_error *error)
{ return armv7m_unsupported(cpu, instruction, error); }

static int add_u32(uint32_t base, uint32_t offset, uint32_t *result)
{
    uint64_t sum = (uint64_t)base + offset;

    if (sum > UINT32_MAX) {
        return 0;
    }
    *result = (uint32_t)sum;
    return 1;
}

static int access_address(uint32_t base, uint32_t offset, unsigned width,
                          uint32_t *address)
{
    uint64_t value = (uint64_t)base + offset;

    if (value + width > UINT64_C(0x100000000)) {
        return 0;
    }
    *address = (uint32_t)value;
    return 1;
}

static int access_address_aligned(uint32_t base, uint32_t offset,
                                   unsigned width, uint32_t *address)
{
    uint64_t value = (uint64_t)base + offset;

    if ((value & (width - 1u)) != 0u ||
        value + width > UINT64_C(0x100000000)) {
        return 0;
    }
    *address = (uint32_t)value;
    return 1;
}

static int transfer_address(uint32_t base, uint32_t offset, unsigned width,
                            uint16_t instruction, semu_cpu *cpu,
                            uint32_t *address, semu_error *error)
{
    if (!access_address(base, offset, width, address)) {
        (void)refuse(cpu, instruction, error);
        return 0;
    }
    return 1;
}

static semu_status transfer(semu_cpu *cpu, uint16_t instruction,
                            semu_error *error)
{
    unsigned op = (instruction >> 9) & 7u;
    unsigned rm = (instruction >> 6) & 7u;
    unsigned rn = (instruction >> 3) & 7u;
    unsigned rt = instruction & 7u;
    uint32_t address;
    uint32_t value;
    semu_status status;

    if (!transfer_address(cpu->state.r[rn], cpu->state.r[rm],
                          op == 0u || op == 4u ? 4u :
                          op == 1u || op == 5u || op == 7u ? 2u : 1u,
                          instruction, cpu, &address, error)) {
        return SEMU_ERR_UNSUPPORTED;
    }
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
    if (!transfer_address(cpu->state.r[rn], imm5 * width, width,
                          instruction, cpu, &address, error)) {
        return SEMU_ERR_UNSUPPORTED;
    }
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
    uint32_t base;
    uint32_t address;
    uint32_t value;
    semu_status status;

    if ((instruction & 0xf800u) == 0x4800u) {
        if (!add_u32(pc, 4u, &base) ||
            !transfer_address(base & ~3u, (instruction & 0xffu) * 4u,
                              4u, instruction, cpu, &address, error)) {
            return SEMU_ERR_UNSUPPORTED;
        }
        status = armv7m_read(cpu, address, 4u, &value, error);
    } else {
        if (!transfer_address(cpu->state.r[13],
                              (instruction & 0xffu) * 4u, 4u, instruction,
                              cpu, &address, error)) {
            return SEMU_ERR_UNSUPPORTED;
        }
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
        return refuse(cpu, instruction, error);
    }
    if (cpu->state.r[13] < armv7m_bit_count(list) * 4u ||
        !access_address_aligned(cpu->state.r[13] - armv7m_bit_count(list) * 4u,
                        0u, 4u, &address)) {
        return refuse(cpu, instruction, error);
    }
    for (reg = 0u; reg < 15u; ++reg) {
        if ((list & (1u << reg)) != 0u) {
            if (!access_address_aligned(address, 0u, 4u, &address) ||
                armv7m_write(cpu, address, 4u, cpu->state.r[reg], error) !=
                    SEMU_OK) {
                return error != NULL ? error->code : SEMU_ERR_RANGE;
            }
            if (armv7m_bit_count(list >> (reg + 1u)) != 0u &&
                !add_u32(address, 4u, &address)) {
                return refuse(cpu, instruction, error);
            }
        }
    }
    armv7m_set_sp(cpu, cpu->state.r[13] - armv7m_bit_count(list) * 4u);
    return SEMU_OK;
}

static semu_status pop(semu_cpu *cpu, uint16_t instruction,
                       semu_error *error)
{
    uint32_t list = instruction & 0xffu;
    uint32_t values[9];
    uint32_t address = cpu->state.r[13];
    uint32_t end;
    unsigned count = 0u;
    unsigned reg;

    if ((instruction & 0x0100u) != 0u) {
        list |= 1u << 15;
    }
    if (list == 0u || !access_address_aligned(address, 0u, 4u, &address) ||
        !add_u32(cpu->state.r[13], armv7m_bit_count(list) * 4u, &end)) {
        return refuse(cpu, instruction, error);
    }
    address = cpu->state.r[13];
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
    uint32_t end;
    unsigned reg;
    int load = (instruction & 0x0800u) != 0u;

    if (list == 0u || !access_address_aligned(address, 0u, 4u, &address) ||
        !add_u32(cpu->state.r[rn], armv7m_bit_count(list) * 4u, &end)) {
        return refuse(cpu, instruction, error);
    }
    address = cpu->state.r[rn];
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
            if (!add_u32(address, 4u, &address)) {
                return refuse(cpu, instruction, error);
            }
        }
    }
    if (!load || (list & (1u << rn)) == 0u) {
        cpu->state.r[rn] += armv7m_bit_count(list) * 4u;
    }
    return SEMU_OK;
}

semu_status armv7m_exec16_memory(semu_cpu *cpu, uint16_t instruction,
                                 uint32_t pc, semu_error *error)
{
    semu_status status;

    if ((instruction & 0xf000u) == 0x5000u) {
        status = transfer(cpu, instruction, error);
    } else if ((instruction & 0xe000u) == 0x6000u ||
               (instruction & 0xf000u) == 0x8000u) {
        status = immediate_transfer(cpu, instruction, error);
    } else if ((instruction & 0xf800u) == 0x4800u ||
               (instruction & 0xf000u) == 0x9000u) {
        status = sp_or_literal(cpu, instruction, pc, error);
    } else if ((instruction & 0xfe00u) == 0xb400u) {
        status = push(cpu, instruction, error);
    } else if ((instruction & 0xfe00u) == 0xbc00u) {
        status = pop(cpu, instruction, error);
    } else if ((instruction & 0xf000u) == 0xc000u) {
        status = multiple(cpu, instruction, error);
    } else {
        status = refuse(cpu, instruction, error);
    }
    if (status != SEMU_OK && cpu->state.halted) {
        cpu->state.r[15] = pc;
    }
    return status;
}
