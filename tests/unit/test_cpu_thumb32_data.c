#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "test.h"

#include <string.h>

/* TEST_TAGS: cpu_renode_regressions */

#define XPSR_N (1u << 31)
#define XPSR_Z (1u << 30)
#define XPSR_C (1u << 29)
#define XPSR_V (1u << 28)
#define XPSR_T (1u << 24)
#define REG_FIRST(op, s, rn) \
    (uint16_t)(0xea00u | ((op) << 5) | ((s) << 4) | (rn))
#define ARITH_FIRST(op, s, rn) \
    (uint16_t)(0xeb00u | ((op) << 5) | ((s) << 4) | (rn))
#define REG_SECOND(rd, type, amount, rm) \
    (uint16_t)(((rd) << 8) | (((amount) & 0x1cu) << 10) | \
               (((amount) & 3u) << 6) | ((type) << 4) | (rm))
#define MOD_FIRST(op, s, rn, i) \
    (uint16_t)(0xf000u | ((op) << 5) | ((s) << 4) | (rn) | ((i) << 10))
#define MOD_SECOND(rd, imm3, imm8) \
    (uint16_t)(((rd) << 8) | ((imm3) << 12) | (imm8))

static semu_stop_reason last_stop_reason;

static semu_cpu_state initial_state(void)
{
    semu_cpu_state state;
    (void)memset(&state, 0, sizeof(state));
    state.r[13] = 0x800u;
    state.r[15] = 0x100u;
    state.msp = 0x800u;
    state.xpsr = XPSR_T;
    return state;
}

static int run32(uint16_t first, uint16_t second, semu_cpu_state initial,
                 semu_status *status, semu_cpu_state *final)
{
    static const uint8_t stop[] = {0x00u, 0xbeu};
    uint8_t program[] = {(uint8_t)first, (uint8_t)(first >> 8),
                         (uint8_t)second, (uint8_t)(second >> 8),
                         stop[0], stop[1]};
    semu_cpu_fixture fixture;
    int ok = semu_cpu_fixture_init(&fixture, program, sizeof(program));

    if (!ok) {
        return 0;
    }
    initial.r[15] = 0x100u;
    initial.instructions = 0u;
    initial.halted = 0;
    initial.waiting_for_interrupt = 0;
    semu_cpu_fixture_apply_state(&fixture, &initial);
    *status = semu_cpu_fixture_step(&fixture);
    last_stop_reason = semu_cpu_stop_reason(fixture.cpu);
    *final = *semu_cpu_get_state(fixture.cpu);
    semu_cpu_fixture_destroy(&fixture);
    return 1;
}

static int branch_encoding(int32_t displacement, int link,
                           uint16_t *first, uint16_t *second)
{
    uint32_t bits = (uint32_t)displacement & 0x01ffffffu;
    uint32_t s = bits >> 24u;
    uint32_t i1 = (bits >> 23u) & 1u;
    uint32_t i2 = (bits >> 22u) & 1u;
    uint32_t j1 = (~(i1 ^ s)) & 1u;
    uint32_t j2 = (~(i2 ^ s)) & 1u;

    *first = (uint16_t)(0xf000u | (s << 10u) | ((bits >> 12u) & 0x3ffu));
    *second = (uint16_t)((link ? 0xd000u : 0x9000u) |
                         (j1 << 13u) | (j2 << 11u) |
                         ((bits >> 1u) & 0x7ffu));
    return ((uint32_t)displacement & 1u) == 0u;
}

static int same_architecture(const semu_cpu_state *before,
                             const semu_cpu_state *after)
{
    semu_cpu_state copy = *after;
    copy.halted = before->halted;
    return memcmp(before, &copy, sizeof(copy)) == 0;
}

