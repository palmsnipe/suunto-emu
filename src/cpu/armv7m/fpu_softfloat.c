#include "fpu_softfloat_internal.h"
static int sf_top_bit(uint64_t value)
{
    int bit = 63;
    while (bit >= 0 && (value & (UINT64_C(1) << (unsigned)bit)) == 0u)
        --bit;
    return bit;
}
static void sf_set_flag(uint32_t *fpscr, uint32_t flag) { *fpscr |= flag; }
unsigned sf_rounding_mode(uint32_t fpscr)
{ return (unsigned)((fpscr & ARMV7M_FPSCR_RMODE_MASK) >> 22u); }
static uint32_t sf_default_nan(void) { return UINT32_C(0x7fc00000); }
static uint32_t sf_infinity(unsigned sign)
{ return (sign != 0u ? SF_SIGN : 0u) | SF_INFINITY_BITS; }
static uint32_t sf_zero(unsigned sign) { return sign != 0u ? SF_SIGN : 0u; }
static uint64_t sf_shift_right_jam(uint64_t value, unsigned count,
                                   unsigned *sticky)
{
    uint64_t lost;
    if (count == 0u) return value;
    if (count >= 64u) {
        if (value != 0u) *sticky = 1u;
        return 0u;
    }
    lost = value & ((UINT64_C(1) << count) - UINT64_C(1));
    if (lost != 0u) *sticky = 1u;
    return value >> count;
}
sf_value sf_unpack(uint32_t bits, uint32_t *fpscr)
{
    sf_value value;
    unsigned exponent = (unsigned)((bits & SF_EXPONENT) >> 23u);
    uint32_t fraction = bits & SF_FRACTION;
    value.raw = bits;
    value.sign = (bits & SF_SIGN) != 0u ? 1u : 0u;
    value.exponent = 0;
    value.significand = 0u;
    if (exponent == 0u) {
        if (fraction == 0u) {
            value.kind = SF_ZERO;
            return value;
        }
        if ((*fpscr & ARMV7M_FPSCR_FZ) != 0u) {
            sf_set_flag(fpscr, ARMV7M_FPSCR_IDC);
            value.kind = SF_ZERO;
            return value;
        }
        value.kind = SF_FINITE;
        value.exponent = -149;
        value.significand = fraction;
        return value;
    }
    if (exponent == 0xffu) {
        value.kind = fraction == 0u ? SF_INFINITY :
                     ((fraction & SF_QNAN_BIT) != 0u ? SF_QNAN : SF_SNAN);
        return value;
    }
    value.kind = SF_FINITE;
    value.exponent = (int)exponent - 150;
    value.significand = SF_HIDDEN | fraction;
    return value;
}
static uint32_t sf_nan_value(sf_value value, uint32_t *fpscr)
{
    uint32_t result = value.raw | SF_QNAN_BIT;
    if (value.kind == SF_SNAN) sf_set_flag(fpscr, ARMV7M_FPSCR_IOC);
    if ((*fpscr & ARMV7M_FPSCR_DN) != 0u) return sf_default_nan();
    return result;
}
static int sf_process_nans(const sf_value *values, unsigned count,
                           uint32_t *result, uint32_t *fpscr)
{
    unsigned index;
    for (index = 0u; index < count; ++index)
        if (values[index].kind == SF_SNAN) {
            *result = sf_nan_value(values[index], fpscr);
            return 1;
        }
    for (index = 0u; index < count; ++index)
        if (values[index].kind == SF_QNAN) {
            *result = sf_nan_value(values[index], fpscr);
            return 1;
        }
    return 0;
}
static int sf_process_nans2(sf_value left, sf_value right,
                            uint32_t *result, uint32_t *fpscr)
{ const sf_value values[2] = {left, right};
  return sf_process_nans(values, 2u, result, fpscr); }
