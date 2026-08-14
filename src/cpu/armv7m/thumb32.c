#include "armv7m_internal.h"

static unsigned bit_count(uint32_t value)
{
    unsigned count = 0u;
    while (value != 0u) {
        count += value & 1u;
        value >>= 1;
    }
    return count;
}

static semu_status move_register(semu_cpu *cpu, uint16_t first,
                                 uint16_t second, uint32_t pc,
                                 semu_error *error)
{
    unsigned rd = (second >> 8) & 15u;
    unsigned rm = second & 15u;
    uint32_t value = armv7m_reg(cpu, rm, pc);

    (void)first;
    if ((second & 0x70f0u) != 0u || rd == 15u) {
        return armv7m_unsupported(cpu, ((uint32_t)first << 16) | second,
                                  error);
    }
    cpu->state.r[rd] = value;
    if (rd == 13u) {
        armv7m_set_sp(cpu, value);
    }
    return SEMU_OK;
}

static semu_status store_multiple_decrement(semu_cpu *cpu, uint16_t first,
                                            uint16_t second,
                                            semu_error *error)
{
    unsigned rn = first & 15u;
    uint32_t list = second;
    uint32_t address;
    unsigned reg;

    if (list == 0u || (list & (1u << 15)) != 0u ||
        (list & (1u << rn)) != 0u) {
        return armv7m_unsupported(cpu, ((uint32_t)first << 16) | second,
                                  error);
    }
    address = cpu->state.r[rn] - bit_count(list) * 4u;
    for (reg = 0u; reg < 15u; ++reg) {
        if ((list & (1u << reg)) != 0u) {
            if (armv7m_write(cpu, address, 4u, cpu->state.r[reg], error) !=
                SEMU_OK) {
                return error != NULL ? error->code : SEMU_ERR_RANGE;
            }
            address += 4u;
        }
    }
    if (rn == 13u) {
        armv7m_set_sp(cpu, cpu->state.r[rn] - bit_count(list) * 4u);
    } else {
        cpu->state.r[rn] -= bit_count(list) * 4u;
    }
    return SEMU_OK;
}

static semu_status wide_transfer(semu_cpu *cpu, uint16_t first,
                                 uint16_t second, uint32_t pc,
                                 semu_error *error)
{
    unsigned kind = (first >> 4) & 15u;
    unsigned rn = first & 15u;
    unsigned rt = (second >> 12) & 15u;
    unsigned width;
    int load;
    uint32_t base;
    uint32_t address;
    uint32_t value;
    semu_status status;

    if (kind == 8u || kind == 9u) {
        width = 1u;
    } else if (kind == 10u || kind == 11u) {
        width = 2u;
    } else {
        width = 4u;
    }
    load = (kind & 1u) != 0u;
    if ((!load && rt == 15u) || (load && rt == 15u && width != 4u)) {
        return armv7m_unsupported(cpu, ((uint32_t)first << 16) | second,
                                  error);
    }
    base = rn == 15u ? ((pc + 4u) & ~3u) : cpu->state.r[rn];
    address = base + (second & 0xfffu);
    if (!load) {
        return armv7m_write(cpu, address, width, cpu->state.r[rt], error);
    }
    status = armv7m_read(cpu, address, width, &value, error);
    if (status != SEMU_OK) {
        return status;
    }
    if (rt == 15u) {
        return armv7m_branch_exchange(cpu, value, error);
    }
    cpu->state.r[rt] = value;
    return SEMU_OK;
}

