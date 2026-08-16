#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "test.h"

#define N (1u << 31)
#define Z (1u << 30)
#define C (1u << 29)
#define V (1u << 28)
#define T (1u << 24)

static void put16(uint8_t *bytes, size_t offset, uint16_t instruction)
{
    bytes[offset] = (uint8_t)instruction;
    bytes[offset + 1u] = (uint8_t)(instruction >> 8);
}

#define SPECIAL(op, high, rm, rd) \
    ((uint16_t)(0x4400u | ((op) << 8u) | ((high) << 7u) | \
                (((rm) & 15u) << 3u) | ((rd) & 7u)))
#define BRANCH_REGISTER(link, rm) \
    ((uint16_t)(0x4700u | ((link) << 7u) | (((rm) & 15u) << 3u)))
#define CONDITIONAL_BRANCH(condition, offset) \
    ((uint16_t)(0xd000u | ((condition) << 8u) | \
                ((uint32_t)((offset) / 2) & 0xffu)))
#define UNCONDITIONAL_BRANCH(offset) \
    ((uint16_t)(0xe000u | ((uint32_t)((offset) / 2) & 0x7ffu)))
#define COMPARE_BRANCH(nonzero, rn, offset) \
    ((uint16_t)(0xb100u | ((nonzero) << 11u) | \
                ((((offset) >> 6u) & 1u) << 9u) | \
                ((((offset) >> 1u) & 0x1fu) << 3u) | ((rn) & 7u)))
#define EXTENSION(operation, rm, rd) \
    ((uint16_t)(0xb200u | ((operation) << 6u) | (((rm) & 7u) << 3u) | \
                ((rd) & 7u)))

static int run_branch(uint16_t instruction, uint32_t xpsr, uint32_t pc)
{
    uint8_t program[4];
    semu_cpu_fixture fixture = {0};
    semu_cpu_state *state;
    int result;

    put16(program, 0u, instruction);
    put16(program, 2u, 0xbe00u);
    if (!semu_cpu_fixture_init(&fixture, program, sizeof(program))) {
        return 0;
    }
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->xpsr = xpsr;
    result = semu_cpu_fixture_step(&fixture) == SEMU_OK && state->r[15] == pc &&
             state->instructions == 1u &&
             semu_cpu_stop_reason(fixture.cpu) == SEMU_STOP_NONE;
    semu_cpu_fixture_destroy(&fixture);
    return result;
}

static void test_branches(semu_test_context *context)
{
    SEMU_TEST_ASSERT(context, run_branch(CONDITIONAL_BRANCH(0u, 6), T | Z,
                                         0x10au));
    SEMU_TEST_ASSERT(context, run_branch(CONDITIONAL_BRANCH(1u, 6), T | Z,
                                         0x102u));
    SEMU_TEST_ASSERT(context, run_branch(CONDITIONAL_BRANCH(1u, -256), T,
                                         0x04u));
    SEMU_TEST_ASSERT(context, run_branch(CONDITIONAL_BRANCH(1u, 254), T,
                                         0x202u));
    SEMU_TEST_ASSERT(context, run_branch(UNCONDITIONAL_BRANCH(-2048), T,
                                         0xfffff904u));
    SEMU_TEST_ASSERT(context, run_branch(UNCONDITIONAL_BRANCH(2046), T,
                                         0x902u));
}

static int run_cb(unsigned nonzero, uint32_t value, uint32_t expected)
{
    uint8_t program[4];
    semu_cpu_fixture fixture = {0};
    semu_cpu_state *state;
    int result;

    put16(program, 0u, COMPARE_BRANCH(nonzero, 0u, 126u));
    put16(program, 2u, 0xbe00u);
    if (!semu_cpu_fixture_init(&fixture, program, sizeof(program))) {
        return 0;
    }
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = value;
    result = semu_cpu_fixture_step(&fixture) == SEMU_OK &&
             state->r[15] == expected;
    semu_cpu_fixture_destroy(&fixture);
    return result;
}

static void test_compare_branches(semu_test_context *context)
{
    SEMU_TEST_ASSERT(context, run_cb(0u, 0u, 0x182u));
    SEMU_TEST_ASSERT(context, run_cb(1u, 0u, 0x102u));
    SEMU_TEST_ASSERT(context, run_cb(1u, 1u, 0x182u));
}