static uint64_t sf_round_shift(uint64_t value, unsigned shift,
                               unsigned sign, unsigned mode,
                               unsigned sticky, int *inexact)
{
    uint64_t truncated;
    uint64_t remainder;
    uint64_t halfway;
    int greater;
    int tie;
    int round_up;
    if (shift == 0u) {
        *inexact = sticky != 0u;
        return value;
    }
    if (shift < 64u) {
        truncated = value >> shift;
        remainder = value & ((UINT64_C(1) << shift) - UINT64_C(1));
        halfway = UINT64_C(1) << (shift - 1u);
        greater = remainder > halfway ||
                  (remainder == halfway && sticky != 0u);
        tie = remainder == halfway && sticky == 0u;
    } else {
        int top = sf_top_bit(value);
        truncated = 0u;
        greater = top >= 0 && (unsigned)top > shift - 1u;
        tie = top >= 0 && (unsigned)top == shift - 1u && sticky == 0u;
        if (top >= 0 && (unsigned)top == shift - 1u && sticky != 0u)
            greater = 1;
    }
    *inexact = shift >= 64u ? (value != 0u || sticky != 0u) :
               (remainder != 0u || sticky != 0u);
    round_up = 0;
    if (*inexact != 0) {
        if (mode == 1u) round_up = sign == 0u;
        else if (mode == 2u) round_up = sign != 0u;
        else if (mode == 0u)
            round_up = greater || (tie && (truncated & 1u) != 0u);
    }
    if (round_up != 0) ++truncated;
    return truncated;
}
uint32_t sf_round_ext(uint32_t *fpscr, sf_ext value)
{
    int top;
    int magnitude_exponent;
    int shift;
    int inexact;
    uint64_t mantissa;
    unsigned mode;
    if (value.significand == 0u) return sf_zero(value.sign);
    top = sf_top_bit(value.significand);
    magnitude_exponent = value.exponent + top;
    mode = sf_rounding_mode(*fpscr);
    if (magnitude_exponent < -126) {
        if ((*fpscr & ARMV7M_FPSCR_FZ) != 0u) {
            sf_set_flag(fpscr, ARMV7M_FPSCR_UFC);
            return sf_zero(value.sign);
        }
        shift = -149 - value.exponent;
        if (shift > 0) {
            mantissa = sf_round_shift(value.significand, (unsigned)shift,
                                      value.sign, mode, value.sticky,
                                      &inexact);
        } else {
            mantissa = value.significand << (unsigned)(-shift);
            inexact = value.sticky != 0u;
        }
        if (inexact != 0) {
            sf_set_flag(fpscr, ARMV7M_FPSCR_IXC);
            sf_set_flag(fpscr, ARMV7M_FPSCR_UFC);
        }
        if (mantissa >= (uint64_t)SF_HIDDEN)
            return (value.sign != 0u ? SF_SIGN : 0u) | SF_HIDDEN;
        return (value.sign != 0u ? SF_SIGN : 0u) | (uint32_t)mantissa;
    }
    shift = top - 23;
    if (shift > 0) {
        mantissa = sf_round_shift(value.significand, (unsigned)shift,
                                  value.sign, mode, value.sticky, &inexact);
    } else {
        mantissa = value.significand << (unsigned)(-shift);
        inexact = value.sticky != 0u;
    }
    if (inexact != 0) sf_set_flag(fpscr, ARMV7M_FPSCR_IXC);
    if (mantissa >= UINT64_C(0x1000000)) {
        mantissa >>= 1u;
        ++magnitude_exponent;
    }
    if (magnitude_exponent > 127) {
        int to_infinity = mode == 0u ||
                          (mode == 1u && value.sign == 0u) ||
                          (mode == 2u && value.sign != 0u);
        sf_set_flag(fpscr, ARMV7M_FPSCR_OFC | ARMV7M_FPSCR_IXC);
        return to_infinity ? sf_infinity(value.sign) :
               ((value.sign != 0u ? SF_SIGN : 0u) | SF_MAX_FINITE);
    }
    return (value.sign != 0u ? SF_SIGN : 0u) |
           ((uint32_t)(magnitude_exponent + 127) << 23u) |
           ((uint32_t)mantissa & SF_FRACTION);
}
static sf_ext sf_add_ext(sf_ext left, sf_ext right)
{
    sf_ext result;
    int difference;

    left.significand <<= 3u;
    left.exponent -= 3;
    right.significand <<= 3u;
    right.exponent -= 3;
    result.exponent = left.exponent;
    difference = left.exponent - right.exponent;
    if (difference > 0) {
        if (difference <= 24) {
            left.significand <<= (unsigned)difference;
            left.exponent = right.exponent;
            result.exponent = right.exponent;
        } else {
            right.significand = sf_shift_right_jam(
                right.significand, (unsigned)difference, &right.sticky);
        }
    } else if (difference < 0) {
        difference = -difference;
        if (difference <= 24) {
            right.significand <<= (unsigned)difference;
            right.exponent = left.exponent;
        } else {
            left.significand = sf_shift_right_jam(
                left.significand, (unsigned)difference, &left.sticky);
            result.exponent = right.exponent;
        }
    }
    result.sticky = left.sticky | right.sticky;
    if (left.sign == right.sign) {
        result.sign = left.sign;
        result.significand = left.significand + right.significand;
        return result;
    }
    if (left.significand > right.significand) {
        result.sign = left.sign;
        result.significand = left.significand - right.significand;
        return result;
    }
    if (right.significand > left.significand) {
        result.sign = right.sign;
        result.significand = right.significand - left.significand;
        return result;
    }
    if (left.sticky != right.sticky) {
        result.sign = left.sticky != 0u ? left.sign : right.sign;
        result.significand = 1u;
        result.sticky = 1u;
        return result;
    }
    result.significand = 0u;
    result.sign = 0u;
    return result;
}
static uint32_t sf_add_values(uint32_t *fpscr, uint32_t left_bits,
                              uint32_t right_bits, int subtract)
{
    sf_value left = sf_unpack(left_bits, fpscr);
    sf_value right = sf_unpack(right_bits, fpscr);
    sf_ext left_ext;
    sf_ext right_ext;
    sf_ext sum;
    uint32_t result;
    if (sf_process_nans2(left, right, &result, fpscr)) return result;
    if (subtract != 0) right.sign ^= 1u;
    if (left.kind == SF_INFINITY && right.kind == SF_INFINITY) {
        if (left.sign != right.sign) {
            sf_set_flag(fpscr, ARMV7M_FPSCR_IOC);
            return sf_default_nan();
        }
        return sf_infinity(left.sign);
    }
    if (left.kind == SF_INFINITY) return sf_infinity(left.sign);
    if (right.kind == SF_INFINITY) return sf_infinity(right.sign);
    if (left.kind == SF_ZERO && right.kind == SF_ZERO) {
        if (left.sign == right.sign) return sf_zero(left.sign);
        return sf_zero(sf_rounding_mode(*fpscr) == 2u ? 1u : 0u);
    }
    if (left.kind == SF_ZERO) {
        right_ext.significand = (uint64_t)right.significand << 3u;
        right_ext.exponent = right.exponent - 3;
        right_ext.sign = right.sign;
        right_ext.sticky = 0u;
        return sf_round_ext(fpscr, right_ext);
    }
    if (right.kind == SF_ZERO) {
        left_ext.significand = (uint64_t)left.significand << 3u;
        left_ext.exponent = left.exponent - 3;
        left_ext.sign = left.sign;
        left_ext.sticky = 0u;
        return sf_round_ext(fpscr, left_ext);
    }
    left_ext.significand = left.significand;
    left_ext.exponent = left.exponent;
    left_ext.sign = left.sign;
    left_ext.sticky = 0u;
    right_ext.significand = right.significand;
    right_ext.exponent = right.exponent;
    right_ext.sign = right.sign;
    right_ext.sticky = 0u;
    sum = sf_add_ext(left_ext, right_ext);
    if (sum.significand == 0u)
        return sf_zero(sf_rounding_mode(*fpscr) == 2u ? 1u : 0u);
    return sf_round_ext(fpscr, sum);
}
static uint32_t sf_mul(uint32_t *fpscr, uint32_t left_bits,
                       uint32_t right_bits)
{
    sf_value left = sf_unpack(left_bits, fpscr);
    sf_value right = sf_unpack(right_bits, fpscr);
    sf_ext product;
    uint32_t result;
    if (sf_process_nans2(left, right, &result, fpscr)) return result;
    if ((left.kind == SF_INFINITY && right.kind == SF_ZERO) ||
        (left.kind == SF_ZERO && right.kind == SF_INFINITY)) {
        sf_set_flag(fpscr, ARMV7M_FPSCR_IOC);
        return sf_default_nan();
    }
    if (left.kind == SF_INFINITY || right.kind == SF_INFINITY)
        return sf_infinity(left.sign ^ right.sign);
    if (left.kind == SF_ZERO || right.kind == SF_ZERO)
        return sf_zero(left.sign ^ right.sign);
    product.significand = (uint64_t)left.significand * right.significand;
    product.exponent = left.exponent + right.exponent;
    product.sign = left.sign ^ right.sign;
    product.sticky = 0u;
    return sf_round_ext(fpscr, product);
}
static uint32_t sf_div(uint32_t *fpscr, uint32_t left_bits,
                       uint32_t right_bits)
{
    sf_value left = sf_unpack(left_bits, fpscr);
    sf_value right = sf_unpack(right_bits, fpscr);
    sf_ext quotient_value;
    uint64_t numerator;
    uint64_t quotient;
    uint64_t remainder;
    uint32_t result;
    if (sf_process_nans2(left, right, &result, fpscr)) return result;
    if ((left.kind == SF_INFINITY && right.kind == SF_INFINITY) ||
        (left.kind == SF_ZERO && right.kind == SF_ZERO)) {
        sf_set_flag(fpscr, ARMV7M_FPSCR_IOC);
        return sf_default_nan();
    }
    if (left.kind == SF_INFINITY || right.kind == SF_ZERO) {
        if (left.kind != SF_INFINITY)
            sf_set_flag(fpscr, ARMV7M_FPSCR_DZC);
        return sf_infinity(left.sign ^ right.sign);
    }
    if (left.kind == SF_ZERO || right.kind == SF_INFINITY)
        return sf_zero(left.sign ^ right.sign);
    numerator = (uint64_t)left.significand << 40u;
    quotient = numerator / right.significand;
    remainder = numerator % right.significand;
    quotient_value.significand = quotient;
    quotient_value.exponent = left.exponent - right.exponent - 40;
    quotient_value.sign = left.sign ^ right.sign;
    quotient_value.sticky = remainder != 0u ? 1u : 0u;
    return sf_round_ext(fpscr, quotient_value);
}
static uint64_t sf_isqrt(uint64_t value, uint64_t *remainder)
{
    uint64_t root = 0u, bit = UINT64_C(1) << 62u;
    while (bit > value) bit >>= 2u;
    while (bit != 0u) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1u) + bit;
        } else {
            root >>= 1u;
        }
        bit >>= 2u;
    }
    *remainder = value; return root;
}
static uint32_t sf_sqrt(uint32_t *fpscr, uint32_t bits)
{
    sf_value value = sf_unpack(bits, fpscr);
    sf_ext result;
    uint32_t significand;
    uint64_t radicand;
    uint64_t root;
    uint64_t remainder;
    int exponent;
    if (value.kind == SF_QNAN || value.kind == SF_SNAN)
        return sf_nan_value(value, fpscr);
    if (value.kind == SF_INFINITY)
        return value.sign == 0u ? SF_INFINITY_BITS :
               (sf_set_flag(fpscr, ARMV7M_FPSCR_IOC), sf_default_nan());
    if (value.kind == SF_ZERO) return sf_zero(value.sign);
    if (value.sign != 0u) {
        sf_set_flag(fpscr, ARMV7M_FPSCR_IOC);
        return sf_default_nan();
    }
    significand = value.significand;
    exponent = value.exponent;
    while ((significand & SF_HIDDEN) == 0u) {
        significand <<= 1u;
        --exponent;
    }
    if (exponent % 2 != 0) {
        significand <<= 1u;
        --exponent;
    }
    radicand = (uint64_t)significand << 38u;
    root = sf_isqrt(radicand, &remainder);
    result.significand = root;
    result.exponent = exponent / 2 - 19;
    result.sign = 0u;
    result.sticky = remainder != 0u ? 1u : 0u;
    return sf_round_ext(fpscr, result);
}
static semu_fpu_eval sf_eval(uint32_t bits, uint32_t fpscr)
{ semu_fpu_eval result = {bits, fpscr}; return result; }
uint32_t semu_fpu_abs_bits(uint32_t operand) { return operand & ~SF_SIGN; }
uint32_t semu_fpu_neg_bits(uint32_t operand) { return operand ^ SF_SIGN; }
semu_fpu_eval semu_fpu_add_bits(uint32_t left, uint32_t right, uint32_t fpscr)
{ return sf_eval(sf_add_values(&fpscr, left, right, 0), fpscr); }
semu_fpu_eval semu_fpu_sub_bits(uint32_t left, uint32_t right, uint32_t fpscr)
{ return sf_eval(sf_add_values(&fpscr, left, right, 1), fpscr); }
semu_fpu_eval semu_fpu_mul_bits(uint32_t left, uint32_t right, uint32_t fpscr)
{ return sf_eval(sf_mul(&fpscr, left, right), fpscr); }
semu_fpu_eval semu_fpu_div_bits(uint32_t left, uint32_t right, uint32_t fpscr)
{ return sf_eval(sf_div(&fpscr, left, right), fpscr); }
semu_fpu_eval semu_fpu_mul_add_bits(uint32_t addend, uint32_t left,
                                    uint32_t right, uint32_t fpscr)
{
    semu_fpu_eval product = semu_fpu_mul_bits(left, right, fpscr);

    return semu_fpu_add_bits(addend, product.bits, product.fpscr);
}
semu_fpu_eval semu_fpu_sqrt_bits(uint32_t operand, uint32_t fpscr)
{ return sf_eval(sf_sqrt(&fpscr, operand), fpscr); }
