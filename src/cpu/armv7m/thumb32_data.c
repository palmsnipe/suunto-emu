#include "armv7m_internal.h"

static semu_status refuse(semu_cpu *cpu, uint16_t first, uint16_t second,
                          uint32_t pc, semu_error *error)
{
    cpu->state.r[15] = pc;
    return armv7m_unsupported(cpu, ((uint32_t)first << 16) | second, error);
}

static int data_register(unsigned reg) { return reg < 13u || reg == 14u; }

static void write_register(semu_cpu *cpu, unsigned reg, uint32_t value)
{
    if (reg == 13u) armv7m_set_sp(cpu, value); else cpu->state.r[reg] = value;
}

static unsigned shift_amount(uint16_t second)
{
    return ((second >> 10u) & 0x1cu) | ((second >> 6u) & 3u);
}

static uint32_t shifted(uint32_t value, unsigned type, unsigned amount,
                        unsigned carry_in, unsigned *carry_out)
{
    uint32_t result = value;
    *carry_out = carry_in;
    if (amount == 0u) {
        if (type == 1u) {
            *carry_out = value >> 31u;
            return 0u;
        }
        if (type == 2u) {
            *carry_out = value >> 31u;
            return (value & 0x80000000u) != 0u ? 0xffffffffu : 0u;
        }
        if (type == 3u) {
            *carry_out = value & 1u;
            return (value >> 1u) | (carry_in << 31u);
        }
        return result;
    }
    if (type == 0u) {
        *carry_out = (value >> (32u - amount)) & 1u;
        return amount < 32u ? value << amount : 0u;
    }
    if (type == 1u) {
        *carry_out = (value >> (amount - 1u)) & 1u;
        return amount < 32u ? value >> amount : 0u;
    }
    *carry_out = (value >> (amount - 1u)) & 1u;
    if (type == 2u) {
        if (amount >= 32u) return (value & 0x80000000u) != 0u ?
                                      0xffffffffu : 0u;
        result = value >> amount;
        if ((value & 0x80000000u) != 0u) result |= 0xffffffffu << (32u - amount);
        return result;
    }
    return (value >> amount) | (value << (32u - amount));
}

static uint32_t modified_immediate(semu_cpu *cpu, uint16_t first,
                                   uint16_t second, unsigned *carry_out)
{
    uint32_t imm12 = ((uint32_t)(first & 0x0400u) << 1u) |
                     ((uint32_t)(second & 0x7000u) >> 4u) |
                     (second & 0xffu);
    uint32_t value = armv7m_expand_modified_immediate(first, second);
    *carry_out = (imm12 & 0xc00u) == 0u ?
                 (unsigned)((cpu->state.xpsr & ARMV7M_XPSR_C) != 0u) :
                 value >> 31;
    return value;
}

static void logical_flags(semu_cpu *cpu, uint32_t value, unsigned carry)
{
    armv7m_set_nz(cpu, value);
    cpu->state.xpsr = (cpu->state.xpsr & ~ARMV7M_XPSR_C) |
                      (carry != 0u ? ARMV7M_XPSR_C : 0u);
}

static uint32_t logical(unsigned operation, uint32_t left, uint32_t right)
{
    switch (operation) {
    case 0u: return left & right;
    case 1u: return left & ~right;
    case 2u: return left | right;
    case 3u: return left | ~right;
    default: return left ^ right;
    }
}

static int arithmetic_operation(unsigned raw)
{
    switch (raw) {
    case 0u: case 8u: return 0;
    case 2u: case 10u: return 1;
    case 3u: case 11u: return 2;
    case 5u: case 13u: return 3;
    case 6u: case 14u: return 4;
    default: return -1;
    }
}

static uint32_t arithmetic(semu_cpu *cpu, unsigned operation,
                           uint32_t left, uint32_t right, int set_flags)
{
    unsigned carry = (cpu->state.xpsr & ARMV7M_XPSR_C) != 0u;
    switch (operation) {
    case 0u: return armv7m_add(cpu, left, right, 0u, set_flags);
    case 1u: return armv7m_add(cpu, left, right, carry, set_flags);
    case 2u: return armv7m_add(cpu, left, ~right, carry, set_flags);
    case 3u: return armv7m_add(cpu, left, ~right, 1u, set_flags);
    default: return armv7m_add(cpu, right, ~left, 1u, set_flags);
    }
}