static semu_status indexed_transfer(semu_cpu *cpu, uint16_t first,
                                    uint16_t second, semu_error *error)
{
    unsigned rn = first & 15u;
    unsigned rt = (second >> 12u) & 15u;
    uint32_t base = cpu->state.r[rn];
    uint32_t immediate = second & 0xffu;
    uint32_t offset = (second & 0x0200u) != 0u
                          ? base + immediate : base - immediate;
    uint32_t address = (second & 0x0400u) != 0u ? offset : base;
    int load = (first & 0x0010u) != 0u;
    unsigned width = 1u << ((first >> 5u) & 3u);
    semu_status status;
    uint32_t value;
    if (rn == 15u || rt == 15u || (second & 0x0800u) == 0u) {
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    }
    if (load) {
        status = armv7m_read(cpu, address, width, &value, error);
        if (status == SEMU_OK) {
            cpu->state.r[rt] = value;
        }
    } else {
        status = armv7m_write(cpu, address, width, cpu->state.r[rt], error);
    }
    if (status == SEMU_OK && (second & 0x0100u) != 0u) {
        cpu->state.r[rn] = offset;
        if (rn == 13u) {
            armv7m_set_sp(cpu, offset);
        }
    }
    return status;
}

static semu_status move_immediate(semu_cpu *cpu, uint16_t first,
                                  uint16_t second)
{
    unsigned rd = (second >> 8) & 15u;
    uint32_t immediate = ((uint32_t)(first & 0x000fu) << 12) |
                         ((uint32_t)(first & 0x0400u) << 1) |
                         ((uint32_t)(second & 0x7000u) >> 4) |
                         (second & 0x00ffu);

    if ((first & 0x0080u) != 0u) {
        cpu->state.r[rd] = (cpu->state.r[rd] & 0x0000ffffu) |
                           (immediate << 16);
    } else {
        cpu->state.r[rd] = immediate;
    }
    return SEMU_OK;
}

static uint32_t rotate_right(uint32_t value, unsigned amount)
{
    amount &= 31u;
    return amount == 0u ? value : (value >> amount) | (value << (32u - amount));
}

static uint32_t expand_modified_immediate(uint16_t first, uint16_t second)
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

static uint32_t shifted_register(uint32_t value, uint16_t second)
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

static semu_status branch(semu_cpu *cpu, uint16_t first, uint16_t second,
                          uint32_t pc, semu_error *error)
{
    uint32_t s = (first >> 10) & 1u;
    uint32_t j1 = (second >> 13) & 1u;
    uint32_t j2 = (second >> 11) & 1u;
    uint32_t i1 = (~(j1 ^ s)) & 1u;
    uint32_t i2 = (~(j2 ^ s)) & 1u;
    uint32_t encoded = (s << 24) | (i1 << 23) | (i2 << 22) |
                       ((uint32_t)(first & 0x03ffu) << 12) |
                       ((uint32_t)(second & 0x07ffu) << 1);
    uint32_t target = pc + 4u +
                      (uint32_t)armv7m_sign_extend(encoded, 25u);

    (void)error;
    if ((second & 0x4000u) != 0u) {
        cpu->state.r[14] = (pc + 4u) | 1u;
    }
    cpu->state.r[15] = target;
    return SEMU_OK;
}

