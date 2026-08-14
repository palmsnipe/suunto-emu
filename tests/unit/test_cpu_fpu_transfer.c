#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "../../src/cpu/armv7m/armv7m_internal.h"
#include "test.h"

#include <string.h>

#define SCS 0xe000e000u
#define CPACR (SCS + 0xd88u)
#define SHCSR (SCS + 0xd24u)
#define CFSR (SCS + 0xd28u)
#define FPCCR (SCS + 0xf34u)
#define FPCAR (SCS + 0xf38u)
#define FPDSCR (SCS + 0xf3cu)
#define XPSR_T (1u << 24)
#define NOCP (1u << 19)

static int write_word(semu_cpu_fixture *fixture, uint32_t address,
                      uint32_t value)
{
    return semu_bus_write(fixture->bus, address, 4u, value,
                          &fixture->error) == SEMU_OK;
}

static int read_word(semu_cpu_fixture *fixture, uint32_t address,
                     uint32_t *value)
{
    return semu_bus_read(fixture->bus, address, 4u, value,
                         &fixture->error) == SEMU_OK;
}

static int enable_fpu(semu_cpu_fixture *fixture, uint32_t value)
{
    return write_word(fixture, CPACR, value);
}

static int init_enabled(semu_cpu_fixture *fixture, const uint8_t *program,
                        size_t size)
{
    return semu_cpu_fixture_init(fixture, program, size) &&
           enable_fpu(fixture, 0x00f00000u);
}