static void test_EA4F_000D_and_register_forms(semu_test_context *context)
{
    semu_cpu_state state = initial_state();
    semu_status status;
    uint16_t first;
    uint16_t second;
    static const struct {
        unsigned op;
        uint32_t value;
    } logical[] = {
        {0u, 0x00f00000u}, {1u, 0xf0000000u}, {2u, 0xfff00000u},
        {3u, 0xf0ffffffu}, {4u, 0xff000000u}
    };
    size_t index;

    state.r[1] = 0xf0f00000u;
    state.r[2] = 0x0ff00000u;
    for (index = 0u; index < sizeof(logical) / sizeof(logical[0]); ++index) {
        first = REG_FIRST(logical[index].op, 0u, 1u);
        second = REG_SECOND(0u, 0u, 0u, 2u);
        SEMU_TEST_ASSERT(context, run32(first, second, state, &status,
                                        &state));
        SEMU_TEST_EQ_U64(context, SEMU_OK, status);
        SEMU_TEST_EQ_U64(context, logical[index].value, state.r[0]);
        SEMU_TEST_EQ_U64(context, XPSR_T, state.xpsr);
    }
    state = initial_state();
    state.r[13] = 0x812u;
    first = 0xea4fu;
    second = 0x000du;
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x812u, state.r[0]);
    SEMU_TEST_EQ_U64(context, 0x104u, state.r[15]);
    state = initial_state();
    state.xpsr = XPSR_T | XPSR_C | XPSR_V;
    state.r[1] = 0x80000001u;
    first = 0xea4fu;
    second = REG_SECOND(0u, 1u, 0u, 1u);
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, 0u, state.r[0]);
    SEMU_TEST_EQ_U64(context, XPSR_T | XPSR_C | XPSR_V, state.xpsr);
    first = 0xea5fu;
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, XPSR_T | XPSR_Z | XPSR_C | XPSR_V,
                     state.xpsr);
    first = 0xea4fu;
    second = REG_SECOND(0u, 2u, 0u, 1u);
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, 0xffffffffu, state.r[0]);
    second = REG_SECOND(0u, 3u, 0u, 1u);
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, 0xc0000000u, state.r[0]);
    first = 0xea4fu;
    second = 0x0041u;
    state = initial_state();
    state.r[1] = 0x40000000u;
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, 0x80000000u, state.r[0]);
    first = ARITH_FIRST(0u, 1u, 1u);
    second = REG_SECOND(0u, 0u, 0u, 2u);
    state = initial_state();
    state.r[1] = 0x7fffffffu;
    state.r[2] = 1u;
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, 0x80000000u, state.r[0]);
    SEMU_TEST_EQ_U64(context, XPSR_T | XPSR_N | XPSR_V, state.xpsr);

    {
        static const struct { unsigned op; uint32_t value; } arithmetic[] = {
            {2u, 14u}, {3u, 7u}, {5u, 7u}, {6u, 0xfffffff9u}
        };
        size_t arithmetic_index;
        state = initial_state();
        state.xpsr |= XPSR_C;
        state.r[1] = 10u;
        state.r[2] = 3u;
        for (arithmetic_index = 0u;
             arithmetic_index < sizeof(arithmetic) / sizeof(arithmetic[0]);
             ++arithmetic_index) {
            first = ARITH_FIRST(arithmetic[arithmetic_index].op, 0u, 1u);
            second = REG_SECOND(0u, 0u, 0u, 2u);
            SEMU_TEST_ASSERT(context, run32(first, second, state, &status,
                                            &state));
            SEMU_TEST_EQ_U64(context, SEMU_OK, status);
            SEMU_TEST_EQ_U64(context, arithmetic[arithmetic_index].value,
                             state.r[0]);
            SEMU_TEST_EQ_U64(context, XPSR_T | XPSR_C, state.xpsr);
        }
    }

    state = initial_state();
    state.r[1] = 1u;
    state.r[2] = 1u;
    first = REG_FIRST(4u, 1u, 1u);
    second = REG_SECOND(15u, 0u, 0u, 2u);
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, XPSR_T | XPSR_Z, state.xpsr);
    first = ARITH_FIRST(0u, 1u, 1u);
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, XPSR_T, state.xpsr);
    first = ARITH_FIRST(5u, 1u, 1u);
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, XPSR_T | XPSR_Z | XPSR_C, state.xpsr);
}