semu_status armv7m_exec32(semu_cpu *cpu, uint16_t first, uint16_t second,
                          uint32_t pc, semu_error *error)
{
    uint32_t packed = ((uint32_t)first << 16) | second;

    cpu->state.r[15] = pc + 4u;
    if (first == 0xea4fu) {
        return move_register(cpu, first, second, pc, error);
    }
    if ((first & 0xfff0u) == 0xe920u) {
        return store_multiple_decrement(cpu, first, second, error);
    }
    if ((first & 0xff80u) == 0xf880u) {
        return wide_transfer(cpu, first, second, pc, error);
    }
    if ((first & 0xff80u) == 0xf800u) {
        return indexed_transfer(cpu, first, second, error);
    }
    if ((first & 0xfbf0u) == 0xf240u ||
        (first & 0xfbf0u) == 0xf2c0u) {
        if (((second >> 8) & 15u) == 15u) {
            return armv7m_unsupported(cpu, packed, error);
        }
        return move_immediate(cpu, first, second);
    }
    if ((first & 0xfbf0u) == 0xf040u) {
        unsigned rn = first & 15u;
        unsigned rd = (second >> 8u) & 15u;
        if (rd == 15u) {
            return armv7m_unsupported(cpu, packed, error);
        }
        cpu->state.r[rd] = (rn == 15u ? 0u : cpu->state.r[rn]) |
                           expand_modified_immediate(first, second);
        return SEMU_OK;
    }
    if ((first & 0xfbe0u) == 0xf000u) {
        unsigned rn = first & 15u;
        unsigned rd = (second >> 8u) & 15u;
        uint32_t result;
        if (rd == 15u || rn == 15u) {
            return armv7m_unsupported(cpu, packed, error);
        }
        result = cpu->state.r[rn] & expand_modified_immediate(first, second);
        cpu->state.r[rd] = result;
        if ((first & 0x0010u) != 0u) {
            armv7m_set_nz(cpu, result);
        }
        return SEMU_OK;
    }
    if ((first & 0xffe0u) == 0xeb00u ||
        (first & 0xffe0u) == 0xeba0u) {
        unsigned rn = first & 15u;
        unsigned rd = (second >> 8u) & 15u;
        unsigned rm = second & 15u;
        uint32_t operand;
        int subtract = (first & 0x00a0u) == 0x00a0u;
        if (rn == 15u || rd == 15u || rm == 15u || (second & 0x8000u) != 0u) {
            return armv7m_unsupported(cpu, packed, error);
        }
        operand = shifted_register(cpu->state.r[rm], second);
        cpu->state.r[rd] = subtract
            ? armv7m_add(cpu, cpu->state.r[rn], ~operand, 1u,
                         (first & 0x0010u) != 0u)
            : armv7m_add(cpu, cpu->state.r[rn], operand, 0u,
                         (first & 0x0010u) != 0u);
        return SEMU_OK;
    }
    if ((first & 0xf800u) == 0xf000u &&
        ((second & 0xd000u) == 0x9000u ||
         (second & 0xd000u) == 0xd000u)) {
        return branch(cpu, first, second, pc, error);
    }
    if (first == 0xf3bfu &&
        (second == 0x8f2fu || second == 0x8f4fu || second == 0x8f5fu ||
         second == 0x8f6fu)) {
        return SEMU_OK;
    }
    if (first == 0xf3afu && second == 0x8000u) {
        return SEMU_OK;
    }
    if (first == 0xf3afu && second == 0x8003u) {
        cpu->state.waiting_for_interrupt = 1;
        return SEMU_OK;
    }
    if (first == 0xeee1u && (second & 0x0fffu) == 0x0a10u) {
        cpu->state.fpscr = cpu->state.r[(second >> 12u) & 15u];
        return SEMU_OK;
    }
    if (first == 0xeef1u && (second & 0x0fffu) == 0x0a10u) {
        unsigned rd = (second >> 12u) & 15u;
        if (rd == 15u) {
            cpu->state.xpsr = (cpu->state.xpsr & 0x0fffffffu) |
                              (cpu->state.fpscr & 0xf0000000u);
        } else {
            cpu->state.r[rd] = cpu->state.fpscr;
        }
        return SEMU_OK;
    }
    if ((first & 0xfff0u) == 0xf380u && (second & 0xff00u) == 0x8800u) {
        unsigned source = first & 15u;
        unsigned special = second & 0xffu;
        uint32_t value = cpu->state.r[source];
        switch (special) {
        case 0x08u:
            cpu->state.msp = value;
            armv7m_set_sp(cpu, value);
            return SEMU_OK;
        case 0x09u: cpu->state.psp = value; return SEMU_OK;
        case 0x10u: cpu->state.primask = value & 1u; return SEMU_OK;
        case 0x11u: cpu->state.basepri = value & 0xffu; return SEMU_OK;
        case 0x12u:
            if (cpu->state.basepri == 0u ||
                (value & 0xffu) < cpu->state.basepri) {
                cpu->state.basepri = value & 0xffu;
            }
            return SEMU_OK;
        case 0x13u: cpu->state.faultmask = value & 1u; return SEMU_OK;
        case 0x14u: cpu->state.control = value & 7u; return SEMU_OK;
        default: break;
        }
    }
    return armv7m_unsupported(cpu, packed, error);
}