static void test_reset_access_and_system_registers(semu_test_context *context)
{
    static const uint8_t program[] = {0x00u, 0xbeu};
    semu_cpu_fixture fixture;
    semu_cpu_state before;
    uint32_t value;

    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, program,
                                                     sizeof(program)));
    SEMU_TEST_EQ_U64(context, 0u, semu_cpu_get_state(fixture.cpu)->fpscr);
    SEMU_TEST_EQ_U64(context, 0u,
                     semu_cpu_get_state(fixture.cpu)->s[0]);
    SEMU_TEST_EQ_U64(context, 0u,
                     semu_cpu_get_state(fixture.cpu)->s[31]);
    SEMU_TEST_ASSERT(context, read_word(&fixture, FPCCR, &value));
    SEMU_TEST_EQ_U64(context, 0xc0000000u, value);
    SEMU_TEST_ASSERT(context, read_word(&fixture, FPCAR, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_ASSERT(context, read_word(&fixture, FPDSCR, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_ASSERT(context, enable_fpu(&fixture, 0x00f00000u));
    SEMU_TEST_ASSERT(context, read_word(&fixture, CPACR, &value));
    SEMU_TEST_EQ_U64(context, 0x00f00000u, value);
    SEMU_TEST_ASSERT(context, write_word(&fixture, FPCAR, 0x1234567fu));
    SEMU_TEST_ASSERT(context, read_word(&fixture, FPCAR, &value));
    SEMU_TEST_EQ_U64(context, 0x12345678u, value);
    SEMU_TEST_ASSERT(context, write_word(&fixture, FPDSCR, 0x07c00000u));
    SEMU_TEST_ASSERT(context, read_word(&fixture, FPDSCR, &value));
    SEMU_TEST_EQ_U64(context, 0x07c00000u, value);

    semu_cpu_get_state_mutable(fixture.cpu)->control = 1u;
    before = *semu_cpu_get_state(fixture.cpu);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(fixture.bus, FPCCR, 4u, &value,
                                   &fixture.error));
    SEMU_TEST_ASSERT(context,
                     memcmp(&before, semu_cpu_get_state(fixture.cpu),
                            sizeof(before)) == 0);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_nocp_disabled_and_partial_access(semu_test_context *context)
{
    static const uint8_t program[] = {0x00u, 0xeeu, 0x10u, 0x0au};
    semu_cpu_fixture fixture;
    semu_cpu_state *state;
    uint32_t value;

    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, program,
                                                     sizeof(program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 24u,
                                                         0x181u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SHCSR, 1u << 18));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 6u, state->xpsr & 0x1ffu);
    SEMU_TEST_EQ_U64(context, 0x180u, state->r[15]);
    SEMU_TEST_ASSERT(context, read_word(&fixture, 0x7f8u, &value));
    SEMU_TEST_EQ_U64(context, 0x100u, value);
    SEMU_TEST_ASSERT(context, read_word(&fixture, CFSR, &value));
    SEMU_TEST_EQ_U64(context, NOCP, value);
    SEMU_TEST_EQ_U64(context, 0u, state->s[0]);
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, program,
                                                     sizeof(program)));
    SEMU_TEST_ASSERT(context, write_word(&fixture, CPACR, 0x00300000u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SHCSR, 1u << 18));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 24u,
                                                         0x181u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 6u,
                     semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu);
    SEMU_TEST_ASSERT(context, read_word(&fixture, CFSR, &value));
    SEMU_TEST_EQ_U64(context, NOCP, value);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_scalar_and_pair_raw_transfers(semu_test_context *context)
{
    static const uint8_t program[] = {
        0x00u, 0xeeu, 0x10u, 0x0au, /* vmov s0,r0 */
        0x10u, 0xeeu, 0x10u, 0x2au, /* vmov r2,s0 */
        0x0fu, 0xeeu, 0x90u, 0x1au, /* vmov s31,r1 */
        0x1fu, 0xeeu, 0x90u, 0x3au, /* vmov r3,s31 */
        0x45u, 0xecu, 0x1fu, 0x4bu, /* vmov d15,r4,r5 */
        0x57u, 0xecu, 0x1fu, 0x6bu  /* vmov r6,r7,d15 */
    };
    semu_cpu_fixture fixture;
    semu_cpu_state *state;

    SEMU_TEST_ASSERT(context, init_enabled(&fixture, program,
                                           sizeof(program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = 0x7fc00001u;
    state->r[1] = 0x80000000u;
    state->r[4] = 0x7f800000u;
    state->r[5] = 0xffc00001u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_cpu_fixture_run(&fixture, 4u));
    SEMU_TEST_EQ_U64(context, 0x7fc00001u, state->s[0]);
    SEMU_TEST_EQ_U64(context, 0x7fc00001u, state->r[2]);
    SEMU_TEST_EQ_U64(context, 0x80000000u, state->s[31]);
    SEMU_TEST_EQ_U64(context, 0x80000000u, state->r[3]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_cpu_fixture_run(&fixture, 2u));
    SEMU_TEST_EQ_U64(context, 0x7f800000u, state->s[30]);
    SEMU_TEST_EQ_U64(context, 0xffc00001u, state->s[31]);
    SEMU_TEST_EQ_U64(context, 0x7f800000u, state->r[6]);
    SEMU_TEST_EQ_U64(context, 0xffc00001u, state->r[7]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_fpscr_and_nzcv_transfers(semu_test_context *context)
{
    static const uint8_t program[] = {
        0xe1u, 0xeeu, 0x10u, 0x0au, /* vmsr fpscr,r0 */
        0xf1u, 0xeeu, 0x10u, 0x8au, /* vmrs r8,fpscr */
        0xf1u, 0xeeu, 0x10u, 0xfau  /* vmrs apsr_nzcv,fpscr */
    };
    semu_cpu_fixture fixture;
    semu_cpu_state *state;

    SEMU_TEST_ASSERT(context, init_enabled(&fixture, program,
                                           sizeof(program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = 0xf0000000u;
    state->xpsr |= 1u << 27;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_cpu_fixture_run(&fixture, 3u));
    SEMU_TEST_EQ_U64(context, 0xf0000000u, state->fpscr);
    SEMU_TEST_EQ_U64(context, 0xf0000000u, state->r[8]);
    SEMU_TEST_EQ_U64(context, 0xf8000000u, state->xpsr & 0xf8000000u);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_single_memory_bounds_and_multiple_writeback(
    semu_test_context *context)
{
    static const uint8_t memory_program[] = {
        0xd0u, 0xedu, 0x00u, 0xfau, /* vldr s31,[r0] */
        0xc1u, 0xedu, 0x00u, 0xfau  /* vstr s31,[r1] */
    };
    static const uint8_t multiple_program[] = {
        0xb0u, 0xecu, 0x04u, 0xeau, /* vldmia r0!,{s28-s31} */
        0x21u, 0xedu, 0x04u, 0xeau  /* vstmdb r1!,{s28-s31} */
    };
    semu_cpu_fixture fixture;
    semu_cpu_state *state;
    uint32_t value;
    unsigned index;

    SEMU_TEST_ASSERT(context, init_enabled(&fixture, memory_program,
                                           sizeof(memory_program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = 0x600u;
    state->r[1] = 0x604u;
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x600u,
                                                         0x80000000u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_cpu_fixture_run(&fixture, 2u));
    SEMU_TEST_EQ_U64(context, 0x80000000u, state->s[31]);
    SEMU_TEST_ASSERT(context, read_word(&fixture, 0x604u, &value));
    SEMU_TEST_EQ_U64(context, 0x80000000u, value);
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context, init_enabled(&fixture, multiple_program,
                                           sizeof(multiple_program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = 0x600u;
    state->r[1] = 0x700u;
    for (index = 0u; index < 4u; ++index)
        SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(
            &fixture, 0x600u + index * 4u, 0xa0000000u + index));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_cpu_fixture_run(&fixture, 2u));
    SEMU_TEST_EQ_U64(context, 0x610u, state->r[0]);
    SEMU_TEST_EQ_U64(context, 0x6f0u, state->r[1]);
    for (index = 0u; index < 4u; ++index) {
        SEMU_TEST_ASSERT(context, read_word(&fixture, 0x6f0u + index * 4u,
                                             &value));
        SEMU_TEST_EQ_U64(context, 0xa0000000u + index, value);
        SEMU_TEST_EQ_U64(context, 0xa0000000u + index, state->s[28u + index]);
    }
    semu_cpu_fixture_destroy(&fixture);
}

static void test_refusals_are_bounded(semu_test_context *context)
{
    static const uint8_t program[] = {0x00u, 0xbeu};
    semu_cpu_fixture fixture;
    semu_cpu_state *state;
    uint32_t value;

    SEMU_TEST_ASSERT(context, init_enabled(&fixture, program,
                                           sizeof(program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->s[0] = 0x7fc00001u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     armv7m_exec32(fixture.cpu, 0xecb0u, 0x0a00u,
                                   0x100u, &fixture.error));
    SEMU_TEST_EQ_U64(context, 0x100u, state->r[15]);
    SEMU_TEST_EQ_U64(context, 0x7fc00001u, state->s[0]);
    SEMU_TEST_EQ_U64(context, 0u, state->instructions);
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context, init_enabled(&fixture, program,
                                           sizeof(program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->s[31] = 0x80000000u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     armv7m_exec32(fixture.cpu, 0xecf0u, 0xfa02u,
                                   0x100u, &fixture.error));
    SEMU_TEST_EQ_U64(context, 0x100u, state->r[15]);
    SEMU_TEST_EQ_U64(context, 0x80000000u, state->s[31]);
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context, init_enabled(&fixture, program,
                                           sizeof(program)));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     armv7m_exec32(fixture.cpu, 0xeee1u, 0x0a20u,
                                   0x100u, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     armv7m_exec32(fixture.cpu, 0xed90u, 0x0b00u,
                                   0x100u, &fixture.error));
    SEMU_TEST_EQ_U64(context, 0x100u,
                     semu_cpu_get_state(fixture.cpu)->r[15]);
    SEMU_TEST_ASSERT(context, !semu_cpu_fault_address(fixture.cpu, &value));
    semu_cpu_fixture_destroy(&fixture);
}

static void test_memory_preflight_is_atomic(semu_test_context *context)
{
    static const uint8_t program[] = {0x00u, 0xbeu};
    semu_cpu_fixture fixture;
    semu_cpu_state *state;
    uint32_t value;
    unsigned index;

    SEMU_TEST_ASSERT(context, init_enabled(&fixture, program,
                                           sizeof(program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[1] = 0xff4u;
    state->s[28] = 0x11111111u;
    state->s[29] = 0x22222222u;
    state->s[30] = 0x33333333u;
    state->s[31] = 0x44444444u;
    for (index = 0u; index < 3u; ++index)
        SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(
            &fixture, 0xff4u + index * 4u, 0xdead0000u + index));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
                     armv7m_exec32(fixture.cpu, 0xeca1u, 0xea04u,
                                   0x100u, &fixture.error));
    SEMU_TEST_EQ_U64(context, 0xff4u, state->r[1]);
    SEMU_TEST_EQ_U64(context, 0x11111111u, state->s[28]);
    SEMU_TEST_EQ_U64(context, 0x22222222u, state->s[29]);
    SEMU_TEST_EQ_U64(context, 0x33333333u, state->s[30]);
    SEMU_TEST_EQ_U64(context, 0x44444444u, state->s[31]);
    SEMU_TEST_EQ_U64(context, 0x100u, state->r[15]);
    for (index = 0u; index < 3u; ++index) {
        SEMU_TEST_ASSERT(context, read_word(&fixture,
                                            0xff4u + index * 4u, &value));
        SEMU_TEST_EQ_U64(context, 0xdead0000u + index, value);
    }
    SEMU_TEST_ASSERT(context, semu_cpu_fault_address(fixture.cpu, &value));
    SEMU_TEST_EQ_U64(context, 0x1000u, value);
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_reset_access_and_system_registers),
        SEMU_TEST_CASE(test_nocp_disabled_and_partial_access),
        SEMU_TEST_CASE(test_scalar_and_pair_raw_transfers),
        SEMU_TEST_CASE(test_fpscr_and_nzcv_transfers),
        SEMU_TEST_CASE(test_single_memory_bounds_and_multiple_writeback),
        SEMU_TEST_CASE(test_refusals_are_bounded),
        SEMU_TEST_CASE(test_memory_preflight_is_atomic)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
