/*
 * Pure TSC6A (texture format 0x17) block expansion (ticket 793).
 *
 * One 96-bit block (12 bytes per 4x4 pixels) expands to sixteen RGBA8888
 * texels.  No bus, no state, no allocation: the caller owns the bytes.
 *
 * Field law and rounding are exactly the twice-reproduced reference law of
 * E-RE-SAP235-TSC6A-001 (DERIVATION.md section 2, reference decoder.py).
 * Bit numbering is little-endian: bit0 is the LSB of byte0.
 *
 *   bits  0..31  sixteen 2-bit color indices; pixel p = 4*r + c (raster,
 *                row 0 first); the index of pixel p occupies bits 2p..2p+1.
 *   bits 32..47  endpoint E0, RGBA4444-packed: R=bits15:12, G=11:8, B=7:4.
 *   bits 48..63  endpoint E1, same packing.  Nibble 0 (A) is ignored.
 *                A 4-bit channel c expands to 8-bit c*17.
 *   bits 64..74  11-bit alpha, constant for the whole block; the 8-bit
 *                alpha is a11 * 255 / 2047 (integer floor).
 *   bits 75..95  auxiliary region with UNVERIFIED semantics: if any of
 *                these 21 bits is nonzero the block is refused (return 0)
 *                and the output is left untouched.  No partial writes.
 *
 * Four-point color table (patent US9,640,149 B2 four-point mode):
 *   idx0 = E0, idx1 = (2*E0+E1)/3, idx2 = (E0+2*E1)/3, idx3 = E1,
 * computed per channel with integer arithmetic.  The reference decoder
 * rounds each weighted sum to the nearest integer, i.e. idx1 is
 * floor((2*a + b + 1) / 3) and idx2 is floor((a + 2*b + 1) / 3); the +1
 * rounding bias is part of the pinned law and the golden window
 * decode-1.bin matches only with it (documented here, not guessed).
 * All arithmetic is on unsigned fixed-width values with small bounds
 * (channels <= 255), so no overflow is possible.
 */

#include "nema_tsc6a_internal.h"

#define TSC6A_BLOCK_BYTES 12u
#define TSC6A_BLOCK_PIXELS 16u

/* Extract a little-endian bitfield: bit i of the block is bit (i & 7) of
 * byte (i >> 3).  width <= 16 here, so a uint32_t accumulator is exact. */
static uint32_t tsc6a_bits(const uint8_t *blk, unsigned start, unsigned width)
{
    uint32_t value = 0u;
    unsigned i;
    for (i = 0u; i < width; ++i) {
        unsigned bit = start + i;
        uint32_t bit_value = (uint32_t)((blk[bit >> 3u] >> (bit & 7u)) & 1u);
        value |= bit_value << i;
    }
    return value;
}

/* RGBA4444 endpoint (top three nibbles) expanded to 8-bit R,G,B. */
static void tsc6a_endpoint(uint32_t packed, uint32_t rgb[3])
{
    rgb[0] = ((packed >> 12u) & 15u) * 17u;
    rgb[1] = ((packed >> 8u) & 15u) * 17u;
    rgb[2] = ((packed >> 4u) & 15u) * 17u;
}

/* Weighted third step: (weight_a*a + weight_b*b + 1) / 3 per channel,
 * matching the reference floor((2a+b+1)/3) / floor((a+2b+1)/3) exactly. */
static void tsc6a_third(const uint32_t a[3], const uint32_t b[3],
                        uint32_t weight_a, uint32_t out[3])
{
    uint32_t weight_b = 3u - weight_a;
    unsigned i;
    for (i = 0u; i < 3u; ++i) {
        /* Max value 2*255 + 255 + 1 = 766: no overflow. */
        out[i] = (weight_a * a[i] + weight_b * b[i] + 1u) / 3u;
    }
}

int tsc6a_block_supported(const uint8_t blk[TSC6A_BLOCK_BYTES])
{
    return blk != NULL && (blk[9] & 0xf8u) == 0u &&
           blk[10] == 0u && blk[11] == 0u;
}

int tsc6a_expand_block(const uint8_t blk[TSC6A_BLOCK_BYTES],
                       uint8_t out[TSC6A_BLOCK_PIXELS][4])
{
    uint32_t e0[3], e1[3], mid1[3], mid2[3];
    const uint32_t *table[4];
    uint32_t alpha;
    unsigned p;

    if (blk == NULL || out == NULL) {
        return 0;
    }
    /* Fail closed on the entire unverified auxiliary region bits 75..95. */
    if (!tsc6a_block_supported(blk)) {
        return 0;
    }
    tsc6a_endpoint(tsc6a_bits(blk, 32u, 16u), e0);
    tsc6a_endpoint(tsc6a_bits(blk, 48u, 16u), e1);
    tsc6a_third(e0, e1, 2u, mid1);
    tsc6a_third(e0, e1, 1u, mid2);
    table[0] = e0;
    table[1] = mid1;
    table[2] = mid2;
    table[3] = e1;
    /* 11-bit block alpha, constant across the block (bits 64..74). */
    alpha = (tsc6a_bits(blk, 64u, 11u) * 255u) / 2047u;

    for (p = 0u; p < TSC6A_BLOCK_PIXELS; ++p) {
        const uint32_t *color = table[tsc6a_bits(blk, 2u * p, 2u)];
        out[p][0] = (uint8_t)color[0];
        out[p][1] = (uint8_t)color[1];
        out[p][2] = (uint8_t)color[2];
        out[p][3] = (uint8_t)alpha;
    }
    return 1;
}
