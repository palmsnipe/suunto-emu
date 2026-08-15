#include "semu/trace.h"
#include "semu/cpu.h"
#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "test.h"

#include <string.h>

/* Provided by src/core/report_cpu.c (not in trace.h to avoid cpu.h
   dependency in the trace contract header). */
void semu_report_fault_from_cpu(semu_report_fault *report,
    const semu_cpu *cpu);

static semu_report_fault make_fault(semu_stop_reason reason,
    uint32_t pc, uint32_t instr, int has_addr, uint32_t addr,
    uint64_t instructions)
{
    semu_report_fault f;
    uint32_t i;
    memset(&f, 0, sizeof(f));
    f.stop_reason = reason;
    f.fault_instruction = instr;
    f.has_fault_address = has_addr;
    f.fault_address = addr;
    f.instructions = instructions;
    for (i = 0u; i < 16u; ++i) {
        f.r[i] = i * 0x11111111u;
    }
    f.r[15] = pc;
    f.xpsr = 0x21000000u;
    f.primask = 0u;
    f.basepri = 0u;
    f.faultmask = 0u;
    f.control = 0u;
    f.fpscr = 0u;
    return f;
}

static void test_stop_classes_distinguishable(semu_test_context *context)
{
    semu_stop_reason reasons[] = {
        SEMU_STOP_HALT,
        SEMU_STOP_BUDGET,
        SEMU_STOP_WFI_DEADLOCK,
        SEMU_STOP_UNMAPPED_ACCESS,
        SEMU_STOP_UNSUPPORTED_INSTRUCTION,
        SEMU_STOP_DEVICE_REFUSED,
        SEMU_STOP_FIRMWARE_ASSERT,
        SEMU_STOP_COMPAT_REFUSED
    };
    char outputs[8][512];
    size_t lens[8];
    uint32_t i, j;

    for (i = 0u; i < 8u; ++i) {
        semu_report_fault f = make_fault(reasons[i], 0x1000u + i,
            0xDEAD0000u + i, 1, 0x40000000u + i, 100u + (uint64_t)i);
        lens[i] = semu_report_fault_format(&f, NULL, outputs[i],
                                           sizeof(outputs[i]));
        SEMU_TEST_ASSERT(context, lens[i] > 0u);
    }
    /* Every pair of stop classes must produce different output. */
    for (i = 0u; i < 8u; ++i) {
        for (j = i + 1u; j < 8u; ++j) {
            SEMU_TEST_ASSERT(context, lens[i] != lens[j] ||
                memcmp(outputs[i], outputs[j], lens[i]) != 0);
        }
    }
}

static void test_byte_identical_repeat(semu_test_context *context)
{
    semu_report_fault f = make_fault(SEMU_STOP_UNSUPPORTED_INSTRUCTION,
        0x00001000u, 0xF7F0A000u, 1, 0x40000000u, 12345u);
    char buf1[512];
    char buf2[512];
    size_t len1, len2;

    len1 = semu_report_fault_format(&f, NULL, buf1, sizeof(buf1));
    len2 = semu_report_fault_format(&f, NULL, buf2, sizeof(buf2));
    SEMU_TEST_EQ_U64(context, len1, len2);
    SEMU_TEST_ASSERT(context, memcmp(buf1, buf2, len1) == 0);
}

static void test_byte_identical_separate_traces(semu_test_context *context)
{
    semu_error err;
    semu_trace *t1, *t2;
    semu_report_fault f;
    char buf1[2048];
    char buf2[2048];
    size_t len1, len2;
    uint32_t i;

    semu_error_clear(&err);
    t1 = semu_trace_create(16u, SEMU_TRACE_OVERFLOW_STOP, &err);
    t2 = semu_trace_create(16u, SEMU_TRACE_OVERFLOW_STOP, &err);
    SEMU_TEST_ASSERT(context, t1 != NULL);
    SEMU_TEST_ASSERT(context, t2 != NULL);
    for (i = 0u; i < 5u; ++i) {
        semu_trace_record r;
        memset(&r, 0, sizeof(r));
        r.schema_version = SEMU_TRACE_SCHEMA_VERSION;
        r.kind = SEMU_TRACE_KIND_MMIO_READ;
        r.virtual_time_ns = 100u * (uint64_t)i;
        r.pc = 0x1000u + i;
        r.addr = 0x40000000u + i;
        r.value = i;
        r.width = 4u;
        semu_trace_append(t1, &r, &err);
        semu_error_clear(&err);
        semu_trace_append(t2, &r, &err);
        semu_error_clear(&err);
    }

    f = make_fault(SEMU_STOP_DEVICE_REFUSED, 0x2000u, 0x1234u, 0, 0u, 500u);
    len1 = semu_report_fault_format(&f, t1, buf1, sizeof(buf1));
    len2 = semu_report_fault_format(&f, t2, buf2, sizeof(buf2));
    SEMU_TEST_EQ_U64(context, len1, len2);
    SEMU_TEST_ASSERT(context, memcmp(buf1, buf2, len1) == 0);

    semu_trace_destroy(t1);
    semu_trace_destroy(t2);
}

