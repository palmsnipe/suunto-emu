#include "semu/cpu.h"
#include "cpu_fixture.h"
#include "cpu_fixture.c"

#include <stdio.h>

#define CHECK(condition) do {                                                \
    if (!(condition)) {                                                      \
        (void)fprintf(stderr, "%s:%d: check failed: %s\n",                  \
                      __FILE__, __LINE__, #condition);                       \
        return 0;                                                            \
    }                                                                        \
} while (0)

typedef semu_cpu_fixture cpu_fixture;
#define fixture_init semu_cpu_fixture_init
#define fixture_destroy semu_cpu_fixture_destroy
#define load_u32 semu_cpu_fixture_load_u32
#define step_ok(fixture) (semu_cpu_fixture_step(fixture) == SEMU_OK)
static int test_reset_arithmetic_and_branch(void)
{
    static const uint8_t program[] = {
        0x01u, 0x20u,             /* movs r0,#1 */
        0x02u, 0x30u,             /* adds r0,#2 */
        0x03u, 0x28u,             /* cmp r0,#3 */
        0x01u, 0xd1u,             /* bne (not taken) */
        0x07u, 0x21u,             /* movs r1,#7 */
        0x00u, 0xe0u,             /* b over next instruction */
        0x09u, 0x21u,             /* movs r1,#9 */
        0x00u, 0xbeu              /* bkpt */
    };
    cpu_fixture fixture;
    const semu_cpu_state *state;
    unsigned index;

    CHECK(fixture_init(&fixture, program, sizeof(program)));
    state = semu_cpu_get_state(fixture.cpu);
    CHECK(state->r[13] == 0x800u);
    CHECK(state->r[15] == 0x100u);
    CHECK((state->xpsr & (1u << 24)) != 0u);
    for (index = 0u; index < 7u; ++index) {
        CHECK(step_ok(&fixture));
    }
    state = semu_cpu_get_state(fixture.cpu);
    CHECK(state->r[0] == 3u);
    CHECK(state->r[1] == 7u);
    CHECK(state->instructions == 7u);
    CHECK(state->halted);
    CHECK(semu_cpu_stop_reason(fixture.cpu) == SEMU_STOP_HALT);
    fixture_destroy(&fixture);
    return 1;
}
static int test_load_store(void)
{
    static const uint8_t program[] = {
        0x08u, 0x60u,             /* str r0,[r1,#0] */
        0x0au, 0x68u,             /* ldr r2,[r1,#0] */
        0x00u, 0xbeu
    };
    cpu_fixture fixture;
    semu_cpu_state *state;
    uint32_t memory;

    CHECK(fixture_init(&fixture, program, sizeof(program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = 0xa55aa55au;
    state->r[1] = 0x600u;
    CHECK(step_ok(&fixture));
    CHECK(step_ok(&fixture));
    CHECK(state->r[2] == 0xa55aa55au);
    CHECK(semu_bus_read(fixture.bus, 0x600u, 4u, &memory,
                        &fixture.error) == SEMU_OK);
    CHECK(memory == 0xa55aa55au);
    fixture_destroy(&fixture);
    return 1;
}
static int test_wfi_deadlock(void)
{
    static const uint8_t program[] = {0x30u, 0xbfu};
    cpu_fixture fixture;
    const semu_cpu_state *state;

    CHECK(fixture_init(&fixture, program, sizeof(program)));
    CHECK(step_ok(&fixture));
    state = semu_cpu_get_state(fixture.cpu);
    CHECK(state->waiting_for_interrupt);
    CHECK(!state->halted);
    CHECK(step_ok(&fixture));
    CHECK(state->halted);
    CHECK(semu_cpu_stop_reason(fixture.cpu) == SEMU_STOP_WFI_DEADLOCK);
    fixture_destroy(&fixture);
    return 1;
}
static int test_unsupported_instruction(void)
{
    static const uint8_t program[] = {0x00u, 0xdeu};
    cpu_fixture fixture;
    semu_status status;

    CHECK(fixture_init(&fixture, program, sizeof(program)));
    status = semu_cpu_step(fixture.cpu, &fixture.error);
    CHECK(status == SEMU_ERR_UNSUPPORTED);
    CHECK(semu_cpu_stop_reason(fixture.cpu) ==
          SEMU_STOP_UNSUPPORTED_INSTRUCTION);
    CHECK(semu_cpu_fault_instruction(fixture.cpu) == 0xde00u);
    CHECK(semu_cpu_get_state(fixture.cpu)->instructions == 0u);
    fixture_destroy(&fixture);
    return 1;
}
static int test_mov_w_sp_regression(void)
{
    static const uint8_t program[] = {
        0x4fu, 0xeau, 0x0du, 0x00u, /* MOV.W r0,sp: EA4F 000D */
        0x00u, 0xbeu
    };
    cpu_fixture fixture;

    CHECK(fixture_init(&fixture, program, sizeof(program)));
    CHECK(step_ok(&fixture));
    CHECK(semu_cpu_get_state(fixture.cpu)->r[0] == 0x800u);
    CHECK(semu_cpu_get_state(fixture.cpu)->r[15] == 0x104u);
    fixture_destroy(&fixture);
    return 1;
}
static int test_stmdb_sp_regression(void)
{
    static const uint8_t program[] = {
        0x2du, 0xe9u, 0xf8u, 0x41u, /* STMDB sp!,{r3-r8,lr} */
        0x00u, 0xbeu
    };
    cpu_fixture fixture;
    semu_cpu_state *state;
    uint32_t value;
    unsigned reg;

    CHECK(fixture_init(&fixture, program, sizeof(program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    for (reg = 3u; reg <= 8u; ++reg) {
        state->r[reg] = 0x1000u + reg;
    }
    state->r[14] = 0xfeed0001u;
    CHECK(step_ok(&fixture));
    CHECK(state->r[13] == 0x7e4u);
    for (reg = 3u; reg <= 8u; ++reg) {
        CHECK(semu_bus_read(fixture.bus, 0x7e4u + (reg - 3u) * 4u,
                            4u, &value, &fixture.error) == SEMU_OK);
        CHECK(value == 0x1000u + reg);
    }
    CHECK(semu_bus_read(fixture.bus, 0x7fcu, 4u, &value,
                        &fixture.error) == SEMU_OK);
    CHECK(value == 0xfeed0001u);
    fixture_destroy(&fixture);
    return 1;
}
static int test_svc_exception_return(void)
{
    static const uint8_t program[] = {0x01u, 0xdfu, 0x00u, 0xbeu};
    static const uint8_t handler[] = {
        0x09u, 0x22u,             /* movs r2,#9 */
        0x70u, 0x47u              /* bx lr */
    };
    cpu_fixture fixture;

    CHECK(fixture_init(&fixture, program, sizeof(program)));
    CHECK(load_u32(&fixture, 11u * 4u, 0x181u));
    CHECK(semu_bus_load(fixture.bus, 0x180u, handler, sizeof(handler),
                        &fixture.error) == SEMU_OK);
    CHECK(step_ok(&fixture));
    CHECK((semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu) == 11u);
    CHECK(step_ok(&fixture));
    CHECK(semu_cpu_get_state(fixture.cpu)->r[2] == 9u);
    CHECK(step_ok(&fixture));
    CHECK((semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu) == 0u);
    CHECK(semu_cpu_get_state(fixture.cpu)->r[2] == 0u);
    CHECK(semu_cpu_get_state(fixture.cpu)->r[15] == 0x102u);
    CHECK(step_ok(&fixture));
    CHECK(semu_cpu_stop_reason(fixture.cpu) == SEMU_STOP_HALT);
    fixture_destroy(&fixture);
    return 1;
}
static int test_level_irq(void)
{
    static const uint8_t program[] = {0x03u, 0x20u, 0x00u, 0xbeu};
    static const uint8_t handler[] = {0x07u, 0x24u, 0x70u, 0x47u};
    cpu_fixture fixture;

    CHECK(fixture_init(&fixture, program, sizeof(program)));
    CHECK(load_u32(&fixture, 16u * 4u, 0x1a1u));
    CHECK(semu_bus_load(fixture.bus, 0x1a0u, handler, sizeof(handler),
                        &fixture.error) == SEMU_OK);
    semu_cpu_set_irq(fixture.cpu, 0u, 1);
    CHECK(step_ok(&fixture));
    CHECK(semu_cpu_get_state(fixture.cpu)->instructions == 0u);
    CHECK((semu_cpu_get_state(fixture.cpu)->xpsr & 0x1ffu) == 16u);
    semu_cpu_set_irq(fixture.cpu, 0u, 0);
    CHECK(step_ok(&fixture));
    CHECK(step_ok(&fixture));
    CHECK(step_ok(&fixture));
    CHECK(semu_cpu_get_state(fixture.cpu)->r[0] == 3u);
    CHECK(semu_cpu_get_state(fixture.cpu)->r[4] == 7u);
    fixture_destroy(&fixture);
    return 1;
}
static int test_msr_psp(void)
{
    static const uint8_t program[] = {
        0x80u, 0xf3u, 0x09u, 0x88u, /* msr psp,r0 */
        0x00u, 0xbeu
    };
    cpu_fixture fixture;

    CHECK(fixture_init(&fixture, program, sizeof(program)));
    semu_cpu_get_state_mutable(fixture.cpu)->r[0] = 0x10001234u;
    CHECK(step_ok(&fixture));
    CHECK(semu_cpu_get_state(fixture.cpu)->psp == 0x10001234u);
    fixture_destroy(&fixture);
    return 1;
}
static int test_orr_modified_immediate(void)
{
    static const uint8_t program[] = {
        0x40u, 0xf4u, 0x70u, 0x00u, /* orr.w r0,r0,#0x00f00000 */
        0x00u, 0xbeu
    };
    cpu_fixture fixture;

    CHECK(fixture_init(&fixture, program, sizeof(program)));
    semu_cpu_get_state_mutable(fixture.cpu)->r[0] = 3u;
    CHECK(step_ok(&fixture));
    CHECK(semu_cpu_get_state(fixture.cpu)->r[0] == 0x00f00003u);
    fixture_destroy(&fixture);
    return 1;
}
static int test_fpscr_transfer(void)
{
    static const uint8_t program[] = {
        0xe1u, 0xeeu, 0x10u, 0x0au, /* vmsr fpscr,r0 */
        0xf1u, 0xeeu, 0x10u, 0x1au, /* vmrs r1,fpscr */
        0x00u, 0xbeu
    };
    cpu_fixture fixture;

    CHECK(fixture_init(&fixture, program, sizeof(program)));
    CHECK(semu_bus_write(fixture.bus, 0xe000ed88u, 4u, 0x00f00000u,
                         &fixture.error) == SEMU_OK);
    semu_cpu_get_state_mutable(fixture.cpu)->r[0] = 0x03400000u;
    CHECK(step_ok(&fixture));
    CHECK(step_ok(&fixture));
    CHECK(semu_cpu_get_state(fixture.cpu)->fpscr == 0x03400000u);
    CHECK(semu_cpu_get_state(fixture.cpu)->r[1] == 0x03400000u);
    fixture_destroy(&fixture);
    return 1;
}
static int test_indexed_word_load(void)
{
    static const uint8_t program[] = {
        0x50u, 0xf8u, 0x04u, 0x1bu, /* ldr.w r1,[r0],#4 */
        0x00u, 0xbeu
    };
    cpu_fixture fixture;

    CHECK(fixture_init(&fixture, program, sizeof(program)));
    CHECK(load_u32(&fixture, 0x600u, 0x12345678u));
    semu_cpu_get_state_mutable(fixture.cpu)->r[0] = 0x600u;
    CHECK(step_ok(&fixture));
    CHECK(semu_cpu_get_state(fixture.cpu)->r[1] == 0x12345678u);
    CHECK(semu_cpu_get_state(fixture.cpu)->r[0] == 0x604u);
    fixture_destroy(&fixture);
    return 1;
}
int main(void)
{
    static int (*const tests[])(void) = {
        test_reset_arithmetic_and_branch,
        test_load_store,
        test_wfi_deadlock,
        test_unsupported_instruction,
        test_mov_w_sp_regression,
        test_stmdb_sp_regression,
        test_svc_exception_return,
        test_level_irq,
        test_msr_psp,
        test_orr_modified_immediate,
        test_fpscr_transfer,
        test_indexed_word_load
    };
    size_t index;

    for (index = 0u; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        if (!tests[index]()) {
            return 1;
        }
    }
    (void)printf("cpu tests: %lu passed\n", (unsigned long)index);
    return 0;
}
