#ifndef SEMU_FPU_MATH_H
#define SEMU_FPU_MATH_H
#include <stdint.h>

/* Pure integer evaluation of IEEE binary32 bit patterns using ARM FPSCR
 * controls/status. No host floating point or CPU state. Pass FPSCR=0 for
 * round-to-nearest-even with gradual underflow; returned status is local.
 * Integer operands/results use uint32_t two's-complement bit patterns when
 * is_unsigned=0. Conversion status pointers must be non-null. round_zero=1
 * truncates float-to-integer; otherwise the supplied FPSCR rounding applies.
 * These are the existing interpreter evaluators, shared without new semantics.
 */
typedef struct semu_fpu_eval {
    uint32_t bits;
    uint32_t fpscr;
} semu_fpu_eval;
semu_fpu_eval semu_fpu_add_bits(uint32_t left, uint32_t right,
                                uint32_t fpscr);
semu_fpu_eval semu_fpu_sub_bits(uint32_t left, uint32_t right,
                                uint32_t fpscr);
semu_fpu_eval semu_fpu_mul_bits(uint32_t left, uint32_t right,
                                uint32_t fpscr);
uint32_t semu_fpu_to_int_bits(uint32_t operand, unsigned is_unsigned,
                              unsigned round_zero, uint32_t *fpscr);
uint32_t semu_fpu_from_int_bits(uint32_t operand, unsigned is_unsigned,
                                uint32_t *fpscr);
#endif
