#include "armv7m_internal.h"
#include "fpu_softfloat.h"

typedef enum fpu_arith_op {
    FPU_OP_ABS,
    FPU_OP_NEG,
    FPU_OP_SQRT,
    FPU_OP_ADD,
    FPU_OP_SUB,
    FPU_OP_MUL,
    FPU_OP_DIV,
    FPU_OP_MLA,
    FPU_OP_MLS,
    FPU_OP_NMLA,
    FPU_OP_NMLS,
    FPU_OP_NMUL
} fpu_arith_op;

typedef struct fpu_decoded {
    fpu_arith_op operation;
    unsigned d;
    unsigned n;
    unsigned m;
} fpu_decoded;

static uint32_t packed_instruction(uint16_t first, uint16_t second)
{
    return ((uint32_t)first << 16u) | second;
}

static int decode_three_register(uint16_t first, uint16_t second,
                                 fpu_decoded *decoded)
{
    uint16_t first_base = first & UINT16_C(0xffb0);
    uint16_t fixed = second & UINT16_C(0x0e50);
    fpu_arith_op operation;

    switch (first_base) {
    case UINT16_C(0xee00):
        if ((second & UINT16_C(0x0010)) != 0u) return 0;
        operation = fixed == UINT16_C(0x0a00) ? FPU_OP_MLA : FPU_OP_MLS;
        break;
    case UINT16_C(0xee10):
        if ((second & UINT16_C(0x0010)) != 0u) return 0;
        operation = fixed == UINT16_C(0x0a40) ? FPU_OP_NMLA : FPU_OP_NMLS;
        break;
    case UINT16_C(0xee20):
        if ((second & UINT16_C(0x0010)) != 0u) return 0;
        operation = fixed == UINT16_C(0x0a00) ? FPU_OP_MUL : FPU_OP_NMUL;
        break;
    case UINT16_C(0xee30):
        if ((second & UINT16_C(0x0010)) != 0u) return 0;
        operation = fixed == UINT16_C(0x0a00) ? FPU_OP_ADD : FPU_OP_SUB;
        break;
    case UINT16_C(0xee80):
        if ((second & UINT16_C(0x0010)) != 0u) return 0;
        operation = FPU_OP_DIV;
        break;
    default:
        return 0;
    }
    if ((fixed != UINT16_C(0x0a00) && fixed != UINT16_C(0x0a40)) ||
        (first_base == UINT16_C(0xee80) && fixed != UINT16_C(0x0a00)))
        return -1;
    if ((first_base == UINT16_C(0xee00) && operation == FPU_OP_MLA) ||
        (first_base == UINT16_C(0xee10) && operation == FPU_OP_NMLS) ||
        (first_base == UINT16_C(0xee20) && operation == FPU_OP_MUL) ||
        (first_base == UINT16_C(0xee30) && operation == FPU_OP_ADD) ||
        (first_base == UINT16_C(0xee80) && operation == FPU_OP_DIV)) {
        if (fixed != UINT16_C(0x0a00)) return -1;
    } else if (fixed != UINT16_C(0x0a40)) {
        return -1;
    }
    decoded->operation = operation;
    decoded->d = ((unsigned)(second >> 12u) & 15u) * 2u +
                 ((unsigned)first >> 6u & 1u);
    decoded->n = ((unsigned)first & 15u) * 2u +
                 ((unsigned)second >> 7u & 1u);
    decoded->m = ((unsigned)second & 15u) * 2u +
                 ((unsigned)second >> 5u & 1u);
    if ((second & UINT16_C(0x0100)) != 0u) return -1;
    return 1;
}