static void test_modified_immediate_and_aliases(semu_test_context *context)
{
    semu_cpu_state state = initial_state();
    semu_status status;
    uint16_t first;
    uint16_t second;

    state.r[0] = 3u;
    SEMU_TEST_ASSERT(context, run32(0xf440u, 0x0070u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0x00f00003u, state.r[0]);
    state = initial_state();
    state.xpsr = XPSR_T | XPSR_C | XPSR_V;
    first = MOD_FIRST(0u, 1u, 1u, 0u);
    second = MOD_SECOND(0u, 0u, 0u);
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, 0u, state.r[0]);
    SEMU_TEST_EQ_U64(context, XPSR_T | XPSR_Z | XPSR_C | XPSR_V,
                     state.xpsr);
    state = initial_state();
    state.r[1] = 0xffffffffu;
    first = MOD_FIRST(8u, 1u, 1u, 0u);
    second = MOD_SECOND(0u, 0u, 1u);
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, 0u, state.r[0]);
    SEMU_TEST_EQ_U64(context, XPSR_T | XPSR_Z | XPSR_C, state.xpsr);
    state = initial_state();
    state.r[1] = 0u;
    first = MOD_FIRST(0u, 1u, 1u, 0u);
    second = MOD_SECOND(15u, 0u, 1u);
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, XPSR_T | XPSR_Z, state.xpsr);
    state = initial_state();
    state.r[1] = 1u;
    first = MOD_FIRST(13u, 1u, 1u, 0u);
    second = MOD_SECOND(15u, 0u, 1u);
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, XPSR_T | XPSR_Z | XPSR_C, state.xpsr);

    state = initial_state();
    state.r[1] = 0x80000001u;
    first = MOD_FIRST(2u, 0u, 15u, 0u);
    second = MOD_SECOND(0u, 0u, 1u);
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, 1u, state.r[0]);
    SEMU_TEST_EQ_U64(context, XPSR_T, state.xpsr);
    first = MOD_FIRST(2u, 1u, 15u, 0u);
    state.xpsr |= XPSR_C;
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, XPSR_T | XPSR_C, state.xpsr);
    first = MOD_FIRST(3u, 1u, 15u, 0u);
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, 0xfffffffeu, state.r[0]);
    SEMU_TEST_EQ_U64(context, XPSR_T | XPSR_N | XPSR_C, state.xpsr);
}

