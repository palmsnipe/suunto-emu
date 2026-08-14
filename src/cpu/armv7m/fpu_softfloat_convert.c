#include "fpu_softfloat_internal.h"

typedef struct sf_magnitude {
    uint64_t whole;
    unsigned inexact;
    unsigned greater_half;
    unsigned tie;
    unsigned too_large;
} sf_magnitude;

static unsigned top_bit32(uint32_t value)
{
    unsigned bit = 0u;

    while (value > 1u) {
        value >>= 1u;
        ++bit;
    }
    return bit;
}

static int compare_magnitude(sf_value left, sf_value right)
{
    int left_top;
    int right_top;
    int left_exponent;
    int right_exponent;
    uint32_t left_significand;
    uint32_t right_significand;

    if (left.kind == SF_INFINITY)
        return right.kind == SF_INFINITY ? 0 : 1;
    if (right.kind == SF_INFINITY) return -1;
    if (left.kind == SF_ZERO)
        return right.kind == SF_ZERO ? 0 : -1;
    if (right.kind == SF_ZERO) return 1;
    left_top = (int)top_bit32(left.significand);
    right_top = (int)top_bit32(right.significand);
    left_exponent = left.exponent + left_top;
    right_exponent = right.exponent + right_top;
    if (left_exponent != right_exponent)
        return left_exponent < right_exponent ? -1 : 1;
    left_significand = left.significand << (23 - left_top);
    right_significand = right.significand << (23 - right_top);
    if (left_significand == right_significand) return 0;
    return left_significand < right_significand ? -1 : 1;
}

uint32_t semu_fpu_compare_flags(uint32_t left_bits, uint32_t right_bits,
                                unsigned quiet_nan_exception,
                                uint32_t *fpscr)
{
    sf_value left = sf_unpack(left_bits, fpscr);
    sf_value right = sf_unpack(right_bits, fpscr);
    int magnitude;

    if (left.kind == SF_QNAN || left.kind == SF_SNAN ||
        right.kind == SF_QNAN || right.kind == SF_SNAN) {
        if (left.kind == SF_SNAN || right.kind == SF_SNAN ||
            quiet_nan_exception != 0u)
            *fpscr |= ARMV7M_FPSCR_IOC;
        return UINT32_C(0x30000000);
    }
    if (left.kind == SF_ZERO && right.kind == SF_ZERO)
        return UINT32_C(0x60000000);
    if (left.sign != right.sign)
        return left.sign != 0u ? UINT32_C(0x80000000) : UINT32_C(0x20000000);
    magnitude = compare_magnitude(left, right);
    if (magnitude == 0) return UINT32_C(0x60000000);
    if (left.sign != 0u) magnitude = -magnitude;
    return magnitude < 0 ? UINT32_C(0x80000000) : UINT32_C(0x20000000);
}

static sf_magnitude scaled_magnitude(sf_value value, unsigned fraction_bits)
{
    sf_magnitude result = {0u, 0u, 0u, 0u, 0u};
    int scale = value.exponent + (int)fraction_bits;
    unsigned shift;
    uint64_t remainder;

    if (scale >= 0) {
        unsigned top = top_bit32(value.significand);
        if (scale > 63 - (int)top) {
            result.too_large = 1u;
            return result;
        }
        result.whole = (uint64_t)value.significand << (unsigned)scale;
        return result;
    }
    shift = (unsigned)(-scale);
    if (shift >= 64u) {
        result.inexact = value.significand != 0u ? 1u : 0u;
        return result;
    }
    result.whole = (uint64_t)value.significand >> shift;
    remainder = (uint64_t)value.significand &
                (shift == 64u ? UINT64_MAX :
                 ((UINT64_C(1) << shift) - UINT64_C(1)));
    result.inexact = remainder != 0u ? 1u : 0u;
    if (shift != 0u) {
        uint64_t halfway = UINT64_C(1) << (shift - 1u);
        result.greater_half = remainder > halfway ? 1u : 0u;
        result.tie = remainder == halfway ? 1u : 0u;
    }
    return result;
}

