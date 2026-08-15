#include "semu/trace.h"
#include "test.h"

#include <string.h>

static semu_trace_record make_record(uint32_t kind, uint64_t time,
    uint32_t pc, uint32_t addr, uint32_t value, uint32_t event, uint32_t width)
{
    semu_trace_record r;
    memset(&r, 0, sizeof(r));
    r.schema_version = SEMU_TRACE_SCHEMA_VERSION;
    r.kind = kind;
    r.virtual_time_ns = time;
    r.pc = pc;
    r.addr = addr;
    r.value = value;
    r.event_code = event;
    r.width = width;
    return r;
}

static semu_status append_simple(semu_trace *t, uint32_t kind, uint64_t time,
    uint32_t pc, uint32_t addr, uint32_t value, uint32_t event, uint32_t width,
    semu_error *err)
{
    semu_trace_record r = make_record(kind, time, pc, addr, value, event,
                                       width);
    return semu_trace_append(t, &r, err);
}

static void test_all_kinds(semu_test_context *context)
{
    semu_error err;
    semu_trace *t;
    const semu_trace_record *rec;
    uint32_t kinds[7] = {
        SEMU_TRACE_KIND_INSTRUCTION,
        SEMU_TRACE_KIND_MMIO_READ,
        SEMU_TRACE_KIND_MMIO_WRITE,
        SEMU_TRACE_KIND_DEVICE,
        SEMU_TRACE_KIND_COMPAT,
        SEMU_TRACE_KIND_INPUT,
        SEMU_TRACE_KIND_FRAME
    };
    uint32_t i;

    semu_error_clear(&err);
    t = semu_trace_create(16u, SEMU_TRACE_OVERFLOW_STOP, &err);
    SEMU_TEST_ASSERT(context, t != NULL);
    for (i = 0u; i < 7u; ++i) {
        semu_status s = append_simple(t, kinds[i], 100u * (uint64_t)i,
            0x1000u + i, 0x2000u, 0x3000u + i, i, 4u, &err);
        SEMU_TEST_EQ_U64(context, SEMU_OK, s);
    }
    SEMU_TEST_EQ_U64(context, 7u, semu_trace_count(t));
    for (i = 0u; i < 7u; ++i) {
        rec = semu_trace_get(t, (size_t)i);
        SEMU_TEST_ASSERT(context, rec != NULL);
        SEMU_TEST_EQ_U64(context, kinds[i], rec->kind);
        SEMU_TEST_EQ_U64(context, i, rec->sequence);
        SEMU_TEST_EQ_U64(context, SEMU_TRACE_SCHEMA_VERSION,
            rec->schema_version);
    }
    semu_trace_destroy(t);
}

static void test_same_time_ordering(semu_test_context *context)
{
    semu_error err;
    semu_trace *t;
    const semu_trace_record *rec;
    uint32_t i;

    semu_error_clear(&err);
    t = semu_trace_create(16u, SEMU_TRACE_OVERFLOW_STOP, &err);
    SEMU_TEST_ASSERT(context, t != NULL);
    /* All records share the same virtual_time but must still be ordered by
       monotonic sequence. */
    for (i = 0u; i < 5u; ++i) {
        semu_status s = append_simple(t, SEMU_TRACE_KIND_INSTRUCTION, 500u,
            0x8000u + i, 0u, 0u, 0u, 2u, &err);
        SEMU_TEST_EQ_U64(context, SEMU_OK, s);
    }
    for (i = 0u; i < 5u; ++i) {
        rec = semu_trace_get(t, (size_t)i);
        SEMU_TEST_ASSERT(context, rec != NULL);
        SEMU_TEST_EQ_U64(context, i, rec->sequence);
        SEMU_TEST_EQ_U64(context, 500u, rec->virtual_time_ns);
        SEMU_TEST_EQ_U64(context, 0x8000u + i, rec->pc);
    }
    semu_trace_destroy(t);
}