static semu_status move_register(semu_cpu *cpu, uint16_t first,
                                 uint16_t second, uint32_t pc,
                                 semu_error *error)
{
    unsigned rd = (second >> 8u) & 15u, rm = second & 15u;
    unsigned s = (first >> 4u) & 1u, carry, type = (second >> 4u) & 3u;
    uint32_t value;
    if ((first & 0xffcfu) != 0xea4fu || rd == 15u || rm == 15u ||
        ((s != 0u || (first & 0x20u) != 0u) && (rd == 13u || rm == 13u)))
        return refuse(cpu, first, second, pc, error);
    value = shifted(cpu->state.r[rm], type, shift_amount(second),
                    (cpu->state.xpsr & ARMV7M_XPSR_C) != 0u, &carry);
    if ((first & 0x20u) != 0u) value = ~value;
    write_register(cpu, rd, value);
    if (s != 0u) logical_flags(cpu, value, carry);
    return SEMU_OK;
}

static semu_status register_data(semu_cpu *cpu, uint16_t first,
                                 uint16_t second, uint32_t pc,
                                 semu_error *error)
{
    unsigned raw = (first >> 5u) & 7u, rn = first & 15u;
    unsigned rd = (second >> 8u) & 15u, rm = second & 15u;
    unsigned s = (first >> 4u) & 1u, carry;
    uint32_t right, result;
    int operation;
    if ((second & 0x8000u) != 0u) return refuse(cpu, first, second, pc, error);
    if ((first & 0xff00u) == 0xea00u) {
        if (raw > 4u || !data_register(rn) || !data_register(rm) ||
            rd == 13u || (rd == 15u &&
            (s == 0u || (raw != 0u && raw != 4u))))
            return refuse(cpu, first, second, pc, error);
        right = shifted(cpu->state.r[rm], (second >> 4u) & 3u,
                        shift_amount(second),
                        (cpu->state.xpsr & ARMV7M_XPSR_C) != 0u, &carry);
        result = logical(raw, cpu->state.r[rn], right);
        if (rd != 15u) write_register(cpu, rd, result);
        if (s != 0u) logical_flags(cpu, result, carry);
        return SEMU_OK;
    }
    if ((first & 0xff00u) != 0xeb00u ||
        (operation = arithmetic_operation(raw)) < 0 || !data_register(rm) ||
        (rn == 13u && raw != 0u && raw != 5u) ||
        (!data_register(rn) && rn != 13u) ||
        (rd == 15u && (s == 0u || (raw != 0u && raw != 5u))) ||
        (rd == 13u && raw != 0u && raw != 5u))
        return refuse(cpu, first, second, pc, error);
    right = shifted(cpu->state.r[rm], (second >> 4u) & 3u,
                    shift_amount(second),
                    (cpu->state.xpsr & ARMV7M_XPSR_C) != 0u, &carry);
    result = arithmetic(cpu, (unsigned)operation,
                        rn == 13u ? cpu->state.r[13] : cpu->state.r[rn],
                        right, s != 0u || rd == 15u);
    if (rd != 15u) write_register(cpu, rd, result);
    return SEMU_OK;
}