static uint64_t rounded_magnitude(sf_magnitude value, unsigned sign,
                                  unsigned mode, unsigned *inexact)
{
    unsigned round_up = 0u;

    *inexact = value.inexact;
    if (value.inexact != 0u) {
        if (mode == 1u) round_up = sign == 0u ? 1u : 0u;
        else if (mode == 2u) round_up = sign != 0u ? 1u : 0u;
        else if (mode == 0u)
            round_up = value.greater_half != 0u ||
                       (value.tie != 0u && (value.whole & 1u) != 0u);
    }
    if (round_up != 0u) ++value.whole;
    return value.whole;
}

static uint32_t integer_result(sf_value value, unsigned size,
                               unsigned is_unsigned, unsigned round_zero,
                               uint32_t *fpscr)
{
    sf_magnitude magnitude;
    uint64_t rounded;
    uint64_t positive_limit;
    uint64_t negative_limit;
    unsigned inexact;
    unsigned mode;
    unsigned negative;
    uint32_t mask;

    if (size == 32u) mask = UINT32_MAX;
    else mask = (UINT32_C(1) << size) - UINT32_C(1);
    if (value.kind == SF_QNAN || value.kind == SF_SNAN) {
        *fpscr |= ARMV7M_FPSCR_IOC;
        return 0u;
    }
    if (value.kind == SF_INFINITY) {
        *fpscr |= ARMV7M_FPSCR_IOC;
        if (is_unsigned != 0u)
            return value.sign != 0u ? 0u : mask;
        return value.sign != 0u ? (UINT32_C(1) << (size - 1u)) :
               (UINT32_C(1) << (size - 1u)) - UINT32_C(1);
    }
    if (value.kind == SF_ZERO) return 0u;
    magnitude = scaled_magnitude(value, 0u);
    mode = round_zero != 0u ? 3u : sf_rounding_mode(*fpscr);
    if (magnitude.too_large != 0u) {
        *fpscr |= ARMV7M_FPSCR_IOC;
        if (is_unsigned != 0u)
            return value.sign != 0u ? 0u : mask;
        return value.sign != 0u ? (UINT32_C(1) << (size - 1u)) :
               (UINT32_C(1) << (size - 1u)) - UINT32_C(1);
    }
    rounded = rounded_magnitude(magnitude, value.sign, mode, &inexact);
    positive_limit = is_unsigned != 0u ? mask :
                     (UINT64_C(1) << (size - 1u)) - UINT64_C(1);
    negative_limit = is_unsigned != 0u ? 0u :
                     UINT64_C(1) << (size - 1u);
    negative = value.sign != 0u && rounded != 0u;
    if ((negative != 0u && is_unsigned != 0u) ||
        (negative == 0u && rounded > positive_limit) ||
        (negative != 0u && rounded > negative_limit)) {
        *fpscr |= ARMV7M_FPSCR_IOC;
        if (is_unsigned != 0u)
            return negative != 0u ? 0u : mask;
        return negative != 0u ? (uint32_t)negative_limit :
               (uint32_t)positive_limit;
    }
    if (inexact != 0u) *fpscr |= ARMV7M_FPSCR_IXC;
    if (negative != 0u) return (0u - (uint32_t)rounded) & mask;
    return (uint32_t)rounded & mask;
}

uint32_t semu_fpu_to_int_bits(uint32_t operand, unsigned is_unsigned,
                              unsigned round_zero, uint32_t *fpscr)
{
    return integer_result(sf_unpack(operand, fpscr), 32u, is_unsigned,
                          round_zero, fpscr);
}