static void test_capacity_stop(semu_test_context *context)
{
    semu_error err;
    semu_trace *t;
    semu_status s;
    uint32_t i;

    semu_error_clear(&err);
    t = semu_trace_create(4u, SEMU_TRACE_OVERFLOW_STOP, &err);
    SEMU_TEST_ASSERT(context, t != NULL);
    for (i = 0u; i < 4u; ++i) {
        s = append_simple(t, SEMU_TRACE_KIND_MMIO_READ, 10u * (uint64_t)i,
            0u, 0x4000u + i, i, 0u, 1u, &err);
        SEMU_TEST_EQ_U64(context, SEMU_OK, s);
    }
    SEMU_TEST_EQ_U64(context, 4u, semu_trace_count(t));
    SEMU_TEST_ASSERT(context, !semu_trace_overflowed(t));

    /* Fifth append must fail with RANGE under STOP policy. */
    semu_error_clear(&err);
    s = append_simple(t, SEMU_TRACE_KIND_MMIO_WRITE, 999u, 0u, 0x4004u, 0u,
                      0u, 1u, &err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, s);
    SEMU_TEST_EQ_U64(context, 4u, semu_trace_count(t));
    SEMU_TEST_ASSERT(context, !semu_trace_overflowed(t));
    semu_trace_destroy(t);
}

static void test_capacity_truncate(semu_test_context *context)
{
    semu_error err;
    semu_trace *t;
    semu_status s;
    const semu_trace_record *rec;
    uint32_t i;

    semu_error_clear(&err);
    t = semu_trace_create(4u, SEMU_TRACE_OVERFLOW_TRUNCATE, &err);
    SEMU_TEST_ASSERT(context, t != NULL);
    for (i = 0u; i < 4u; ++i) {
        s = append_simple(t, SEMU_TRACE_KIND_DEVICE, 10u * (uint64_t)i, 0u,
                          0u, i, 0u, 1u, &err);
        SEMU_TEST_EQ_U64(context, SEMU_OK, s);
    }
    SEMU_TEST_ASSERT(context, !semu_trace_overflowed(t));

    /* Fifth append overwrites oldest; overflowed flag set. */
    s = append_simple(t, SEMU_TRACE_KIND_DEVICE, 999u, 0u, 0u, 99u, 0u, 1u,
                      &err);
    SEMU_TEST_EQ_U64(context, SEMU_OK, s);
    SEMU_TEST_ASSERT(context, semu_trace_overflowed(t));
    SEMU_TEST_EQ_U64(context, 4u, semu_trace_count(t));

    /* Oldest surviving record should be sequence 1 (sequence 0 dropped). */
    rec = semu_trace_get(t, 0u);
    SEMU_TEST_ASSERT(context, rec != NULL);
    SEMU_TEST_EQ_U64(context, 1u, rec->sequence);

    /* Newest record is the one we just appended. */
    rec = semu_trace_get(t, 3u);
    SEMU_TEST_ASSERT(context, rec != NULL);
    SEMU_TEST_EQ_U64(context, 4u, rec->sequence);
    SEMU_TEST_EQ_U64(context, 99u, rec->value);

    /* Continue truncating to verify wraparound. */
    s = append_simple(t, SEMU_TRACE_KIND_DEVICE, 1000u, 0u, 0u, 100u, 0u, 1u,
                      &err);
    SEMU_TEST_EQ_U64(context, SEMU_OK, s);
    rec = semu_trace_get(t, 0u);
    SEMU_TEST_ASSERT(context, rec != NULL);
    SEMU_TEST_EQ_U64(context, 2u, rec->sequence);
    semu_trace_destroy(t);
}