static int decode_unary(uint16_t first, uint16_t second,
                        fpu_decoded *decoded)
{
    uint16_t first_base = first & UINT16_C(0xffbf);
    uint16_t fixed = second & UINT16_C(0x0ed0);

    if (first_base != UINT16_C(0xeeb0) &&
        first_base != UINT16_C(0xeeb1)) return 0;
    if (first_base == UINT16_C(0xeeb0) && fixed == UINT16_C(0x0ac0))
        decoded->operation = FPU_OP_ABS;
    else if (first_base == UINT16_C(0xeeb1) && fixed == UINT16_C(0x0a40))
        decoded->operation = FPU_OP_NEG;
    else if (first_base == UINT16_C(0xeeb1) && fixed == UINT16_C(0x0ac0))
        decoded->operation = FPU_OP_SQRT;
    else
        return 0;
    decoded->d = ((unsigned)(second >> 12u) & 15u) * 2u +
                 ((unsigned)first >> 6u & 1u);
    decoded->m = ((unsigned)second & 15u) * 2u +
                 ((unsigned)second >> 5u & 1u);
    if ((second & UINT16_C(0x0100)) != 0u) return -1;
    return 1;
}

static int decode_arithmetic(uint16_t first, uint16_t second,
                             fpu_decoded *decoded)
{
    int result = decode_three_register(first, second, decoded);

    if (result != 0) return result;
    return decode_unary(first, second, decoded);
}

static semu_fpu_eval execute_arithmetic(const semu_cpu_state *state,
                                        const fpu_decoded *decoded)
{
    uint32_t destination = 0u;
    uint32_t left;
    uint32_t right = 0u;
    semu_fpu_eval product;

    if (decoded->operation == FPU_OP_ABS ||
        decoded->operation == FPU_OP_NEG ||
        decoded->operation == FPU_OP_SQRT) {
        left = state->s[decoded->m];
    } else {
        destination = state->s[decoded->d];
        left = state->s[decoded->n];
        right = state->s[decoded->m];
    }
    switch (decoded->operation) {
    case FPU_OP_ABS:
        return (semu_fpu_eval){semu_fpu_abs_bits(left), state->fpscr};
    case FPU_OP_NEG:
        return (semu_fpu_eval){semu_fpu_neg_bits(left), state->fpscr};
    case FPU_OP_SQRT:
        return semu_fpu_sqrt_bits(left, state->fpscr);
    case FPU_OP_ADD:
        return semu_fpu_add_bits(left, right, state->fpscr);
    case FPU_OP_SUB:
        return semu_fpu_sub_bits(left, right, state->fpscr);
    case FPU_OP_MUL:
        return semu_fpu_mul_bits(left, right, state->fpscr);
    case FPU_OP_DIV:
        return semu_fpu_div_bits(left, right, state->fpscr);
    case FPU_OP_MLA:
        product = semu_fpu_mul_bits(left, right, state->fpscr);
        return semu_fpu_add_bits(destination, product.bits, product.fpscr);
    case FPU_OP_MLS:
        product = semu_fpu_mul_bits(left, right, state->fpscr);
        return semu_fpu_add_bits(destination, semu_fpu_neg_bits(product.bits),
                                 product.fpscr);
    case FPU_OP_NMLA:
        product = semu_fpu_mul_bits(left, right, state->fpscr);
        return semu_fpu_add_bits(semu_fpu_neg_bits(destination),
                                 semu_fpu_neg_bits(product.bits),
                                 product.fpscr);
    case FPU_OP_NMLS:
        product = semu_fpu_mul_bits(left, right, state->fpscr);
        return semu_fpu_add_bits(semu_fpu_neg_bits(destination), product.bits,
                                 product.fpscr);
    case FPU_OP_NMUL:
        product = semu_fpu_mul_bits(left, right, state->fpscr);
        product.bits = semu_fpu_neg_bits(product.bits);
        return product;
    }
    return (semu_fpu_eval){0u, state->fpscr};
}

semu_status armv7m_fpu_arith(semu_cpu *cpu, uint16_t first,
                             uint16_t second, semu_error *error)
{
    fpu_decoded decoded;
    semu_fpu_eval result;
    int decoded_result = decode_arithmetic(first, second, &decoded);
    semu_status status;

    if (decoded_result == 0) return SEMU_ERR_UNSUPPORTED;
    if (decoded_result < 0)
        return armv7m_unsupported(cpu, packed_instruction(first, second),
                                  error);
    status = armv7m_fpu_check_access(cpu, error);
    if (status != SEMU_OK) return status;
    result = execute_arithmetic(semu_cpu_get_state(cpu), &decoded);
    cpu->state.s[decoded.d] = result.bits;
    cpu->state.fpscr = result.fpscr;
    return SEMU_OK;
}