static void test_no_fault_address(semu_test_context *context)
{
    semu_report_fault f = make_fault(SEMU_STOP_BUDGET, 0x1000u, 0u, 0, 0u,
                                     1000u);
    char buf[512];
    size_t len;

    len = semu_report_fault_format(&f, NULL, buf, sizeof(buf));
    SEMU_TEST_ASSERT(context, len > 0u);
    SEMU_TEST_ASSERT(context, strstr(buf, "fault_address=none") != NULL);
}

static void test_has_fault_address(semu_test_context *context)
{
    semu_report_fault f = make_fault(SEMU_STOP_UNMAPPED_ACCESS, 0x1000u, 0u,
                                     1, 0x40001234u, 1000u);
    char buf[512];
    size_t len;

    len = semu_report_fault_format(&f, NULL, buf, sizeof(buf));
    SEMU_TEST_ASSERT(context, len > 0u);
    SEMU_TEST_ASSERT(context, strstr(buf, "fault_address=0x40001234") != NULL);
}

static void test_history_truncation(semu_test_context *context)
{
    semu_error err;
    semu_trace *t;
    semu_report_fault f = make_fault(SEMU_STOP_UNSUPPORTED_INSTRUCTION,
        0x1000u, 0xDEADu, 0, 0u, 50u);
    char buf[4096];
    size_t len;
    uint32_t i;

    semu_error_clear(&err);
    t = semu_trace_create(4u, SEMU_TRACE_OVERFLOW_TRUNCATE, &err);
    SEMU_TEST_ASSERT(context, t != NULL);
    for (i = 0u; i < 10u; ++i) {
        semu_trace_record r;
        memset(&r, 0, sizeof(r));
        r.schema_version = SEMU_TRACE_SCHEMA_VERSION;
        r.kind = SEMU_TRACE_KIND_INSTRUCTION;
        r.virtual_time_ns = (uint64_t)i;
        r.pc = i;
        r.width = 2u;
        semu_trace_append(t, &r, &err);
        semu_error_clear(&err);
    }
    SEMU_TEST_ASSERT(context, semu_trace_overflowed(t));
    SEMU_TEST_EQ_U64(context, 4u, semu_trace_count(t));

    /* Report should still format successfully with truncated history. */
    len = semu_report_fault_format(&f, t, buf, sizeof(buf));
    SEMU_TEST_ASSERT(context, len > 0u);
    SEMU_TEST_ASSERT(context, strstr(buf, "--- trace ---") != NULL);
    /* Only 4 records survive; oldest surviving sequence is 6. */
    SEMU_TEST_ASSERT(context, strstr(buf, "seq=6") != NULL);
    SEMU_TEST_ASSERT(context, strstr(buf, "seq=0") == NULL);

    semu_trace_destroy(t);
}

static void test_no_host_state_in_output(semu_test_context *context)
{
    semu_report_fault f;
    char buf[512];
    size_t len;

    memset(&f, 0, sizeof(f));
    f.stop_reason = SEMU_STOP_FIRMWARE_ASSERT;
    f.fault_instruction = 0xABCDu;
    f.has_fault_address = 1;
    f.fault_address = 0x50000000u;
    f.instructions = 999u;
    f.r[15] = 0x1000u;

    len = semu_report_fault_format(&f, NULL, buf, sizeof(buf));
    SEMU_TEST_ASSERT(context, len > 0u);
    /* No path separators or typical host pointer-range hex prefixes. */
    SEMU_TEST_ASSERT(context, strstr(buf, "/") == NULL);
    SEMU_TEST_ASSERT(context, strstr(buf, "0x7fff") == NULL);
    SEMU_TEST_ASSERT(context, strstr(buf, "0x6000") == NULL);
}

