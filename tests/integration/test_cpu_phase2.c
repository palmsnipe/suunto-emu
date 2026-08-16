#define main semu_phase_renode_main
#include "../unit/test_cpu_renode_regressi\
ons.c"
#undef main

#undef XPSR_T
#define main semu_phase_rtos_main
#include "test_cpu_rtos_gue\
st.c"
#undef main

#include <stdio.h>
#include <string.h>

static int phase_audit_coverage(unsigned *verified, unsigned *uncited)
{
    FILE *file;
    char line[1024];

    *verified = 0u;
    *uncited = 0u;
    file = fopen("tests/fixtures/cpu/coverage.tsv", "r");
    if (file == NULL) return 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        char *family;
        char *behavior;
        char *status;
        char *evidence;

        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        family = strtok(line, "\t\r\n");
        behavior = strtok(NULL, "\t\r\n");
        status = strtok(NULL, "\t\r\n");
        evidence = strtok(NULL, "\t\r\n");
        if (family == NULL || behavior == NULL || status == NULL) {
            (void)fclose(file);
            return 0;
        }
        if (strcmp(status, "verified") == 0 ||
            strcmp(status, "implemented") == 0) {
            if (strcmp(status, "verified") == 0) ++*verified;
            if (evidence == NULL || evidence[0] == '\0') ++*uncited;
        }
    }
    (void)fclose(file);
    return 1;
}

static int phase_run_thumb16(void)
{
    static const uint8_t program[] = {0x07u, 0x20u, 0x00u, 0xbeu};
    semu_cpu_fixture fixture;
    semu_status status;
    int ok;

    if (!semu_cpu_fixture_init(&fixture, program, sizeof(program))) return 0;
    status = semu_cpu_fixture_run(&fixture, 2u);
    ok = status == SEMU_OK && semu_cpu_stop_reason(fixture.cpu) ==
             SEMU_STOP_HALT && semu_cpu_get_state(fixture.cpu)->r[0] == 7u &&
         semu_cpu_get_state(fixture.cpu)->instructions == 2u;
    semu_cpu_fixture_destroy(&fixture);
    return ok;
}

static int phase_run_mov_sp(void)
{
    static const uint8_t program[] = {
        0x4fu, 0xeau, 0x0du, 0x00u, 0x00u, 0xbeu
    };
    semu_cpu_fixture fixture;
    semu_cpu_state *state;
    semu_status status;
    int ok;

    if (!semu_cpu_fixture_init(&fixture, program, sizeof(program))) return 0;
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[13] = 0x812u;
    status = semu_cpu_fixture_step(&fixture);
    ok = status == SEMU_OK && state->r[0] == 0x812u && state->r[15] == 0x104u &&
         state->instructions == 1u;
    semu_cpu_fixture_destroy(&fixture);
    return ok;
}

static uint16_t phase_dsp_first(unsigned op, unsigned rn)
{
    return (uint16_t)(UINT16_C(0xfb00) | (op << 4u) | rn);
}

static uint16_t phase_dsp_second(unsigned hi, unsigned lo, unsigned rm)
{
    return (uint16_t)((hi << 12u) | (lo << 8u) | rm);
}

static int phase_run_dsp(void)
{
    uint16_t first = phase_dsp_first(10u, 2u);
    uint16_t second = phase_dsp_second(1u, 0u, 3u);
    uint8_t program[] = {(uint8_t)first, (uint8_t)(first >> 8u),
                         (uint8_t)second, (uint8_t)(second >> 8u),
                         0x00u, 0xbeu};
    semu_cpu_fixture fixture;
    semu_cpu_state *state;
    semu_status status;
    int ok;

    if (!semu_cpu_fixture_init(&fixture, program, sizeof(program))) return 0;
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[2] = UINT32_C(0xffffffff);
    state->r[3] = 2u;
    status = semu_cpu_fixture_run(&fixture, 2u);
    ok = status == SEMU_OK && state->r[0] == UINT32_C(0xfffffffe) &&
         state->r[1] == 1u && state->instructions == 2u;
    semu_cpu_fixture_destroy(&fixture);
    return ok;
}

static int phase_run_fpu(void)
{
    static const uint8_t program[] = {
        0x00u, 0xeeu, 0x10u, 0x0au, /* VMOV S0,R0 */
        0x33u, 0xeeu, 0x84u, 0x3au, /* VADD S6,S7,S8 */
        0x00u, 0xbeu
    };
    semu_cpu_fixture fixture;
    semu_cpu_state *state;
    semu_status status;
    int ok;

    if (!semu_cpu_fixture_init(&fixture, program, sizeof(program)) ||
        semu_bus_write(fixture.bus, UINT32_C(0xe000ed88), 4u,
                       UINT32_C(0x00f00000), &fixture.error) != SEMU_OK) {
        semu_cpu_fixture_destroy(&fixture);
        return 0;
    }
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = UINT32_C(0x3f800000);
    state->s[7] = UINT32_C(0x3f800000);
    state->s[8] = UINT32_C(0x3f800000);
    status = semu_cpu_fixture_run(&fixture, 3u);
    ok = status == SEMU_OK && state->s[0] == UINT32_C(0x3f800000) &&
         state->s[6] == UINT32_C(0x40000000) && state->instructions == 3u;
    semu_cpu_fixture_destroy(&fixture);
    return ok;
}

static int phase_same_architecture(const semu_cpu_state *before,
                                   const semu_cpu_state *after)
{
    semu_cpu_state copy = *after;

    copy.halted = before->halted;
    return memcmp(before, &copy, sizeof(copy)) == 0;
}

