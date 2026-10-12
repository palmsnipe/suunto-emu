/* Ticket 807 / E-RE-SAP235-TSC6A-AUX-001: the auxiliary region is a
 * 7-bit-per-channel exact color. Synthetic pixels only; the real 20x32
 * asset bytes stay external. */
#include "../../src/display/nema_tsc6a_internal.h"
#include "test.h"
#include <string.h>

/* A zero-aux block whose every texel is index 0 (E0). */
static void zero_aux_block(uint8_t *blk)
{
    memset(blk, 0, 12u);
    blk[4] = 0xffu; blk[5] = 0xffu; /* E0 white */
    blk[8] = 0xffu; blk[9] = 0x07u; /* alpha 0x7ff */
}

/* An exact-color block: 7-bit channels v0,v1,v2 with the pinned 7-bit
 * little-endian packing (byte9[7:3] | byte10 << 5 | byte11 << 13). */
static void exact_color_block(uint8_t *blk, unsigned v0, unsigned v1,
                              unsigned v2, unsigned alpha11)
{
    uint32_t v = (v0 & 0x7fu) | ((v1 & 0x7fu) << 7) | ((v2 & 0x7fu) << 14);
    memset(blk, 0, 12u);
    blk[9] = (uint8_t)((v & 0x1fu) << 3);
    blk[10] = (uint8_t)((v >> 5) & 0xffu);
    blk[11] = (uint8_t)((v >> 13) & 0xffu);
    blk[8] = (uint8_t)(alpha11 & 0xffu);
    blk[9] |= (uint8_t)((alpha11 >> 8) & 0x07u);
}

static void test_exact_color_expands(semu_test_context *c)
{
    uint8_t blk[12];
    uint8_t out[16][4];
    unsigned p;
    unsigned i;

    /* The pinned strip colors: (0,42,127) -> #0054ff at alpha 0x7ff. */
    exact_color_block(blk, 0u, 42u, 127u, 0x7ffu);
    SEMU_TEST_EQ_U64(c, 1u, (unsigned)tsc6a_expand_block(blk, out));
    for (p = 0u; p < 16u; ++p) {
        SEMU_TEST_EQ_U64(c, 0u, out[p][0]);
        SEMU_TEST_EQ_U64(c, 84u, out[p][1]);
        SEMU_TEST_EQ_U64(c, 255u, out[p][2]);
        SEMU_TEST_EQ_U64(c, 255u, out[p][3]);
    }
    /* Saturated white: (127,127,127) -> #ffffff, the corpus's most
     * common exact color (8806 pinned blocks). */
    exact_color_block(blk, 127u, 127u, 127u, 0x7ffu);
    SEMU_TEST_EQ_U64(c, 1u, (unsigned)tsc6a_expand_block(blk, out));
    for (p = 0u; p < 16u; ++p) {
        SEMU_TEST_EQ_U64(c, 255u, out[p][0]);
        SEMU_TEST_EQ_U64(c, 255u, out[p][1]);
        SEMU_TEST_EQ_U64(c, 255u, out[p][2]);
    }
    /* The icon's witnessed grays: (42,42,42) at alpha 677 -> 84/84. */
    exact_color_block(blk, 42u, 42u, 42u, 677u);
    SEMU_TEST_EQ_U64(c, 1u, (unsigned)tsc6a_expand_block(blk, out));
    for (p = 0u; p < 16u; ++p) {
        SEMU_TEST_EQ_U64(c, 84u, out[p][0]);
        SEMU_TEST_EQ_U64(c, 84u, out[p][1]);
        SEMU_TEST_EQ_U64(c, 84u, out[p][2]);
        SEMU_TEST_EQ_U64(c, 84u, out[p][3]);
    }
    /* Alpha scaling stays the pinned 11-bit law (a*255/2047). */
    exact_color_block(blk, 1u, 2u, 3u, 0u);
    SEMU_TEST_EQ_U64(c, 1u, (unsigned)tsc6a_expand_block(blk, out));
    for (p = 0u; p < 16u; ++p) SEMU_TEST_EQ_U64(c, 0u, out[p][3]);

    /* The zero-aux law is untouched: an exact-color-looking index plane
     * with a zero aux region still decodes through the ramp. */
    zero_aux_block(blk);
    SEMU_TEST_EQ_U64(c, 1u, (unsigned)tsc6a_expand_block(blk, out));
    for (p = 0u; p < 16u; ++p) {
        SEMU_TEST_EQ_U64(c, 255u, out[p][0]);
        SEMU_TEST_EQ_U64(c, 255u, out[p][1]);
        SEMU_TEST_EQ_U64(c, 255u, out[p][2]);
        SEMU_TEST_EQ_U64(c, 255u, out[p][3]);
    }

    /* Every one of the 21 bits is color data: sweep a single bit set. */
    for (i = 0u; i < 21u; ++i) {
        uint32_t v = UINT32_C(1) << i;
        memset(blk, 0, 12u);
        blk[9] = (uint8_t)((v & 0x1fu) << 3);
        blk[10] = (uint8_t)((v >> 5) & 0xffu);
        blk[11] = (uint8_t)((v >> 13) & 0xffu);
        blk[8] = 0xffu; blk[9] |= 0x03u;
        SEMU_TEST_EQ_U64(c, 1u, (unsigned)tsc6a_expand_block(blk, out));
    }
}