static void test_high_registers(semu_test_context *context)
{
    uint8_t program[10];
    semu_cpu_fixture fixture = {0};
    semu_cpu_state *state;

    put16(program, 0u, SPECIAL(0u, 1u, 0u, 0u));
    put16(program, 2u, SPECIAL(2u, 1u, 8u, 1u));
    put16(program, 4u, SPECIAL(1u, 1u, 1u, 1u));
    put16(program, 6u, SPECIAL(0u, 1u, 0u, 5u));
    put16(program, 8u, 0xbe00u);
    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program, sizeof(program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = 3u;
    state->r[1] = 3u;
    state->xpsr = T | C | V;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_run(&fixture, 4u));
    SEMU_TEST_EQ_U64(context, 3u, state->r[8]);
    SEMU_TEST_EQ_U64(context, 3u, state->r[9]);
    SEMU_TEST_EQ_U64(context, 0x803u, state->r[13]);
    SEMU_TEST_EQ_U64(context, T | Z | C, state->xpsr);
    SEMU_TEST_EQ_U64(context, 0x108u, state->r[15]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_pc_link_and_bx(semu_test_context *context)
{
    uint8_t program[4];
    semu_cpu_fixture fixture = {0};
    semu_cpu_state *state;

    put16(program, 0u, SPECIAL(2u, 1u, 0u, 7u));
    put16(program, 2u, 0xbe00u);
    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program, sizeof(program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = 0x181u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0x180u, state->r[15]);
    semu_cpu_fixture_destroy(&fixture);

    put16(program, 0u, BRANCH_REGISTER(1u, 0u));
    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program, sizeof(program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = 0x181u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0x180u, state->r[15]);
    SEMU_TEST_EQ_U64(context, 0x103u, state->r[14]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_reverse_extend(semu_test_context *context)
{
    uint8_t program[16];
    semu_cpu_fixture fixture = {0};
    semu_cpu_state *state;

    put16(program, 0u, 0xba08u);
    put16(program, 2u, 0xba5au);
    put16(program, 4u, 0xbaecu);
    put16(program, 6u, EXTENSION(1u, 1u, 6u));
    put16(program, 8u, EXTENSION(0u, 3u, 7u));
    put16(program, 10u, EXTENSION(3u, 5u, 1u));
    put16(program, 12u, EXTENSION(2u, 7u, 3u));
    put16(program, 14u, 0xbe00u);
    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program, sizeof(program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[1] = 0x12345680u;
    state->r[3] = 0x00008001u;
    state->r[5] = 0x123480ffu;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_run(&fixture, 7u));
    SEMU_TEST_EQ_U64(context, 0x80563412u, state->r[0]);
    SEMU_TEST_EQ_U64(context, 0x180u, state->r[2]);
    SEMU_TEST_EQ_U64(context, 0xffffff80u, state->r[4]);
    SEMU_TEST_EQ_U64(context, 0xffffff80u, state->r[6]);
    SEMU_TEST_EQ_U64(context, 0xffff8001u, state->r[7]);
    SEMU_TEST_EQ_U64(context, 0xffu, state->r[1]);
    SEMU_TEST_EQ_U64(context, 0x8001u, state->r[3]);
    semu_cpu_fixture_destroy(&fixture);
}

static int run_it_length(unsigned mask, unsigned count)
{
    uint8_t program[12] = {0};
    semu_cpu_fixture fixture = {0};
    semu_cpu_state *state;
    unsigned index;
    int result;

    put16(program, 0u, (uint16_t)(0xbf00u | mask));
    for (index = 0u; index < count; ++index) {
        put16(program, 2u + index * 2u, SPECIAL(2u, 1u, 0u, 0u));
    }
    put16(program, 2u + count * 2u, 0xbe00u);
    if (!semu_cpu_fixture_init(&fixture, program, 4u + count * 2u)) {
        return 0;
    }
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = 0x12345678u;
    state->xpsr = T | Z;
    result = semu_cpu_fixture_run(&fixture, count + 1u) == SEMU_OK &&
             state->r[8] == 0x12345678u && state->r[15] ==
             0x100u + (count + 1u) * 2u &&
             state->instructions == count + 1u;
    semu_cpu_fixture_destroy(&fixture);
    return result;
}

static void test_it(semu_test_context *context)
{
    uint8_t program[12] = {0};
    semu_cpu_fixture fixture = {0};
    semu_cpu_state *state;
    static const unsigned masks[] = {8u, 4u, 2u, 1u};
    unsigned index;

    for (index = 0u; index < 4u; ++index) {
        SEMU_TEST_ASSERT(context, run_it_length(masks[index], index + 1u));
    }
    put16(program, 0u, 0xbf19u);
    put16(program, 2u, SPECIAL(2u, 1u, 0u, 0u));
    put16(program, 4u, SPECIAL(2u, 1u, 1u, 1u));
    put16(program, 6u, SPECIAL(2u, 1u, 2u, 2u));
    put16(program, 8u, SPECIAL(2u, 1u, 3u, 3u));
    put16(program, 10u, 0xbe00u);
    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program, sizeof(program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = 1u;
    state->r[1] = 2u;
    state->r[2] = 3u;
    state->r[3] = 4u;
    state->xpsr = T | Z;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_run(&fixture, 5u));
    SEMU_TEST_EQ_U64(context, 0u, state->r[8]);
    SEMU_TEST_EQ_U64(context, 0u, state->r[9]);
    SEMU_TEST_EQ_U64(context, 3u, state->r[10]);
    SEMU_TEST_EQ_U64(context, 4u, state->r[11]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_hints_sleep_and_bkpt(semu_test_context *context)
{
    static const uint8_t event_program[] = {
        0x40u, 0xbfu, 0x20u, 0xbfu, 0x00u, 0xbfu,
        0x10u, 0xbfu, 0x00u, 0xbeu
    };
    static const uint8_t wfe_program[] = {0x20u, 0xbfu, 0x00u, 0xbeu};
    static const uint8_t wfi_program[] = {0x30u, 0xbfu, 0x00u, 0xbeu};
    semu_cpu_fixture fixture = {0};
    const semu_cpu_state *state;

    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, event_program,
                                                    sizeof(event_program)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_run(&fixture, 4u));
    state = semu_cpu_get_state(fixture.cpu);
    SEMU_TEST_ASSERT(context, !state->waiting_for_interrupt);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
                     semu_cpu_stop_reason(fixture.cpu));
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, wfe_program,
                                                    sizeof(wfe_program)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_ASSERT(context,
                     semu_cpu_get_state(fixture.cpu)->waiting_for_interrupt);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_WFI_DEADLOCK,
                     semu_cpu_stop_reason(fixture.cpu));
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, wfi_program,
                                                    sizeof(wfi_program)));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_ASSERT(context,
                     semu_cpu_get_state(fixture.cpu)->waiting_for_interrupt);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_WFI_DEADLOCK,
                     semu_cpu_stop_reason(fixture.cpu));
    semu_cpu_fixture_destroy(&fixture);
}