uint32_t semu_fpu_to_fixed_bits(uint32_t operand, unsigned size,
                                unsigned fraction_bits, unsigned is_unsigned,
                                uint32_t *fpscr)
{
    sf_value value = sf_unpack(operand, fpscr);
    sf_magnitude magnitude;
    uint64_t rounded;
    uint64_t positive_limit;
    uint64_t negative_limit;
    unsigned inexact;
    unsigned negative;
    uint32_t mask;

    if (size != 16u && size != 32u) {
        *fpscr |= ARMV7M_FPSCR_IOC;
        return 0u;
    }
    if (value.kind == SF_QNAN || value.kind == SF_SNAN ||
        value.kind == SF_INFINITY)
        return integer_result(value, size, is_unsigned, 1u, fpscr);
    if (value.kind == SF_ZERO) return 0u;
    magnitude = scaled_magnitude(value, fraction_bits);
    mask = size == 32u ? UINT32_MAX : (UINT32_C(1) << size) - 1u;
    if (magnitude.too_large != 0u) {
        *fpscr |= ARMV7M_FPSCR_IOC;
        return value.sign != 0u ? (is_unsigned != 0u ? 0u :
               (UINT32_C(1) << (size - 1u))) :
               (is_unsigned != 0u ? mask :
               (UINT32_C(1) << (size - 1u)) - 1u);
    }
    rounded = rounded_magnitude(magnitude, value.sign, 3u, &inexact);
    positive_limit = is_unsigned != 0u ? mask :
                     (UINT64_C(1) << (size - 1u)) - 1u;
    negative_limit = is_unsigned != 0u ? 0u : UINT64_C(1) << (size - 1u);
    negative = value.sign != 0u && rounded != 0u;
    if ((negative != 0u && is_unsigned != 0u) ||
        (negative == 0u && rounded > positive_limit) ||
        (negative != 0u && rounded > negative_limit)) {
        *fpscr |= ARMV7M_FPSCR_IOC;
        return negative != 0u ? (is_unsigned != 0u ? 0u :
               (uint32_t)negative_limit) : (uint32_t)positive_limit;
    }
    if (inexact != 0u) *fpscr |= ARMV7M_FPSCR_IXC;
    return negative != 0u ? (0u - (uint32_t)rounded) & mask :
           (uint32_t)rounded & mask;
}

static uint32_t fixed_magnitude(uint32_t operand, unsigned size,
                                unsigned is_unsigned, unsigned *negative)
{
    uint32_t mask = size == 32u ? UINT32_MAX : (UINT32_C(1) << size) - 1u;
    uint32_t value = operand & mask;

    *negative = is_unsigned == 0u &&
                ((value >> (size - 1u)) & 1u) != 0u ? 1u : 0u;
    if (*negative != 0u) value = (0u - value) & mask;
    return value;
}

static uint32_t from_fixed(uint32_t operand, unsigned size,
                           unsigned fraction_bits, unsigned is_unsigned,
                           unsigned round_nearest, uint32_t *fpscr)
{
    sf_ext value;
    uint32_t magnitude;
    unsigned negative;
    uint32_t local_fpscr = *fpscr;

    magnitude = fixed_magnitude(operand, size, is_unsigned, &negative);
    if (magnitude == 0u) return 0u;
    value.significand = magnitude;
    value.exponent = -(int)fraction_bits;
    value.sign = negative;
    value.sticky = 0u;
    if (round_nearest != 0u)
        local_fpscr = (local_fpscr & ~ARMV7M_FPSCR_RMODE_MASK) | 0u;
    magnitude = sf_round_ext(&local_fpscr, value);
    *fpscr = local_fpscr;
    return magnitude;
}

uint32_t semu_fpu_from_int_bits(uint32_t operand, unsigned is_unsigned,
                                uint32_t *fpscr)
{
    return from_fixed(operand, 32u, 0u, is_unsigned, 0u, fpscr);
}

uint32_t semu_fpu_from_fixed_bits(uint32_t operand, unsigned size,
                                  unsigned fraction_bits, unsigned is_unsigned,
                                  uint32_t *fpscr)
{
    return from_fixed(operand, size, fraction_bits, is_unsigned, 1u, fpscr);
}

uint32_t semu_fpu_expand_imm8(uint8_t immediate)
{
    unsigned sign = (unsigned)(immediate >> 7u) & 1u;
    unsigned bit = (unsigned)(immediate >> 6u) & 1u;
    uint32_t exponent = ((bit ^ 1u) << 7u) |
                       (bit != 0u ? UINT32_C(0x7c) : 0u) |
                       ((uint32_t)(immediate >> 4u) & 3u);

    return (sign << 31u) | (exponent << 23u) |
           ((uint32_t)(immediate & 0x0fu) << 19u);
}