static void test_format_stability(semu_test_context *context)
{
    semu_error err;
    semu_trace *t1, *t2;
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
        uint32_t kind = i % 2u == 0u ? SEMU_TRACE_KIND_INSTRUCTION
                                     : SEMU_TRACE_KIND_MMIO_READ;
        append_simple(t1, kind, 1000u * (uint64_t)(i + 1u), 0x1000u + i,
            0x2000u + i, 0x3000u + i, i, 4u, &err);
        semu_error_clear(&err);
        append_simple(t2, kind, 1000u * (uint64_t)(i + 1u), 0x1000u + i,
            0x2000u + i, 0x3000u + i, i, 4u, &err);
        semu_error_clear(&err);
    }

    len1 = semu_trace_format(t1, buf1, sizeof(buf1));
    len2 = semu_trace_format(t2, buf2, sizeof(buf2));
    SEMU_TEST_EQ_U64(context, len1, len2);
    SEMU_TEST_ASSERT(context, len1 > 0u);
    SEMU_TEST_ASSERT(context, memcmp(buf1, buf2, len1) == 0);

    semu_trace_destroy(t1);
    semu_trace_destroy(t2);
}

static void test_format_byte_identical_repeat(semu_test_context *context)
{
    /* Format the same trace twice and confirm byte-identical output. */
    semu_error err;
    semu_trace *t;
    char buf1[1024];
    char buf2[1024];
    size_t len1, len2;
    uint32_t i;

    semu_error_clear(&err);
    t = semu_trace_create(8u, SEMU_TRACE_OVERFLOW_STOP, &err);
    SEMU_TEST_ASSERT(context, t != NULL);
    for (i = 0u; i < 4u; ++i) {
        append_simple(t, SEMU_TRACE_KIND_FRAME, 5000u * (uint64_t)(i + 1u),
            0u, 0u, i, 0u, 16u, &err);
        semu_error_clear(&err);
    }
    len1 = semu_trace_format(t, buf1, sizeof(buf1));
    len2 = semu_trace_format(t, buf2, sizeof(buf2));
    SEMU_TEST_EQ_U64(context, len1, len2);
    SEMU_TEST_ASSERT(context, memcmp(buf1, buf2, len1) == 0);
    semu_trace_destroy(t);
}

static void test_reset(semu_test_context *context)
{
    semu_error err;
    semu_trace *t;
    semu_status s;

    semu_error_clear(&err);
    t = semu_trace_create(8u, SEMU_TRACE_OVERFLOW_STOP, &err);
    SEMU_TEST_ASSERT(context, t != NULL);
    s = append_simple(t, SEMU_TRACE_KIND_INPUT, 100u, 0u, 0u, 0u, 0u, 1u,
                      &err);
    SEMU_TEST_EQ_U64(context, SEMU_OK, s);
    SEMU_TEST_EQ_U64(context, 1u, semu_trace_count(t));

    semu_trace_reset(t);
    SEMU_TEST_EQ_U64(context, 0u, semu_trace_count(t));
    SEMU_TEST_ASSERT(context, semu_trace_get(t, 0u) == NULL);
    SEMU_TEST_ASSERT(context, !semu_trace_overflowed(t));

    /* After reset, appending restarts sequence at 0. */
    s = append_simple(t, SEMU_TRACE_KIND_COMPAT, 200u, 0u, 0u, 0u, 0u, 1u,
                      &err);
    SEMU_TEST_EQ_U64(context, SEMU_OK, s);
    {
        const semu_trace_record *rec = semu_trace_get(t, 0u);
        SEMU_TEST_ASSERT(context, rec != NULL);
        SEMU_TEST_EQ_U64(context, 0u, rec->sequence);
    }
    semu_trace_destroy(t);
}