static semu_status modified_data(semu_cpu *cpu, uint16_t first,
                                 uint16_t second, uint32_t pc,
                                 semu_error *error)
{
    unsigned raw = (first & 0x1e0u) >> 5u, rn = first & 15u;
    unsigned rd = (second >> 8u) & 15u, s = (first >> 4u) & 1u, carry;
    uint32_t left, right, result;
    int operation;
    if ((second & 0x8000u) != 0u) return refuse(cpu, first, second, pc, error);
    operation = raw < 5u ? (int)raw : arithmetic_operation(raw);
    if (operation < 0) return refuse(cpu, first, second, pc, error);
    if (raw < 5u) {
        if (rn == 15u && (raw == 2u || raw == 3u)) left = 0u;
        else if (!data_register(rn)) return refuse(cpu, first, second, pc, error);
        else left = cpu->state.r[rn];
        if (rd == 15u && (s == 0u || (raw != 0u && raw != 4u)))
            return refuse(cpu, first, second, pc, error);
        if (rd == 13u) return refuse(cpu, first, second, pc, error);
        right = modified_immediate(cpu, first, second, &carry);
        result = logical(raw, left, right);
        if (rd != 15u) write_register(cpu, rd, result);
        if (s != 0u) logical_flags(cpu, result, carry);
        return SEMU_OK;
    }
    if (rn == 15u || (rn == 13u && raw != 8u && raw != 13u) ||
        (rd == 13u && (rn != 13u || (raw != 8u && raw != 13u))) ||
        (rd == 15u &&
        (s == 0u || (raw != 8u && raw != 13u))))
        return refuse(cpu, first, second, pc, error);
    right = modified_immediate(cpu, first, second, &carry);
    result = arithmetic(cpu, (unsigned)operation, cpu->state.r[rn], right, s);
    if (rd != 15u) write_register(cpu, rd, result);
    return SEMU_OK;
}

static semu_status wide_data(semu_cpu *cpu, uint16_t first, uint16_t second,
                             uint32_t pc, semu_error *error)
{
    unsigned rd = (second >> 8u) & 15u, rn = first & 15u;
    uint32_t imm, base;
    if ((second & 0x8000u) != 0u || rd == 15u)
        return refuse(cpu, first, second, pc, error);
    if ((first & 0xf3f0u) == 0xf240u || (first & 0xf3f0u) == 0xf2c0u) {
        if (rd == 13u) return refuse(cpu, first, second, pc, error);
        imm = ((uint32_t)(first & 15u) << 12u) |
              ((uint32_t)(first & 0x400u) << 1u) |
              ((uint32_t)(second & 0x7000u) >> 4u) | (second & 0xffu);
        cpu->state.r[rd] = (first & 0x80u) != 0u ?
                           (cpu->state.r[rd] & 0xffffu) | (imm << 16u) : imm;
        return SEMU_OK;
    }
    if ((first & 0xf3e0u) != 0xf200u && (first & 0xf3e0u) != 0xf2a0u)
        return refuse(cpu, first, second, pc, error);
    imm = ((uint32_t)(first & 0x400u) << 1u) |
          ((uint32_t)(second & 0x7000u) >> 4u) | (second & 0xffu);
    if (rn == 15u) base = (pc + 4u) & ~3u;
    else if (rn <= 13u) base = cpu->state.r[rn];
    else return refuse(cpu, first, second, pc, error);
    if (rd == 13u && rn != 13u)
        return refuse(cpu, first, second, pc, error);
    write_register(cpu, rd, (first & 0x80u) != 0u ? base - imm : base + imm);
    return SEMU_OK;
}

static semu_status clz_data(semu_cpu *cpu, uint16_t first, uint16_t second,
                            uint32_t pc, semu_error *error)
{
    unsigned rd = (second >> 8u) & 15u, rm = first & 15u, count = 0u;
    uint32_t value = cpu->state.r[rm];
    if (!data_register(rd) || !data_register(rm))
        return refuse(cpu, first, second, pc, error);
    while ((value & 0x80000000u) == 0u && count < 32u) {
        value <<= 1; ++count;
    }
    cpu->state.r[rd] = count;
    return SEMU_OK;
}

static semu_status bitfield_data(semu_cpu *cpu, uint16_t first,
                                 uint16_t second, uint32_t pc,
                                 semu_error *error)
{
    unsigned rd = (second >> 8u) & 15u, rn = first & 15u;
    unsigned lsb = shift_amount(second), field = second & 31u, width;
    uint32_t mask, value;
    if ((second & 0x8000u) != 0u || !data_register(rd))
        return refuse(cpu, first, second, pc, error);
    if ((first & 0xfff0u) == 0xf360u) {
        if (rn != 15u && !data_register(rn))
            return refuse(cpu, first, second, pc, error);
        if (lsb > field) return refuse(cpu, first, second, pc, error);
        width = field - lsb + 1u;
        mask = width == 32u ? 0xffffffffu : ((1u << width) - 1u) << lsb;
        value = rn == 15u ? 0u : cpu->state.r[rn];
        cpu->state.r[rd] = (cpu->state.r[rd] & ~mask) |
                           ((value << lsb) & mask);
        return SEMU_OK;
    }
    if ((first & 0xfff0u) != 0xf340u && (first & 0xfff0u) != 0xf3c0u)
        return refuse(cpu, first, second, pc, error);
    if (!data_register(rn) || lsb + field + 1u > 32u)
        return refuse(cpu, first, second, pc, error);
    width = field + 1u;
    mask = width == 32u ? 0xffffffffu : (1u << width) - 1u;
    value = (cpu->state.r[rn] >> lsb) & mask;
    if ((first & 0x80u) == 0u && width < 32u &&
        (value & (1u << (width - 1u))) != 0u) value |= ~mask;
    cpu->state.r[rd] = value;
    return SEMU_OK;
}

