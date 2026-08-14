#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "test.h"

#include <string.h>

/* TEST_TAGS: cpu_renode_regressions */

#define XPSR_T (1u << 24)

static void test_stmdb_sp_regression(semu_test_context *context)
{
    static const uint8_t program[] = {0x2du, 0xe9u, 0xf8u, 0x41u};
    semu_cpu_state state;
    semu_cpu_fixture fixture;
    semu_status status;
    uint32_t value;
    unsigned reg;

    (void)memset(&state, 0, sizeof(state));
    state.r[13] = 0x800u;
    state.r[15] = 0x100u;
    state.msp = 0x800u;
    state.xpsr = XPSR_T;
    for (reg = 3u; reg <= 8u; ++reg) state.r[reg] = 0x1000u + reg;
    state.r[14] = 0xfeed0001u;

    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, program,
                                                    sizeof(program)));
    semu_cpu_fixture_apply_state(&fixture, &state);
    status = semu_cpu_fixture_step(&fixture);
    SEMU_TEST_EQ_U64(context, SEMU_OK, status);
    SEMU_TEST_EQ_U64(context, 0x7e4u, semu_cpu_get_state(fixture.cpu)->r[13]);
    SEMU_TEST_EQ_U64(context, 0x104u, semu_cpu_get_state(fixture.cpu)->r[15]);
    for (reg = 0u; reg < 6u; ++reg) {
        SEMU_TEST_ASSERT(context, semu_bus_read(fixture.bus, 0x7e4u + reg * 4u,
                                                4u, &value, &fixture.error) ==
                         SEMU_OK);
        SEMU_TEST_EQ_U64(context, 0x1003u + reg, value);
    }
    SEMU_TEST_ASSERT(context, semu_bus_read(fixture.bus, 0x7fcu, 4u, &value,
                                            &fixture.error) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 0xfeed0001u, value);
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_stmdb_sp_regression)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