static int refusal(uint16_t instruction, int unprivileged)
{
    uint8_t program[4];
    semu_cpu_fixture fixture = {0};
    semu_cpu_state *state;
    uint32_t initial_r0 = 0x13579bdfu;
    int result;

    put16(program, 0u, instruction);
    put16(program, 2u, 0xbe00u);
    if (!semu_cpu_fixture_init(&fixture, program, sizeof(program))) {
        return 0;
    }
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = initial_r0;
    state->r[14] = 0xfeed0001u;
    state->xpsr = T | N | C | V;
    state->control = unprivileged ? 1u : 0u;
    result = semu_cpu_fixture_step(&fixture) == SEMU_ERR_UNSUPPORTED &&
             state->r[0] == initial_r0 && state->r[14] == 0xfeed0001u &&
             state->xpsr == (T | N | C | V) && state->instructions == 0u &&
             semu_cpu_stop_reason(fixture.cpu) ==
                 SEMU_STOP_UNSUPPORTED_INSTRUCTION;
    semu_cpu_fixture_destroy(&fixture);
    return result;
}

static void test_cps_and_refusals(semu_test_context *context)
{
    static const uint8_t program[] = {
        0x72u, 0xb6u, 0x62u, 0xb6u, 0x71u, 0xb6u,
        0x61u, 0xb6u, 0x73u, 0xb6u, 0x63u, 0xb6u,
        0x00u, 0xbeu
    };
    semu_cpu_fixture fixture = {0};
    semu_cpu_state *state;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program, sizeof(program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_run(&fixture, 6u));
    SEMU_TEST_EQ_U64(context, 0u, state->primask);
    SEMU_TEST_EQ_U64(context, 0u, state->faultmask);
    SEMU_TEST_EQ_U64(context, 6u, state->instructions);
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context, refusal(0xbf50u, 0));
    SEMU_TEST_ASSERT(context, refusal(0xbff1u, 0));
    SEMU_TEST_ASSERT(context, refusal(0xba80u, 0));
    SEMU_TEST_ASSERT(context, refusal(0xb664u, 0));
    SEMU_TEST_ASSERT(context, refusal(SPECIAL(1u, 1u, 15u, 0u), 0));
    SEMU_TEST_ASSERT(context, refusal(BRANCH_REGISTER(1u, 15u), 0));
    SEMU_TEST_ASSERT(context, refusal(0xde00u, 0));
    SEMU_TEST_ASSERT(context, refusal(0xb672u, 1));
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_branches),
        SEMU_TEST_CASE(test_compare_branches),
        SEMU_TEST_CASE(test_high_registers),
        SEMU_TEST_CASE(test_pc_link_and_bx),
        SEMU_TEST_CASE(test_reverse_extend),
        SEMU_TEST_CASE(test_it),
        SEMU_TEST_CASE(test_hints_sleep_and_bkpt),
        SEMU_TEST_CASE(test_cps_and_refusals)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
