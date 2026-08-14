#ifndef SEMU_ARMV7M_FPU_SOFTFLOAT_INTERNAL_H
#define SEMU_ARMV7M_FPU_SOFTFLOAT_INTERNAL_H

#include <stdint.h>

#include "fpu_softfloat.h"

#define SF_SIGN UINT32_C(0x80000000)
#define SF_EXPONENT UINT32_C(0x7f800000)
#define SF_FRACTION UINT32_C(0x007fffff)
#define SF_HIDDEN UINT32_C(0x00800000)
#define SF_QNAN_BIT UINT32_C(0x00400000)
#define SF_INFINITY_BITS SF_EXPONENT
#define SF_MAX_FINITE UINT32_C(0x7f7fffff)

typedef enum sf_kind {
    SF_ZERO,
    SF_FINITE,
    SF_INFINITY,
    SF_QNAN,
    SF_SNAN
} sf_kind;

typedef struct sf_value {
    sf_kind kind;
    unsigned sign;
    int exponent;
    uint32_t significand;
    uint32_t raw;
} sf_value;

typedef struct sf_ext {
    uint64_t significand;
    int exponent;
    unsigned sign;
    unsigned sticky;
} sf_ext;

unsigned sf_rounding_mode(uint32_t fpscr);
sf_value sf_unpack(uint32_t bits, uint32_t *fpscr);
uint32_t sf_round_ext(uint32_t *fpscr, sf_ext value);

#endif