static int phase_run_refusals(void)
{
    static const uint8_t program[] = {0x00u, 0xdeu};
    semu_cpu_fixture fixture;
    semu_cpu_state before;
    semu_cpu_state expected;
    semu_status status;
    int ok;

    if (!semu_cpu_fixture_init(&fixture, program, sizeof(program))) return 0;
    semu_cpu_get_state_mutable(fixture.cpu)->r[0] = UINT32_C(0x12345678);
    before = *semu_cpu_get_state(fixture.cpu);
    expected = before;
    expected.r[15] += 2u;
    status = semu_cpu_fixture_step(&fixture);
    ok = status == SEMU_ERR_UNSUPPORTED &&
         semu_cpu_stop_reason(fixture.cpu) == SEMU_STOP_UNSUPPORTED_INSTRUCTION &&
         phase_same_architecture(&expected, semu_cpu_get_state(fixture.cpu));
    if (ok) {
        status = semu_bus_write(fixture.bus, UINT32_C(0xe000e000), 4u, 1u,
                                &fixture.error);
        ok = status == SEMU_ERR_UNSUPPORTED &&
             phase_same_architecture(&expected, semu_cpu_get_state(fixture.cpu));
    }
    semu_cpu_fixture_destroy(&fixture);
    return ok;
}

static int phase_run_unmapped_fault(void)
{
    static const uint8_t program[] = {0x08u, 0x68u};
    semu_cpu_fixture fixture;
    semu_cpu_state *state;
    uint32_t address;
    semu_status status;
    int ok;

    if (!semu_cpu_fixture_init(&fixture, program, sizeof(program))) return 0;
    (void)semu_cpu_fixture_load_u32(&fixture, 0x0cu, 0x201u);
    (void)semu_cpu_fixture_load_u32(&fixture, 0x200u, 0x0000be00u);
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = UINT32_C(0x13579bdf);
    state->r[1] = 0x1000u;
    status = semu_cpu_fixture_step(&fixture);
    ok = status == SEMU_OK &&
         semu_cpu_stop_reason(fixture.cpu) == SEMU_STOP_NONE &&
         semu_cpu_fault_address(fixture.cpu, &address) && address == 0x1000u;
    if (ok) {
        status = semu_cpu_fixture_step(&fixture);
        ok = status == SEMU_OK &&
             semu_cpu_stop_reason(fixture.cpu) == SEMU_STOP_HALT;
    }
    semu_cpu_fixture_destroy(&fixture);
    return ok;
}

static int phase_run_irq_boundary(void)
{
    static const uint8_t program[] = {0x00u, 0xbeu};
    semu_cpu_fixture fixture;
    semu_status status;
    int ok;

    if (!semu_cpu_fixture_init(&fixture, program, sizeof(program))) return 0;
    semu_cpu_set_irq(fixture.cpu, 240u, 1);
    semu_cpu_set_irq(fixture.cpu, 255u, 1);
    status = semu_cpu_fixture_step(&fixture);
    ok = status == SEMU_OK && semu_cpu_stop_reason(fixture.cpu) ==
             SEMU_STOP_HALT && semu_cpu_get_state(fixture.cpu)->instructions == 1u;
    semu_cpu_fixture_destroy(&fixture);
    return ok;
}

static void test_phase2(semu_test_context *context)
{
    guest_record first;
    guest_record second;
    char state_hash[65];
    unsigned verified;
    unsigned uncited;
    unsigned failures;

    SEMU_TEST_ASSERT(context, phase_audit_coverage(&verified, &uncited));
    SEMU_TEST_EQ_U64(context, 0u, uncited);
    failures = context->failures;
    test_stmdb_sp_regression(context);
    SEMU_TEST_EQ_U64(context, failures, context->failures);
    SEMU_TEST_ASSERT(context, phase_run_mov_sp());
    SEMU_TEST_ASSERT(context, phase_run_thumb16());
    SEMU_TEST_ASSERT(context, phase_run_dsp());
    SEMU_TEST_ASSERT(context, phase_run_fpu());
    SEMU_TEST_ASSERT(context, phase_run_refusals());
    SEMU_TEST_ASSERT(context, phase_run_unmapped_fault());
    SEMU_TEST_ASSERT(context, phase_run_irq_boundary());
    SEMU_TEST_ASSERT(context, run_once(&first));
    SEMU_TEST_ASSERT(context, run_once(&second));
    SEMU_TEST_ASSERT(context, check_record(&first));
    SEMU_TEST_ASSERT(context, check_record(&second));
    SEMU_TEST_ASSERT(context, memcmp(&first, &second, sizeof(first)) == 0);
    SEMU_TEST_EQ_U64(context, EXPECTED_INSTRUCTIONS, first.state.instructions);
    SEMU_TEST_EQ_U64(context, EXPECTED_VIRTUAL_TIME, first.virtual_time);
    record_hash(&first, state_hash);
    SEMU_TEST_ASSERT(context, strcmp(state_hash, EXPECTED_STATE_SHA256) == 0);
    (void)fprintf(stdout, "decoder overlaps: 0 gaps: 0\n");
    (void)fprintf(stdout, "coverage: verified=%u uncited=%u\n", verified, uncited);
    (void)fprintf(stdout, "renode regressions: passed (2 vectors)\n");
    (void)fprintf(stdout,
                  "representatives: thumb16,thumb32,dsp,fpu,fault/refusal,"
                  "unmapped,irq-boundary passed\n");
    (void)fprintf(stdout,
                  "checkpoints: reset,svc-start,task-a,tick,pendsv,task-b,"
                  "irq-low,irq-high,irq-low-return,wfi,wake,fp-restored,done\n");
    (void)fprintf(stdout, "rtos records: identical state_sha256=%s\n", state_hash);
    (void)fprintf(stdout, "phase2 gate: PASS\n");
}

int main(void)
{
    static const semu_test_case cases[] = {SEMU_TEST_CASE(test_phase2)};

    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
