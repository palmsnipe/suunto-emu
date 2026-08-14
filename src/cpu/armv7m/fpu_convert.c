#include "armv7m_internal.h"
#include "fpu_softfloat.h"

typedef enum fpu_convert_op {
    FPU_CONVERT_COMPARE,
    FPU_CONVERT_INTEGER,
    FPU_CONVERT_FIXED,
    FPU_CONVERT_IMMEDIATE,
    FPU_CONVERT_REGISTER
} fpu_convert_op;

typedef struct fpu_convert_decoded {
    fpu_convert_op operation;
    unsigned d;
    unsigned m;
    unsigned is_unsigned;
    unsigned round_zero;
    unsigned to_fixed;
    unsigned size;
    unsigned fraction_bits;
    unsigned quiet_nan_exception;
    unsigned with_zero;
    uint8_t immediate;
} fpu_convert_decoded;

static semu_status refuse(semu_cpu *cpu, uint16_t first, uint16_t second,
                          semu_error *error)
{
    return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second, error);
}

static unsigned fp_register(uint16_t first, uint16_t second, int destination)
{
    if (destination != 0)
        return ((unsigned)(second >> 12u) & 15u) * 2u +
               ((unsigned)first >> 6u & 1u);
    return ((unsigned)second & 15u) * 2u +
           ((unsigned)second >> 5u & 1u);
}

static int decode_compare(uint16_t first, uint16_t second,
                          fpu_convert_decoded *decoded)
{
    uint16_t base = first & UINT16_C(0xffbf);
    uint16_t fixed;
    int with_zero;

    if (base != UINT16_C(0xeeb4) && base != UINT16_C(0xeeb5)) return 0;
    if ((second & UINT16_C(0x0100)) != 0u) return -1;
    fixed = second & UINT16_C(0x0ed0);
    if (fixed != UINT16_C(0x0a40) && fixed != UINT16_C(0x0ac0)) {
        if ((second & UINT16_C(0x0ef0)) == UINT16_C(0x0a00)) return 0;
        return -1;
    }
    with_zero = base == UINT16_C(0xeeb5);
    if ((second & UINT16_C(0x0010)) != 0u ||
        (with_zero && (second & UINT16_C(0x003f)) != 0u)) return -1;
    decoded->operation = FPU_CONVERT_COMPARE;
    decoded->d = fp_register(first, second, 1);
    decoded->m = fp_register(first, second, 0);
    decoded->quiet_nan_exception = (second >> 7u) & 1u;
    decoded->with_zero = with_zero ? 1u : 0u;
    return 1;
}

static int decode_immediate_or_register(uint16_t first, uint16_t second,
                                        fpu_convert_decoded *decoded)
{
    uint16_t base = first & UINT16_C(0xffb0);

    if (base != UINT16_C(0xeeb0)) return 0;
    if ((second & UINT16_C(0x0ef0)) == UINT16_C(0x0a00)) {
        if ((second & UINT16_C(0x0100)) != 0u) return -1;
        decoded->operation = FPU_CONVERT_IMMEDIATE;
        decoded->d = fp_register(first, second, 1);
        decoded->immediate = (uint8_t)(((first & 15u) << 4u) |
                                       (second & 15u));
        return 1;
    }
    if ((second & UINT16_C(0x0100)) != 0u ||
        (second & UINT16_C(0x0ed0)) != UINT16_C(0x0a40) ||
        (second & UINT16_C(0x0010)) != 0u ||
        (first & UINT16_C(0x000f)) != 0u)
        return 0;
    decoded->operation = FPU_CONVERT_REGISTER;
    decoded->d = fp_register(first, second, 1);
    decoded->m = fp_register(first, second, 0);
    return 1;
}

static int decode_integer(uint16_t first, uint16_t second,
                          fpu_convert_decoded *decoded)
{
    uint16_t base = first & UINT16_C(0xffbf);
    uint16_t fixed;

    if (base != UINT16_C(0xeeb8) && base != UINT16_C(0xeebc) &&
        base != UINT16_C(0xeebd)) return 0;
    if ((second & UINT16_C(0x0100)) != 0u) return -1;
    fixed = second & UINT16_C(0x0ed0);
    if (fixed != UINT16_C(0x0a40) && fixed != UINT16_C(0x0ac0))
        return -1;
    if ((second & UINT16_C(0x0010)) != 0u) return -1;
    decoded->operation = FPU_CONVERT_INTEGER;
    decoded->d = fp_register(first, second, 1);
    decoded->m = fp_register(first, second, 0);
    if (base == UINT16_C(0xeeb8)) {
        decoded->is_unsigned = (second >> 7u & 1u) == 0u ? 1u : 0u;
        decoded->round_zero = 0u;
        decoded->to_fixed = 0u;
    } else {
        decoded->is_unsigned = base == UINT16_C(0xeebc) ? 1u : 0u;
        decoded->round_zero = (second >> 7u) & 1u;
        decoded->to_fixed = 1u;
    }
    return 1;
}

