#ifndef SEMU_ARMV7M_FPU_SOFTFLOAT_H
#define SEMU_ARMV7M_FPU_SOFTFLOAT_H

#include <stdint.h>

#include "semu/cpu.h"
#include "semu/fpu_math.h"

#define ARMV7M_FPSCR_DN (UINT32_C(1) << 25)
#define ARMV7M_FPSCR_FZ (UINT32_C(1) << 24)
#define ARMV7M_FPSCR_RMODE_MASK (UINT32_C(3) << 22)
#define ARMV7M_FPSCR_IDC (UINT32_C(1) << 7)
#define ARMV7M_FPSCR_IXC (UINT32_C(1) << 4)
#define ARMV7M_FPSCR_UFC (UINT32_C(1) << 3)
#define ARMV7M_FPSCR_OFC (UINT32_C(1) << 2)
#define ARMV7M_FPSCR_DZC (UINT32_C(1) << 1)
#define ARMV7M_FPSCR_IOC UINT32_C(1)

uint32_t semu_fpu_abs_bits(uint32_t operand);
uint32_t semu_fpu_neg_bits(uint32_t operand);

semu_fpu_eval semu_fpu_div_bits(uint32_t left, uint32_t right,
                                uint32_t fpscr);
semu_fpu_eval semu_fpu_mul_add_bits(uint32_t addend, uint32_t left,
                                    uint32_t right, uint32_t fpscr);
semu_fpu_eval semu_fpu_sqrt_bits(uint32_t operand, uint32_t fpscr);
uint32_t semu_fpu_compare_flags(uint32_t left, uint32_t right,
                                unsigned quiet_nan_exception,
                                uint32_t *fpscr);

uint32_t semu_fpu_to_fixed_bits(uint32_t operand, unsigned size,
                                unsigned fraction_bits, unsigned is_unsigned,
                                uint32_t *fpscr);
uint32_t semu_fpu_from_fixed_bits(uint32_t operand, unsigned size,
                                  unsigned fraction_bits, unsigned is_unsigned,
                                  uint32_t *fpscr);
uint32_t semu_fpu_expand_imm8(uint8_t immediate);

semu_status armv7m_fpu_arith(semu_cpu *cpu, uint16_t first,
                             uint16_t second, semu_error *error);

#endif