static void test_wide_bitfield_and_extract(semu_test_context *context)
{
    semu_cpu_state state = initial_state();
    semu_status status;

    SEMU_TEST_ASSERT(context, run32(0xf241u, 0x2034u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0x1234u, state.r[0]);
    state.r[0] = 0xcafe5678u;
    SEMU_TEST_ASSERT(context, run32(0xf2c1u, 0x2034u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0x12345678u, state.r[0]);
    state = initial_state();
    SEMU_TEST_ASSERT(context, run32(0xf649u, 0x2034u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0x9a34u, state.r[0]);
    state.r[0] = 0x56789abcu;
    SEMU_TEST_ASSERT(context, run32(0xf6c9u, 0x2034u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0x9a349abcu, state.r[0]);
    state = initial_state();
    state.r[1] = 0x100u;
    SEMU_TEST_ASSERT(context, run32(0xf201u, 0x1023u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0x223u, state.r[0]);
    state.r[1] = 0x200u;
    SEMU_TEST_ASSERT(context, run32(0xf2a1u, 0x1023u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0xddu, state.r[0]);
    state = initial_state();
    state.r[1] = 0u;
    SEMU_TEST_ASSERT(context, run32(0xf601u, 0x2034u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0xa34u, state.r[0]);
    state = initial_state();
    SEMU_TEST_ASSERT(context, run32(0xf20du, 0x0d01u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0x801u, state.r[13]);
    SEMU_TEST_ASSERT(context, run32(0xf1adu, 0x0d01u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0x800u, state.r[13]);
    state = initial_state();
    SEMU_TEST_ASSERT(context, run32(0xf2afu, 0x0008u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0xfcu, state.r[0]);
    state = initial_state();
    state.r[1] = 0x00f00000u;
    SEMU_TEST_ASSERT(context, run32(0xfab1u, 0xf081u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 8u, state.r[0]);
    state = initial_state();
    state.r[0] = 0xffff0000u;
    state.r[1] = 5u;
    SEMU_TEST_ASSERT(context, run32(0xf361u, 0x00cau, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0xffff0028u, state.r[0]);
    state.r[0] = 0xffffffffu;
    SEMU_TEST_ASSERT(context, run32(0xf36fu, 0x00cau, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0xfffff807u, state.r[0]);
    state.r[1] = 0x000007f8u;
    SEMU_TEST_ASSERT(context, run32(0xf341u, 0x00c7u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0xffffffffu, state.r[0]);
    SEMU_TEST_ASSERT(context, run32(0xf3c1u, 0x00c7u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0xffu, state.r[0]);

    state = initial_state();
    state.r[0] = 0u;
    state.r[1] = 1u;
    SEMU_TEST_ASSERT(context, run32(0xf361u, 0x0000u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 1u, state.r[0]);
    state.r[0] = 0xffffffffu;
    SEMU_TEST_ASSERT(context, run32(0xf36fu, 0x001fu, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0u, state.r[0]);
    state.r[1] = 1u;
    SEMU_TEST_ASSERT(context, run32(0xf341u, 0x0000u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0xffffffffu, state.r[0]);
    state.r[1] = 0xffffffffu;
    SEMU_TEST_ASSERT(context, run32(0xf3c1u, 0x001fu, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, 0xffffffffu, state.r[0]);
}

static void test_branch_bounds_and_link(semu_test_context *context)
{
    semu_cpu_state state = initial_state();
    semu_status status;
    uint16_t first;
    uint16_t second;

    SEMU_TEST_ASSERT(context, branch_encoding(0, 0, &first, &second));
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, 0x104u, state.r[15]);
    SEMU_TEST_ASSERT(context, branch_encoding(0x7ffffeu, 0, &first, &second));
    state = initial_state();
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, 0x800102u, state.r[15]);
    SEMU_TEST_ASSERT(context, branch_encoding(-0x800000, 0, &first, &second));
    state = initial_state();
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, 0xff800104u, state.r[15]);
    SEMU_TEST_ASSERT(context, branch_encoding(0, 1, &first, &second));
    state = initial_state();
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, 0x104u, state.r[15]);
    SEMU_TEST_EQ_U64(context, 0x105u, state.r[14]);
    SEMU_TEST_ASSERT(context, branch_encoding(0x00fffffe, 0, &first, &second));
    state = initial_state();
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, 0x01000102u, state.r[15]);
    SEMU_TEST_ASSERT(context, branch_encoding(-0x01000000, 0, &first, &second));
    state = initial_state();
    SEMU_TEST_ASSERT(context, run32(first, second, state, &status, &state));
    SEMU_TEST_EQ_U64(context, 0xff000104u, state.r[15]);
}

static void test_conditional_branch_t3(semu_test_context *context)
{
    /* B<cc>.W T3 encoding: first = 11110 S cond imm6,
     * second = 10 J1 0 J2 imm11. In T3, I1 = J2 (bit 11) and
     * I2 = J1 (bit 13), i.e. the J bits are swapped relative to T4
     * and used directly (no inversion). For displacement +32 (S=0,
     * I1=0, I2=0, imm6=0, imm11=0x10), J1=J2=0.
     * Target = pc + 4 + 32 = 0x124 when taken. */
    semu_cpu_state state = initial_state();
    semu_status status;

    /* BEQ.W (cond=EQ=0): Z=0 → not taken, falls through to pc+4. */
    state = initial_state();
    SEMU_TEST_ASSERT(context, run32(0xf000u, 0x8010u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x104u, state.r[15]);

    /* BPL.W shares its first halfword with the barrier family. The
     * dispatcher must route this non-barrier-shaped operand to the branch
     * decoder. */
    state = initial_state();
    SEMU_TEST_ASSERT(context, run32(0xf57fu, 0xae88u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0xfffffe14u, state.r[15]);

    /* BEQ.W (cond=EQ=0): Z=1 → taken, target = 0x124. */
    state = initial_state();
    state.xpsr |= XPSR_Z;
    SEMU_TEST_ASSERT(context, run32(0xf000u, 0x8010u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x124u, state.r[15]);

    /* BGT.W (cond=GT=0xC): Z=0 and N==V → taken. This encoding
     * (first=0xf300) collides with the DSP saturation dispatch and
     * must be routed to conditional_branch via the branch pre-check. */
    state = initial_state();
    SEMU_TEST_ASSERT(context, run32(0xf300u, 0x8010u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x124u, state.r[15]);

    /* BGT.W (cond=GT=0xC): Z=1 → not taken. */
    state = initial_state();
    state.xpsr |= XPSR_Z;
    SEMU_TEST_ASSERT(context, run32(0xf300u, 0x8010u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x104u, state.r[15]);

    /* Large positive offset with I1=1 (displacement +0x80010). In T3,
     * I1 = J2 (bit 11) and I2 = J1 (bit 13), swapped from T4. So J2=1
     * encodes I1=1. The 21-bit T3 sign extension must be used, not
     * the 25-bit T4 extension. */
    state = initial_state();
    state.xpsr |= XPSR_Z;
    SEMU_TEST_ASSERT(context, run32(0xf000u, 0x8808u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x80114u, state.r[15]);

    /* Large positive offset with I2=1 (displacement +0x40010). J1=1
     * encodes I2=1 in T3. Same 21-bit vs 25-bit regression check. */
    state = initial_state();
    state.xpsr |= XPSR_Z;
    SEMU_TEST_ASSERT(context, run32(0xf000u, 0xa008u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x40114u, state.r[15]);

    /* Negative offset with S=1, I1=1, I2=1 (displacement -0x80). Target
     * = 0x100 + 4 - 0x80 = 0x84. A buggy 25-bit extension would produce
     * a wildly different negative target. */
    state = initial_state();
    state.xpsr |= XPSR_Z;
    SEMU_TEST_ASSERT(context, run32(0xf43fu, 0xafc0u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x84u, state.r[15]);

    /* Refusal: cond=15 (undefined) with T3 encoding. */
    state = initial_state();
    SEMU_TEST_ASSERT(context, run32(0xf3c0u, 0xa810u, state, &status,
                                    &state));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, status);
    SEMU_TEST_EQ_U64(context, 0x100u, state.r[15]);
}

static void test_refusals_preserve_state(semu_test_context *context)
{
    static const struct { uint16_t first; uint16_t second; } cases[] = {
        {0xea4fu, 0x000fu}, {0xea0du, 0x0001u},
        {0xf240u, 0x0d01u}, {0xf361u, 0x70c0u}, {0xf341u, 0x70c1u}
    };
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        semu_cpu_state before = initial_state();
        semu_cpu_state after;
        semu_status status;
        before.r[0] = 0x13579bdfu;
        before.r[1] = 0x2468ace0u;
        before.xpsr = XPSR_T | XPSR_N | XPSR_C | XPSR_V;
        SEMU_TEST_ASSERT(context, run32(cases[index].first,
                                        cases[index].second, before,
                                        &status, &after));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, status);
        SEMU_TEST_ASSERT(context, same_architecture(&before, &after));
        SEMU_TEST_ASSERT(context, after.halted);
        SEMU_TEST_EQ_U64(context, SEMU_STOP_UNSUPPORTED_INSTRUCTION,
                         last_stop_reason);
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_EA4F_000D_and_register_forms),
        SEMU_TEST_CASE(test_modified_immediate_and_aliases),
        SEMU_TEST_CASE(test_wide_bitfield_and_extract),
        SEMU_TEST_CASE(test_branch_bounds_and_link),
        SEMU_TEST_CASE(test_conditional_branch_t3),
        SEMU_TEST_CASE(test_refusals_preserve_state)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