static int decode_fixed(uint16_t first, uint16_t second,
                        fpu_convert_decoded *decoded)
{
    uint16_t base = first & UINT16_C(0xffbf);
    unsigned encoded_fraction;
    unsigned size;

    if (base != UINT16_C(0xeeba) && base != UINT16_C(0xeebb) &&
        base != UINT16_C(0xeebe) && base != UINT16_C(0xeebf)) return 0;
    if ((second & UINT16_C(0x0100)) != 0u ||
        (second & UINT16_C(0x0e50)) != UINT16_C(0x0a40)) return -1;
    encoded_fraction = ((unsigned)second & 15u) * 2u +
                       ((unsigned)second >> 5u & 1u);
    size = ((unsigned)second >> 7u & 1u) != 0u ? 32u : 16u;
    if ((size == 16u && encoded_fraction > 16u) ||
        (size == 32u && encoded_fraction > 31u)) return -1;
    decoded->operation = FPU_CONVERT_FIXED;
    decoded->d = fp_register(first, second, 1);
    decoded->is_unsigned = (unsigned)first & 1u;
    decoded->to_fixed = ((unsigned)first >> 2u) & 1u;
    decoded->size = size;
    decoded->fraction_bits = size - encoded_fraction;
    return 1;
}

static int decode_conversion(uint16_t first, uint16_t second,
                             fpu_convert_decoded *decoded)
{
    int result;

    result = decode_compare(first, second, decoded);
    if (result != 0) return result;
    result = decode_immediate_or_register(first, second, decoded);
    if (result != 0) return result;
    result = decode_integer(first, second, decoded);
    if (result != 0) return result;
    return decode_fixed(first, second, decoded);
}

static semu_status execute_conversion(semu_cpu *cpu,
                                      const fpu_convert_decoded *decoded,
                                      uint16_t first, uint16_t second,
                                      semu_error *error)
{
    uint32_t fpscr = cpu->state.fpscr;
    uint32_t result;

    switch (decoded->operation) {
    case FPU_CONVERT_COMPARE:
        result = semu_fpu_compare_flags(
            cpu->state.s[decoded->d],
            decoded->with_zero != 0u ? 0u : cpu->state.s[decoded->m],
            decoded->quiet_nan_exception, &fpscr);
        cpu->state.fpscr = (fpscr & UINT32_C(0x0fffffff)) | result;
        return SEMU_OK;
    case FPU_CONVERT_INTEGER:
        if (decoded->to_fixed != 0u)
            result = semu_fpu_to_int_bits(cpu->state.s[decoded->m],
                                          decoded->is_unsigned,
                                          decoded->round_zero, &fpscr);
        else
            result = semu_fpu_from_int_bits(cpu->state.s[decoded->m],
                                            decoded->is_unsigned, &fpscr);
        cpu->state.s[decoded->d] = result;
        cpu->state.fpscr = fpscr;
        return SEMU_OK;
    case FPU_CONVERT_FIXED:
        if (decoded->to_fixed != 0u) {
            result = semu_fpu_to_fixed_bits(
                cpu->state.s[decoded->d], decoded->size,
                decoded->fraction_bits, decoded->is_unsigned, &fpscr);
            if (decoded->is_unsigned == 0u && decoded->size < 32u &&
                (result & (UINT32_C(1) << (decoded->size - 1u))) != 0u)
                result |= UINT32_MAX << decoded->size;
        } else {
            result = semu_fpu_from_fixed_bits(
                cpu->state.s[decoded->d], decoded->size,
                decoded->fraction_bits, decoded->is_unsigned, &fpscr);
        }
        cpu->state.s[decoded->d] = result;
        cpu->state.fpscr = fpscr;
        return SEMU_OK;
    case FPU_CONVERT_IMMEDIATE:
        cpu->state.s[decoded->d] = semu_fpu_expand_imm8(decoded->immediate);
        return SEMU_OK;
    case FPU_CONVERT_REGISTER:
        cpu->state.s[decoded->d] = cpu->state.s[decoded->m];
        return SEMU_OK;
    }
    return refuse(cpu, first, second, error);
}

semu_status armv7m_fpu_convert(semu_cpu *cpu, uint16_t first,
                               uint16_t second, semu_error *error)
{
    fpu_convert_decoded decoded;
    int decoded_result = decode_conversion(first, second, &decoded);
    uint32_t prior_ipsr;
    semu_status status;

    if (decoded_result == 0) return SEMU_ERR_UNSUPPORTED;
    if (decoded_result < 0) return refuse(cpu, first, second, error);
    prior_ipsr = cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK;
    status = armv7m_fpu_check_access(cpu, error);
    if (status != SEMU_OK) return status;
    if ((cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK) != prior_ipsr)
        return SEMU_OK;
    return execute_conversion(cpu, &decoded, first, second, error);
}