static void test_zero_aux_law_unchanged(semu_test_context *c)
{
    uint8_t blk[12];
    uint8_t out[16][4];
    unsigned p;

    /* The pinned four-point ramp: E0 black, E1 teal (4,10,11), the
     * reference (2*E0+E1+1)/3 rounding, 2-bit index plane intact. */
    memset(blk, 0, 12u);
    blk[6] = 0x8au; /* E1 R=8? packed: R nibble bits 15:12 of the LE word */
    blk[7] = 0x4bu; /* E1 = 0x4b8a: R=4,G=11,B=10 */
    blk[8] = 0xffu; blk[9] = 0x07u; /* alpha 0x7ff */
    for (p = 0u; p < 16u; ++p)
        blk[p >> 2] |= (uint8_t)((p % 4u) << ((p & 3u) * 2u));
    SEMU_TEST_EQ_U64(c, 1u, (unsigned)tsc6a_expand_block(blk, out));
    /* idx0 = E0 = (0,0,0); idx1 = (2*E0+E1+1)/3 with E1 = 0x4b8a
     * (R=4, G=11, B=8 nibbles): R (2*0+68+1)/3 = 23, G (187+1)/3 = 62,
     * B (136+1)/3 = 45. */
    SEMU_TEST_EQ_U64(c, 0u, out[0][0]);  SEMU_TEST_EQ_U64(c, 0u, out[0][1]);
    SEMU_TEST_EQ_U64(c, 0u, out[0][2]);
    SEMU_TEST_EQ_U64(c, 23u, out[1][0]); SEMU_TEST_EQ_U64(c, 62u, out[1][1]);
    SEMU_TEST_EQ_U64(c, 45u, out[1][2]);
    /* idx3 = E1 = (68, 187, 136). */
    SEMU_TEST_EQ_U64(c, 68u, out[3][0]); SEMU_TEST_EQ_U64(c, 187u, out[3][1]);
    SEMU_TEST_EQ_U64(c, 136u, out[3][2]);
    /* alpha 0x7ff -> 255 */
    SEMU_TEST_EQ_U64(c, 255u, out[0][3]);

    /* A single aux bit is now a valid exact-color block: color
     * v0 = 1 -> (2, 0, 0) at the pinned alpha law.  The refusal surface
     * for aux blocks is gone by design (ticket 807); near-miss refusals
     * live in the shape/clip/matrix predicates, not the block law. */
    memset(blk, 0, 12u);
    blk[9] = 0x08u | 0x07u; /* one aux bit + alpha 0x7ff */
    SEMU_TEST_EQ_U64(c, 1u, (unsigned)tsc6a_expand_block(blk, out));
    for (p = 0u; p < 16u; ++p) {
        SEMU_TEST_EQ_U64(c, 2u, out[p][0]);
        SEMU_TEST_EQ_U64(c, 0u, out[p][1]);
        SEMU_TEST_EQ_U64(c, 0u, out[p][2]);
    }
    SEMU_TEST_EQ_U64(c, 0u, (unsigned)tsc6a_expand_block(NULL, out));
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_exact_color_expands),
        SEMU_TEST_CASE(test_zero_aux_law_unchanged)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
