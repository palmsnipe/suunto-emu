#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "test.h"

#include <stdio.h>

#define SCS 0xe000e000u
#define SCB_ICSR (SCS + 0xd04u)
#define PENDSVSET (1u << 28)

static int load_handler(semu_cpu_fixture *fixture, uint32_t vector,
                        uint32_t address, const uint8_t *bytes, size_t size)
{
    return semu_cpu_fixture_load_u32(fixture, vector * 4u, address | 1u) &&
           semu_bus_load(fixture->bus, address, bytes, size,
                         &fixture->error) == SEMU_OK;
}

static void test_svc_pendsv_order_and_return(semu_test_context *context)
{
    static const uint8_t program[] = {0x01u, 0xdfu, 0x00u, 0xbeu};
    static const uint8_t svc_handler[] = {
        0x02u, 0x48u,             /* ldr r0,[pc,#8] */
        0x01u, 0x21u,             /* movs r1,#1 */
        0x09u, 0x07u,             /* lsls r1,r1,#28 */
        0x01u, 0x60u,             /* str r1,[r0,#0] */
        0x70u, 0x47u,             /* bx lr */
        0x00u, 0xbfu,             /* alignment nop */
        0x04u, 0xedu, 0x00u, 0xe0u
    };
    static const uint8_t pendsv_handler[] = {
        0x02u, 0x48u,             /* ldr r0,[pc,#8] */
        0x2au, 0x21u,             /* movs r1,#42 */
        0x01u, 0x60u,             /* str r1,[r0,#0] */
        0x70u, 0x47u,             /* bx lr */
        0x00u, 0xbfu, 0x00u, 0xbfu,
        0x00u, 0x07u, 0x00u, 0x00u
    };
    semu_cpu_fixture fixture;
    semu_cpu_state *state;
    uint32_t marker;
    unsigned order[2];
    unsigned count = 0u;
    unsigned step;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program, sizeof(program)));
    SEMU_TEST_ASSERT(context, load_handler(&fixture, 11u, 0x200u,
                                           svc_handler, sizeof(svc_handler)));
    SEMU_TEST_ASSERT(context, load_handler(&fixture, 14u, 0x220u,
                                           pendsv_handler,
                                           sizeof(pendsv_handler)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    for (step = 0u; step < 16u; ++step) {
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
        if ((state->xpsr & 0x1ffu) == 11u && count == 0u)
            order[count++] = 11u;
        if ((state->xpsr & 0x1ffu) == 14u && count == 1u)
            order[count++] = 14u;
        if (state->halted) break;
    }
    SEMU_TEST_EQ_U64(context, 2u, count);
    SEMU_TEST_EQ_U64(context, 11u, order[0]);
    SEMU_TEST_EQ_U64(context, 14u, order[1]);
    SEMU_TEST_ASSERT(context,
                     semu_bus_read(fixture.bus, 0x700u, 4u, &marker,
                                   &fixture.error) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 42u, marker);
    SEMU_TEST_EQ_U64(context, 11u, semu_scheduler_now(fixture.scheduler));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
                     semu_cpu_stop_reason(fixture.cpu));
    (void)fprintf(stdout, "transcript SVC(11)->PendSV(14), virtual_time=11ns\n");
    semu_cpu_fixture_destroy(&fixture);
}

static void test_unprivileged_pendsv_refusal(semu_test_context *context)
{
    static const uint8_t program[] = {0x00u, 0xbeu};
    semu_cpu_fixture fixture;
    semu_cpu_state *state;
    uint32_t before_xpsr;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program, sizeof(program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->control = 1u;
    before_xpsr = state->xpsr;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_write(fixture.bus, SCB_ICSR, 4u, PENDSVSET,
                                    &fixture.error));
    SEMU_TEST_EQ_U64(context, before_xpsr, state->xpsr);
    SEMU_TEST_EQ_U64(context, 0u, state->waiting_for_interrupt);
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_svc_pendsv_order_and_return),
        SEMU_TEST_CASE(test_unprivileged_pendsv_refusal)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