static void test_from_cpu(semu_test_context *context)
{
    static const uint8_t program[] = { 0x00u, 0xBFu }; /* NOP (Thumb) */
    semu_cpu_fixture fixture;
    semu_cpu_state state;
    semu_report_fault report;
    char buf[1024];
    size_t len;

    SEMU_TEST_ASSERT(context,
        semu_cpu_fixture_init(&fixture, program, sizeof(program)));
    memset(&state, 0, sizeof(state));
    state.r[0] = 0x12345678u;
    state.r[1] = 0xDEADBEEFu;
    state.r[15] = 0x00000100u;
    state.xpsr = 0x21000003u;
    state.primask = 0x01u;
    state.basepri = 0x00u;
    state.faultmask = 0x00u;
    state.control = 0x00u;
    state.fpscr = 0x00000000u;
    state.instructions = 42u;
    semu_cpu_fixture_apply_state(&fixture, &state);

    memset(&report, 0, sizeof(report));
    semu_report_fault_from_cpu(&report, fixture.cpu);

    SEMU_TEST_EQ_U64(context, SEMU_STOP_NONE, report.stop_reason);
    SEMU_TEST_EQ_U64(context, 0x12345678u, report.r[0]);
    SEMU_TEST_EQ_U64(context, 0xDEADBEEFu, report.r[1]);
    SEMU_TEST_EQ_U64(context, 0x00000100u, report.r[15]);
    SEMU_TEST_EQ_U64(context, 0x21000003u, report.xpsr);
    SEMU_TEST_EQ_U64(context, 0x01u, report.primask);
    SEMU_TEST_EQ_U64(context, 42u, report.instructions);

    /* Format must succeed and be byte-identical on repeat. */
    {
        char buf2[1024];
        size_t len2;
        len = semu_report_fault_format(&report, NULL, buf, sizeof(buf));
        len2 = semu_report_fault_format(&report, NULL, buf2, sizeof(buf2));
        SEMU_TEST_ASSERT(context, len > 0u);
        SEMU_TEST_EQ_U64(context, len, len2);
        SEMU_TEST_ASSERT(context, memcmp(buf, buf2, len) == 0);
    }
    semu_cpu_fixture_destroy(&fixture);
}

static void test_from_cpu_null_safety(semu_test_context *context)
{
    semu_report_fault report;
    char buf[64];

    memset(&report, 0, sizeof(report));
    /* from_cpu with NULL pointers must not crash. */
    semu_report_fault_from_cpu(NULL, NULL);
    semu_report_fault_from_cpu(&report, NULL);
    SEMU_TEST_EQ_U64(context, 0u, report.stop_reason);

    /* Format with NULL report produces empty. */
    SEMU_TEST_EQ_U64(context, 0u,
        semu_report_fault_format(NULL, NULL, buf, sizeof(buf)));
    SEMU_TEST_EQ_U64(context, 0u,
        semu_report_fault_format(NULL, NULL, NULL, 0u));
}

static void test_trace_null_omitted(semu_test_context *context)
{
    semu_report_fault f = make_fault(SEMU_STOP_HALT, 0x8000u, 0u, 0, 0u,
                                     0u);
    char buf[512];
    size_t len;

    len = semu_report_fault_format(&f, NULL, buf, sizeof(buf));
    SEMU_TEST_ASSERT(context, len > 0u);
    /* No trace section when trace is NULL. */
    SEMU_TEST_ASSERT(context, strstr(buf, "--- trace ---") == NULL);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_stop_classes_distinguishable),
        SEMU_TEST_CASE(test_byte_identical_repeat),
        SEMU_TEST_CASE(test_byte_identical_separate_traces),
        SEMU_TEST_CASE(test_no_fault_address),
        SEMU_TEST_CASE(test_has_fault_address),
        SEMU_TEST_CASE(test_history_truncation),
        SEMU_TEST_CASE(test_no_host_state_in_output),
        SEMU_TEST_CASE(test_from_cpu),
        SEMU_TEST_CASE(test_from_cpu_null_safety),
        SEMU_TEST_CASE(test_trace_null_omitted)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