static semu_status branch_data(semu_cpu *cpu, uint16_t first,
                               uint16_t second, uint32_t pc)
{
    uint32_t s = (first >> 10u) & 1u, j1 = (second >> 13u) & 1u;
    uint32_t j2 = (second >> 11u) & 1u, i1 = (~(j1 ^ s)) & 1u;
    uint32_t i2 = (~(j2 ^ s)) & 1u;
    uint32_t encoded = (s << 24u) | (i1 << 23u) | (i2 << 22u) |
                       ((uint32_t)(first & 0x3ffu) << 12u) |
                       ((uint32_t)(second & 0x7ffu) << 1u);
    cpu->state.r[15] = pc + 4u +
                       (uint32_t)armv7m_sign_extend(encoded, 25u);
    if ((second & 0x4000u) != 0u) cpu->state.r[14] = (pc + 4u) | 1u;
    return SEMU_OK;
}

static semu_status conditional_branch(semu_cpu *cpu, uint16_t first,
                                       uint16_t second, uint32_t pc,
                                       semu_error *error)
{
    unsigned condition = (first >> 6u) & 0xfu;
    uint32_t s = (first >> 10u) & 1u, j1 = (second >> 13u) & 1u;
    uint32_t j2 = (second >> 11u) & 1u, i1 = (~(j1 ^ s)) & 1u;
    uint32_t i2 = (~(j2 ^ s)) & 1u, encoded;

    if (condition >= 14u)
        return refuse(cpu, first, second, pc, error);
    if (!armv7m_condition_passed(cpu, condition))
        return SEMU_OK;
    encoded = (s << 24u) | (i1 << 23u) | (i2 << 22u) |
              ((uint32_t)(first & 0x3fu) << 12u) |
              ((uint32_t)(second & 0x7ffu) << 1u);
    cpu->state.r[15] = pc + 4u +
                       (uint32_t)armv7m_sign_extend(encoded, 25u);
    return SEMU_OK;
}

semu_status armv7m_exec32_data(semu_cpu *cpu, uint16_t first,
                               uint16_t second, uint32_t pc,
                               semu_error *error)
{
    if ((first & 0xf800u) == 0xf000u &&
        (second & 0xd000u) == 0x8000u)
        return conditional_branch(cpu, first, second, pc, error);
    if ((first & 0xf800u) == 0xf000u &&
        ((second & 0xd000u) == 0x9000u || (second & 0xd000u) == 0xd000u))
        return branch_data(cpu, first, second, pc);
    if ((first & 0xffcfu) == 0xea4fu)
        return move_register(cpu, first, second, pc, error);
    if ((first & 0xff00u) == 0xea00u || (first & 0xff00u) == 0xeb00u)
        return register_data(cpu, first, second, pc, error);
    if ((first & 0xfb00u) == 0xf200u)
        return wide_data(cpu, first, second, pc, error);
    if ((first & 0xfff0u) == 0xfab0u &&
        (second & 0xf0f0u) == 0xf080u && (second & 15u) == (first & 15u))
        return clz_data(cpu, first, second, pc, error);
    if ((first & 0xfff0u) == 0xf360u ||
        (first & 0xfff0u) == 0xf340u || (first & 0xfff0u) == 0xf3c0u)
        return bitfield_data(cpu, first, second, pc, error);
    if ((first & 0xf800u) == 0xf000u && (first & 0x200u) == 0u)
        return modified_data(cpu, first, second, pc, error);
    return refuse(cpu, first, second, pc, error);
}
