#include "armv7m_internal.h"

#include <stdint.h>

#define ARMV7M_XPSR_Q (1u << 27)
#define ARMV7M_XPSR_GE (0x0fu << 16)

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

static int64_t signed32(uint32_t value)
{
    int64_t result = (int64_t)(value & 0x7fffffffu);
    return (value & 0x80000000u) != 0u ? result - 2147483648LL : result;
}

static int64_t signed_lane(uint32_t value, unsigned shift, unsigned width)
{
    uint32_t mask = (1u << width) - 1u;
    uint32_t sign = 1u << (width - 1u);
    uint32_t raw = (value >> shift) & mask;
    int64_t result = (int64_t)(raw & (sign - 1u));
    return (raw & sign) != 0u ? result - (int64_t)sign : result;
}

static uint32_t signed_bits(int64_t value)
{
    if (value < 0) return 0u - (uint32_t)(-value);
    return (uint32_t)value;
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

static uint32_t saturate_signed(int64_t value, unsigned width, int *saturated)
{
    int64_t limit = 1LL << (width - 1u);
    int64_t high = limit - 1;
    int64_t low = -limit;
    if (value > high) {
        *saturated = 1;
        return signed_bits(high);
    }
    if (value < low) {
        *saturated = 1;
        return signed_bits(low);
    }
    *saturated = 0;
    return signed_bits(value);
}

static uint32_t saturate_unsigned(int64_t value, unsigned width, int *saturated)
{
    int64_t high = (1LL << width) - 1;
    if (value < 0) {
        *saturated = 1;
        return 0u;
    }
    if (value > high) {
        *saturated = 1;
        return (uint32_t)high;
    }
    *saturated = 0;
    return (uint32_t)value;
}

static semu_status multiply_short(semu_cpu *cpu, uint16_t first,
                                   uint16_t second, uint32_t pc,
                                   semu_error *error)
{
    unsigned op = (first >> 4u) & 7u, rn = first & 15u;
    unsigned ra = second >> 12u, rd = (second >> 8u) & 15u;
    unsigned rm = second & 15u, form = (second >> 4u) & 3u;
    uint64_t product;
    int64_t signed_product;
    if (op == 1u) {
        if ((second & 0x00c0u) != 0u || !data_register(rn) ||
            !data_register(rm) || !data_register(rd) ||
            (ra != 15u && !data_register(ra)))
            return refuse(cpu, first, second, pc, error);
        signed_product = signed_lane(cpu->state.r[rn], 0u, 16u) *
                         signed_lane(cpu->state.r[rm], 0u, 16u);
        cpu->state.r[rd] = signed_bits(signed_product +
                                       (ra == 15u ? 0 :
                                        signed32(cpu->state.r[ra])));
        return SEMU_OK;
    }
    if (op != 0u || (second & 0x00c0u) != 0u || !data_register(rn) ||
        !data_register(rm) || !data_register(rd) ||
        (form == 0u && ra != 15u && !data_register(ra)) ||
        (form == 1u && !data_register(ra)))
        return refuse(cpu, first, second, pc, error);
    if (form == 0u && ra == 15u) {
        product = (uint64_t)cpu->state.r[rn] * cpu->state.r[rm];
        cpu->state.r[rd] = (uint32_t)product;
        return SEMU_OK;
    }
    if (form == 0u) {
        product = (uint64_t)cpu->state.r[rn] * cpu->state.r[rm];
        cpu->state.r[rd] = (uint32_t)(product + cpu->state.r[ra]);
        return SEMU_OK;
    }
    if (form == 1u) {
        product = (uint64_t)cpu->state.r[rn] * cpu->state.r[rm];
        cpu->state.r[rd] = cpu->state.r[ra] - (uint32_t)product;
        return SEMU_OK;
    }
    return refuse(cpu, first, second, pc, error);
}

static semu_status multiply_long(semu_cpu *cpu, uint16_t first,
                                 uint16_t second, uint32_t pc,
                                 semu_error *error)
{
    unsigned op = (first >> 4u) & 15u, rn = first & 15u;
    unsigned rdlo = (second >> 12u) & 15u, rdhi = (second >> 8u) & 15u;
    unsigned rm = second & 15u;
    uint64_t result, product, old;
    int64_t numerator, denominator, quotient;

    if (!data_register(rn) || !data_register(rm))
        return refuse(cpu, first, second, pc, error);
    if (op == 9u || op == 11u) {
        if ((second & 0x00f0u) != 0x00f0u || rdlo != 15u ||
            !data_register(rdhi))
            return refuse(cpu, first, second, pc, error);
        if (cpu->state.r[rm] == 0u) {
            cpu->state.r[rdhi] = 0u;
            return SEMU_OK;
        }
        if (op == 11u) {
            cpu->state.r[rdhi] = cpu->state.r[rn] / cpu->state.r[rm];
            return SEMU_OK;
        }
        numerator = signed32(cpu->state.r[rn]);
        denominator = signed32(cpu->state.r[rm]);
        if (numerator == -2147483648LL && denominator == -1LL) {
            cpu->state.r[rdhi] = 0x80000000u;
            return SEMU_OK;
        }
        quotient = numerator / denominator;
        cpu->state.r[rdhi] = signed_bits(quotient);
        return SEMU_OK;
    }
    if (!data_register(rdlo) || !data_register(rdhi) || rdlo == rdhi || (second & 0x00f0u) != 0u)
        return refuse(cpu, first, second, pc, error);
    if (op == 8u || op == 12u) {
        product = (uint64_t)(signed32(cpu->state.r[rn]) *
                             signed32(cpu->state.r[rm]));
    } else if (op == 10u || op == 14u) {
        product = (uint64_t)cpu->state.r[rn] * cpu->state.r[rm];
    } else {
        return refuse(cpu, first, second, pc, error);
    }
    if (op == 12u || op == 14u) {
        old = ((uint64_t)cpu->state.r[rdhi] << 32u) | cpu->state.r[rdlo];
        result = product + old;
    } else {
        result = product;
    }
    cpu->state.r[rdlo] = (uint32_t)result;
    cpu->state.r[rdhi] = (uint32_t)(result >> 32u);
    return SEMU_OK;
}
static semu_status saturation(semu_cpu *cpu, uint16_t first,
                              uint16_t second, uint32_t pc,
                              semu_error *error)
{
    unsigned rn = first & 15u, rd = (second >> 8u) & 15u;
    unsigned sh = (first >> 5u) & 1u;
    unsigned shift = (((second >> 12u) & 7u) << 2u) |
                     ((second >> 6u) & 3u);
    unsigned raw = second & 31u, width;
    int is_unsigned = (first & 0x80u) != 0u, saturated, lane_saturated;
    uint32_t result, value;

    if ((first & 0x0050u) != 0u ||
        ((first & 0xff80u) != 0xf300u &&
         (first & 0xff80u) != 0xf380u) ||
        !data_register(rn) || !data_register(rd) || (second & 0x20u) != 0u)
        return refuse(cpu, first, second, pc, error);
    if (sh != 0u && shift == 0u) {
        if ((second & 0x70d0u) != 0u) return refuse(cpu, first, second, pc,
                                                    error);
        if (is_unsigned) {
            if (raw > 15u) return refuse(cpu, first, second, pc, error);
            unsigned limit = raw & 15u;
            result = saturate_unsigned(signed_lane(cpu->state.r[rn], 0u, 16u),
                                       limit, &saturated);
            result |= saturate_unsigned(signed_lane(cpu->state.r[rn], 16u,
                                                    16u), limit, &lane_saturated)
                      << 16u;
            saturated |= lane_saturated;
        } else {
            if (raw > 15u) return refuse(cpu, first, second, pc, error);
            raw += 1u;
            result = saturate_signed(signed_lane(cpu->state.r[rn], 0u, 16u),
                                     raw, &saturated) & 0xffffu;
            result |= (saturate_signed(signed_lane(cpu->state.r[rn], 16u,
                                                   16u), raw,
                                       &lane_saturated) & 0xffffu) << 16u;
            saturated |= lane_saturated;
        }
        cpu->state.r[rd] = result;
        if (saturated != 0) cpu->state.xpsr |= ARMV7M_XPSR_Q;
        return SEMU_OK;
    }
    if (sh == 0u) value = cpu->state.r[rn] << shift;
    else if (shift == 0u || shift > 31u)
        return refuse(cpu, first, second, pc, error);
    else value = asr32(cpu->state.r[rn], shift);
    width = is_unsigned ? raw : raw + 1u;
    result = is_unsigned ? saturate_unsigned(signed32(value), width,
                                             &saturated) :
                           saturate_signed(signed32(value), width,
                                           &saturated);
    cpu->state.r[rd] = result;
    if (saturated != 0) cpu->state.xpsr |= ARMV7M_XPSR_Q;
    return SEMU_OK;
}

static uint32_t parallel_lane(uint32_t left, uint32_t right,
                              unsigned left_shift, unsigned right_shift,
                              unsigned width, int subtract, int signed_op,
                              unsigned *ge)
{
    uint32_t mask = (1u << width) - 1u;
    uint32_t a = (left >> left_shift) & mask, b = (right >> right_shift) & mask;
    int64_t signed_result;
    uint64_t unsigned_result;

    if (signed_op) {
        signed_result = signed_lane(left, left_shift, width);
        signed_result = subtract ? signed_result -
                                   signed_lane(right, right_shift, width) :
                                   signed_result +
                                   signed_lane(right, right_shift, width);
        *ge = signed_result >= 0 ? 1u : 0u;
        return signed_bits(signed_result) & mask;
    }
    if (subtract) {
        *ge = a >= b ? 1u : 0u;
        return (a - b) & mask;
    }
    unsigned_result = (uint64_t)a + b;
    *ge = unsigned_result >= ((uint64_t)1u << width) ? 1u : 0u;
    return (uint32_t)unsigned_result & mask;
}
static semu_status parallel(semu_cpu *cpu, uint16_t first,
                            uint16_t second, uint32_t pc, semu_error *error)
{
    unsigned op = (first >> 4u) & 7u, rn = first & 15u;
    unsigned rd = (second >> 8u) & 15u, rm = second & 15u;
    unsigned form = second & 0x70u, width, lane, ge = 0u, lane_ge;
    int signed_op = form == 0u, subtract;
    uint32_t left, right, result = 0u;

    if ((form != 0u && form != 0x40u) ||
        (op != 0u && op != 1u && op != 2u && op != 4u && op != 5u &&
         op != 6u) || !data_register(rn) || !data_register(rm) ||
        !data_register(rd) || (second & 0x0080u) != 0u)
        return refuse(cpu, first, second, pc, error);
    left = cpu->state.r[rn];
    right = cpu->state.r[rm];
    if (op == 0u || op == 4u) {
        width = 8u;
        subtract = op == 4u;
        for (lane = 0u; lane < 4u; ++lane) {
            result |= parallel_lane(left, right, lane * 8u, lane * 8u, width,
                                    subtract, signed_op, &lane_ge) <<
                      (lane * 8u);
            ge |= lane_ge << lane;
        }
    } else if (op == 1u || op == 5u) {
        width = 16u;
        subtract = op == 5u;
        for (lane = 0u; lane < 2u; ++lane) {
            result |= parallel_lane(left, right, lane * 16u, lane * 16u, width,
                                    subtract, signed_op, &lane_ge) <<
                      (lane * 16u);
            if (lane_ge != 0u) ge |= 3u << (lane * 2u);
        }
    } else {
        result |= parallel_lane(left, right, 0u, 16u, 16u,
                                op == 2u ? 1 : 0, signed_op, &lane_ge);
        if (lane_ge != 0u) ge |= 3u;
        result |= parallel_lane(left, right, 16u, 0u, 16u,
                                op == 6u ? 1 : 0, signed_op, &lane_ge) <<
                  16u;
        if (lane_ge != 0u) ge |= 3u << 2u;
    }
    cpu->state.r[rd] = result;
    cpu->state.xpsr = (cpu->state.xpsr & ~ARMV7M_XPSR_GE) | (ge << 16u);
    return SEMU_OK;
}
static semu_status miscellaneous(semu_cpu *cpu, uint16_t first,
                                 uint16_t second, uint32_t pc,
                                 semu_error *error)
{
    unsigned op1 = (first >> 4u) & 7u, op2 = second & 0xf0u;
    unsigned rn = first & 15u, rd = (second >> 8u) & 15u, rm = second & 15u;
    uint32_t value, result;
    int sat1, sat2;

    if ((first & 0xff80u) != 0xfa80u || (second & 0x0040u) != 0u)
        return refuse(cpu, first, second, pc, error);
    if (op1 == 0u) {
        if ((op2 != 0x80u && op2 != 0x90u && op2 != 0xa0u &&
             op2 != 0xb0u) || !data_register(rn) || !data_register(rm) ||
            !data_register(rd))
            return refuse(cpu, first, second, pc, error);
        if (op2 == 0x80u) {
            result = saturate_signed(signed32(cpu->state.r[rn]) +
                                     signed32(cpu->state.r[rm]), 32u, &sat1);
        } else if (op2 == 0xa0u) {
            result = saturate_signed(signed32(cpu->state.r[rn]) -
                                     signed32(cpu->state.r[rm]), 32u, &sat1);
        } else {
            int64_t doubled = signed32(cpu->state.r[rm]) * 2LL;
            uint32_t double_bits = saturate_signed(doubled, 32u, &sat1);
            int64_t addend = signed32(cpu->state.r[rn]);
            int64_t double_value = signed32(double_bits);
            int64_t final = op2 == 0x90u ? addend + double_value :
                                            addend - double_value;
            result = saturate_signed(final, 32u, &sat2);
            sat1 |= sat2;
        }
        cpu->state.r[rd] = result;
        if (sat1 != 0) cpu->state.xpsr |= ARMV7M_XPSR_Q;
        return SEMU_OK;
    }
    if (op1 == 1u) {
        if (!data_register(rd) || !data_register(rm) || !data_register(rn) ||
            rn != rm ||
            (op2 != 0x80u && op2 != 0x90u && op2 != 0xa0u &&
             op2 != 0xb0u))
            return refuse(cpu, first, second, pc, error);
        value = cpu->state.r[rm];
        if (op2 == 0x80u)
            result = (value << 24u) | ((value & 0x0000ff00u) << 8u) |
                     ((value & 0x00ff0000u) >> 8u) | (value >> 24u);
        else if (op2 == 0x90u)
            result = ((value & 0x00ff00ffu) << 8u) |
                     ((value & 0xff00ff00u) >> 8u);
        else if (op2 == 0xa0u) {
            unsigned bit;
            result = 0u;
            for (bit = 0u; bit < 32u; ++bit)
                if ((value & (1u << bit)) != 0u)
                    result |= 1u << (31u - bit);
        } else {
            result = ((value & 0x0000ff00u) >> 8u) |
                     ((value & 0x000000ffu) << 8u);
            result = signed_bits(signed_lane(result, 0u, 16u));
        }
        cpu->state.r[rd] = result;
        return SEMU_OK;
    }
    if (op1 == 2u && op2 == 0x80u && data_register(rn) &&
        data_register(rm) && data_register(rd)) {
        value = cpu->state.xpsr >> 16u;
        result = 0u;
        for (op1 = 0u; op1 < 4u; ++op1)
            result |= ((value & (1u << op1)) != 0u ?
                       cpu->state.r[rn] : cpu->state.r[rm]) &
                      (0xffu << (op1 * 8u));
        cpu->state.r[rd] = result;
        return SEMU_OK;
    }
    if (op1 == 3u && op2 == 0x80u && data_register(rn) &&
        rn == rm && data_register(rd)) {
        value = cpu->state.r[rm];
        result = 0u;
        while ((value & 0x80000000u) == 0u && result < 32u) {
            value <<= 1u;
            ++result;
        }
        cpu->state.r[rd] = result;
        return SEMU_OK;
    }
    return refuse(cpu, first, second, pc, error);
}

static semu_status pack_halfword(semu_cpu *cpu, uint16_t first,
                                 uint16_t second, uint32_t pc,
                                 semu_error *error)
{
    unsigned rn = first & 15u, rd = (second >> 8u) & 15u, rm = second & 15u;
    unsigned amount = (((second >> 12u) & 7u) << 2u) |
                      ((second >> 6u) & 3u);
    int top = (second & 0x20u) != 0u;
    uint32_t shifted, result;

    if ((first & 0xfff0u) != 0xeac0u || (second & 0x8010u) != 0u ||
        !data_register(rn) || !data_register(rm) || !data_register(rd) ||
        (!top && amount > 31u) || (top && amount > 32u))
        return refuse(cpu, first, second, pc, error);
    if (top) {
        shifted = amount == 0u ?
                  ((cpu->state.r[rm] & 0x80000000u) != 0u ? 0xffffffffu : 0u) :
                  asr32(cpu->state.r[rm], amount);
    } else {
        shifted = cpu->state.r[rm] << amount;
    }
    result = top ? (cpu->state.r[rn] & 0xffff0000u) | (shifted & 0xffffu) :
                   (cpu->state.r[rn] & 0x0000ffffu) | (shifted & 0xffff0000u);
    cpu->state.r[rd] = result;
    return SEMU_OK;
}

static semu_status extend_add(semu_cpu *cpu, uint16_t first,
                              uint16_t second, uint32_t pc,
                              semu_error *error)
{
    unsigned base = first & 0xfff0u, rn = first & 15u;
    unsigned rd = (second >> 8u) & 15u, rm = second & 15u;
    unsigned rotation = (second >> 4u) & 3u;
    uint32_t rotated, addend, result;
    int64_t low, high;

    if ((base != 0xfa00u && base != 0xfa10u && base != 0xfa20u &&
         base != 0xfa30u && base != 0xfa40u && base != 0xfa50u) ||
        (second & 0x00c0u) != 0x0080u || !data_register(rd) ||
        !data_register(rm) || (rn != 15u && !data_register(rn)))
        return refuse(cpu, first, second, pc, error);
    rotated = ror32(cpu->state.r[rm], rotation * 8u);
    addend = rn == 15u ? 0u : cpu->state.r[rn];
    if (base == 0xfa00u || base == 0xfa10u) {
        uint32_t extracted = rotated & 0xffffu;
        if (base == 0xfa00u) extracted = signed_bits(signed_lane(rotated, 0u,
                                                                  16u));
        result = addend + extracted;
    } else if (base == 0xfa40u || base == 0xfa50u) {
        uint32_t extracted = (rotated & 0xffu);
        if (base == 0xfa40u) extracted = signed_bits(signed_lane(rotated, 0u,
                                                                  8u));
        result = addend + extracted;
    } else {
        low = (int64_t)(addend & 0xffffu) +
              (base == 0xfa20u ? signed_lane(rotated, 0u, 8u) :
                                  (int64_t)(rotated & 0xffu));
        high = (int64_t)(addend >> 16u) +
               (base == 0xfa20u ? signed_lane(rotated, 16u, 8u) :
                                   (int64_t)((rotated >> 16u) & 0xffu));
        result = (signed_bits(low) & 0xffffu) |
                 ((signed_bits(high) & 0xffffu) << 16u);
    }
    cpu->state.r[rd] = result;
    return SEMU_OK;
}
semu_status armv7m_exec32_dsp(semu_cpu *cpu, uint16_t first,
                              uint16_t second, uint32_t pc,
                              semu_error *error)
{
    if ((first & 0xff00u) == 0xfb00u) {
        unsigned op = (first >> 4u) & 15u;
        if (op <= 7u) return multiply_short(cpu, first, second, pc, error);
        return multiply_long(cpu, first, second, pc, error);
    }
    if ((first & 0xfff0u) == 0xeac0u)
        return pack_halfword(cpu, first, second, pc, error);
    if ((first & 0xff80u) == 0xf300u ||
        (first & 0xff80u) == 0xf380u)
        return saturation(cpu, first, second, pc, error);
    if ((first & 0xfb80u) == 0xfa00u) {
        if ((second & 0x8080u) == 0x8000u)
            return armv7m_exec32_shift(cpu, first, second, pc, error);
        return extend_add(cpu, first, second, pc, error);
    }
    if ((first & 0xff80u) == 0xfa80u) {
        unsigned form = second & 0x70u;
        if ((second & 0x80u) == 0u && (form == 0u || form == 0x40u))
            return parallel(cpu, first, second, pc, error);
        return miscellaneous(cpu, first, second, pc, error);
    }
    return refuse(cpu, first, second, pc, error);
}