static void test_no_host_state_in_records(semu_test_context *context)
{
    /* Records only carry integers: schema, kind, time, seq, pc, addr,
       value, event, width. No pointers or paths. This test verifies that
       formatted output contains no host pointer or path artifacts. */
    semu_error err;
    semu_trace *t;
    char buf[512];
    size_t len;

    semu_error_clear(&err);
    t = semu_trace_create(8u, SEMU_TRACE_OVERFLOW_STOP, &err);
    SEMU_TEST_ASSERT(context, t != NULL);
    append_simple(t, SEMU_TRACE_KIND_MMIO_WRITE, 42u, 0x8000u, 0x50000000u,
                  0xDEADu, 0u, 4u, &err);
    semu_error_clear(&err);
    len = semu_trace_format(t, buf, sizeof(buf));
    SEMU_TEST_ASSERT(context, len > 0u);
    /* Output must not contain hex addresses in pointer range or path
       separators. */
    SEMU_TEST_ASSERT(context, strstr(buf, "0x7") == NULL);
    SEMU_TEST_ASSERT(context, strstr(buf, "/") == NULL);
    semu_trace_destroy(t);
}

static void test_null_safety(semu_test_context *context)
{
    semu_error err;
    semu_trace_record r;
    semu_status s;

    semu_error_clear(&err);
    /* Capacity 0 is rejected. */
    SEMU_TEST_ASSERT(context,
        semu_trace_create(0u, SEMU_TRACE_OVERFLOW_STOP, &err) == NULL);
    /* Capacity exceeding max is rejected. */
    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context,
        semu_trace_create(SEMU_TRACE_MAX_RECORDS + 1u,
            SEMU_TRACE_OVERFLOW_STOP, &err) == NULL);

    /* Append to NULL is rejected. */
    r = make_record(SEMU_TRACE_KIND_INSTRUCTION, 0u, 0u, 0u, 0u, 0u, 0u);
    s = semu_trace_append(NULL, &r, &err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT, s);

    /* get/count/overflowed on NULL are safe. */
    SEMU_TEST_EQ_U64(context, 0u, semu_trace_count(NULL));
    SEMU_TEST_ASSERT(context, !semu_trace_overflowed(NULL));
    SEMU_TEST_ASSERT(context, semu_trace_get(NULL, 0u) == NULL);

    /* Format on NULL produces empty. */
    {
        char buf[16];
        SEMU_TEST_EQ_U64(context, 0u,
            semu_trace_format(NULL, buf, sizeof(buf)));
    }
}

static void test_max_capacity(semu_test_context *context)
{
    semu_error err;
    semu_trace *t;
    uint32_t i;

    semu_error_clear(&err);
    t = semu_trace_create(SEMU_TRACE_MAX_RECORDS, SEMU_TRACE_OVERFLOW_STOP,
        &err);
    SEMU_TEST_ASSERT(context, t != NULL);
    for (i = 0u; i < SEMU_TRACE_MAX_RECORDS; ++i) {
        semu_status s = append_simple(t, SEMU_TRACE_KIND_INSTRUCTION,
            (uint64_t)i, 0u, 0u, 0u, 0u, 4u, &err);
        SEMU_TEST_EQ_U64(context, SEMU_OK, s);
    }
    SEMU_TEST_EQ_U64(context, (size_t)SEMU_TRACE_MAX_RECORDS,
        semu_trace_count(t));
    SEMU_TEST_ASSERT(context, !semu_trace_overflowed(t));

    /* One more must fail under STOP. */
    {
        semu_status s = append_simple(t, SEMU_TRACE_KIND_INSTRUCTION, 0u,
            0u, 0u, 0u, 0u, 4u, &err);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, s);
    }
    semu_trace_destroy(t);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_all_kinds),
        SEMU_TEST_CASE(test_same_time_ordering),
        SEMU_TEST_CASE(test_capacity_stop),
        SEMU_TEST_CASE(test_capacity_truncate),
        SEMU_TEST_CASE(test_format_stability),
        SEMU_TEST_CASE(test_format_byte_identical_repeat),
        SEMU_TEST_CASE(test_reset),
        SEMU_TEST_CASE(test_no_host_state_in_records),
        SEMU_TEST_CASE(test_null_safety),
        SEMU_TEST_CASE(test_max_capacity)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
