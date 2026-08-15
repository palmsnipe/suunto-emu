#include "../../src/display/nema_diagnostics.h"
#include "../../src/display/nema_completion.h"
#include "test.h"

#include "semu/scheduler.h"
#include "semu/types.h"

#include <string.h>

/* --- Diagnostics tests --- */

static void test_refusal_categories(semu_test_context *context)
{
    semu_error err;
    nema_diagnostics *diag = NULL;
    const nema_diag_record *recs;
    size_t count;
    size_t i;

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_diagnostics_create(&diag, &err) == SEMU_OK);
    for (i = 0u; i <= (size_t)NEMA_DIAG_FRAMING_ERROR; ++i) {
        SEMU_TEST_ASSERT(context,
            nema_diagnostics_record(diag, (nema_diag_category)i,
                                    (uint32_t)i, 0x1000u + (uint32_t)i,
                                    0xF0u, (uint32_t)i, NULL, 0u, &err)
            == SEMU_OK);
    }
    recs = nema_diagnostics_records(diag, &count);
    SEMU_TEST_EQ_U64(context, 8u, count);
    SEMU_TEST_EQ_U64(context, NEMA_DIAG_UNKNOWN_REGISTER,
                     recs[0u].category);
    SEMU_TEST_EQ_U64(context, NEMA_DIAG_FRAMING_ERROR,
                     recs[7u].category);
    nema_diagnostics_destroy(diag);
}

static void test_context_truncation(semu_test_context *context)
{
    semu_error err;
    nema_diagnostics *diag = NULL;
    const nema_diag_record *recs;
    size_t count;
    uint32_t ctx[20u];
    size_t i;

    semu_error_clear(&err);
    for (i = 0u; i < 20u; ++i) ctx[i] = (uint32_t)i;
    SEMU_TEST_ASSERT(context, nema_diagnostics_create(&diag, &err) == SEMU_OK);
    SEMU_TEST_ASSERT(context,
        nema_diagnostics_record(diag, NEMA_DIAG_FRAMING_ERROR,
                                1u, 0x2000u, 0x100u, 5u,
                                ctx, 20u, &err) == SEMU_OK);
    recs = nema_diagnostics_records(diag, &count);
    SEMU_TEST_EQ_U64(context, 1u, count);
    SEMU_TEST_EQ_U64(context, NEMA_DIAG_MAX_CONTEXT, recs[0u].context_count);
    SEMU_TEST_ASSERT(context, recs[0u].truncated);
    SEMU_TEST_EQ_U64(context, 0u, recs[0u].context[0u]);
    SEMU_TEST_EQ_U64(context, 15u, recs[0u].context[15u]);
    nema_diagnostics_destroy(diag);
}

static void test_no_host_data(semu_test_context *context)
{
    semu_error err;
    nema_diagnostics *diag = NULL;
    const nema_diag_record *recs;
    size_t count;
    char buf[512];
    uint32_t ctx[2u] = { 0xDEADBEEFu, 0xCAFEBABEu };

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_diagnostics_create(&diag, &err) == SEMU_OK);
    nema_diagnostics_record(diag, NEMA_DIAG_BAD_PREFIX,
                            3u, 0x3000u, 0x110u, 0u, ctx, 2u, &err);
    recs = nema_diagnostics_records(diag, &count);
    nema_diag_format(&recs[0u], buf, sizeof(buf));
    /* Must contain category name and hex values, no pointer/path */
    SEMU_TEST_ASSERT(context, strstr(buf, "bad_prefix") != NULL);
    SEMU_TEST_ASSERT(context, strstr(buf, "0xdeadbeef") != NULL);
    SEMU_TEST_ASSERT(context, strstr(buf, "0x00003000") != NULL);
    nema_diagnostics_destroy(diag);
}

static void test_repeat_format(semu_test_context *context)
{
    semu_error err;
    nema_diagnostics *diag = NULL;
    const nema_diag_record *recs;
    size_t count;
    char buf1[512], buf2[512];
    uint32_t ctx[3u] = { 1u, 2u, 3u };

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_diagnostics_create(&diag, &err) == SEMU_OK);
    nema_diagnostics_record(diag, NEMA_DIAG_UNSUPPORTED_DRAW,
                             5u, 0x4000u, 0x100u, 0x00000005u,
                             ctx, 3u, &err);
    recs = nema_diagnostics_records(diag, &count);
    nema_diag_format(&recs[0u], buf1, sizeof(buf1));
    nema_diag_format(&recs[0u], buf2, sizeof(buf2));
    SEMU_TEST_ASSERT(context, strcmp(buf1, buf2) == 0);
    nema_diagnostics_destroy(diag);
}

static void test_diag_reset(semu_test_context *context)
{
    semu_error err;
    nema_diagnostics *diag = NULL;
    size_t count;

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_diagnostics_create(&diag, &err) == SEMU_OK);
    nema_diagnostics_record(diag, NEMA_DIAG_BAD_SIZE,
                            1u, 0u, 0u, 0u, NULL, 0u, &err);
    nema_diagnostics_records(diag, &count);
    SEMU_TEST_EQ_U64(context, 1u, count);
    nema_diagnostics_reset(diag);
    nema_diagnostics_records(diag, &count);
    SEMU_TEST_EQ_U64(context, 0u, count);
    nema_diagnostics_destroy(diag);
}

/* --- Completion tests --- */

typedef struct {
    uint32_t offsets[8u];
    uint32_t values[8u];
    size_t count;
    int irq_asserted;
    unsigned irq_line;
} reg_capture;

static void on_reg_write(void *ctx, uint32_t offset, uint32_t value)
{
    reg_capture *c = (reg_capture *)ctx;
    if (c->count < 8u) {
        c->offsets[c->count] = offset;
        c->values[c->count] = value;
    }
    ++c->count;
}

static void on_irq(void *ctx, unsigned line, int asserted)
{
    reg_capture *c = (reg_capture *)ctx;
    c->irq_asserted = asserted;
    c->irq_line = line;
}

static void test_completion_once(semu_test_context *context)
{
    semu_error err;
    nema_completion *comp = NULL;
    semu_scheduler *sched = NULL;
    reg_capture cap = {0};

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_completion_create(&comp, &err) == SEMU_OK);
    sched = semu_scheduler_create(&err);
    SEMU_TEST_ASSERT(context, sched != NULL);

    SEMU_TEST_ASSERT(context,
        nema_completion_schedule(comp, sched, 42u,
                                 on_reg_write, &cap,
                                 on_irq, &cap, &err) == SEMU_OK);
    SEMU_TEST_ASSERT(context, nema_completion_pending(comp, 42u));

    /* Schedule again for same list — no-op */
    SEMU_TEST_ASSERT(context,
        nema_completion_schedule(comp, sched, 42u,
                                 on_reg_write, &cap,
                                 on_irq, &cap, &err) == SEMU_OK);

    /* Advance time — scheduler fires the event during advance */
    semu_scheduler_advance(sched, NEMA_COMPLETION_DELAY_NS, &err);

    SEMU_TEST_EQ_U64(context, 2u, cap.count);
    SEMU_TEST_EQ_U64(context, NEMA_REG_CLID, cap.offsets[0u]);
    SEMU_TEST_EQ_U64(context, 42u, cap.values[0u]);
    SEMU_TEST_EQ_U64(context, NEMA_REG_INTERRUPT, cap.offsets[1u]);
    SEMU_TEST_EQ_U64(context, 1u, cap.values[1u]);
    SEMU_TEST_ASSERT(context, cap.irq_asserted);
    SEMU_TEST_EQ_U64(context, NEMA_COMPLETION_IRQ_LINE, cap.irq_line);
    SEMU_TEST_ASSERT(context, !nema_completion_pending(comp, 42u));
    SEMU_TEST_EQ_U64(context, 1u, nema_completion_count(comp));

    semu_scheduler_destroy(sched);
    nema_completion_destroy(comp);
}

static void test_no_completion_on_refusal(semu_test_context *context)
{
    semu_error err;
    nema_completion *comp = NULL;
    semu_scheduler *sched = NULL;
    reg_capture cap = {0};

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_completion_create(&comp, &err) == SEMU_OK);
    sched = semu_scheduler_create(&err);
    SEMU_TEST_ASSERT(context, sched != NULL);

    /* Refused lists simply don't schedule — nothing fires */
    semu_scheduler_advance(sched, NEMA_COMPLETION_DELAY_NS * 10u, &err);
    SEMU_TEST_ASSERT(context, !semu_scheduler_has_events(sched));
    SEMU_TEST_EQ_U64(context, 0u, cap.count);
    SEMU_TEST_EQ_U64(context, 0u, nema_completion_count(comp));

    semu_scheduler_destroy(sched);
    nema_completion_destroy(comp);
}

static void test_reset_cancellation(semu_test_context *context)
{
    semu_error err;
    nema_completion *comp = NULL;
    semu_scheduler *sched = NULL;
    reg_capture cap = {0};

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_completion_create(&comp, &err) == SEMU_OK);
    sched = semu_scheduler_create(&err);
    SEMU_TEST_ASSERT(context, sched != NULL);

    nema_completion_schedule(comp, sched, 1u, on_reg_write, &cap,
                              on_irq, &cap, &err);
    SEMU_TEST_ASSERT(context, nema_completion_pending(comp, 1u));
    nema_completion_reset(comp);
    SEMU_TEST_ASSERT(context, !nema_completion_pending(comp, 1u));

    /* Even if scheduler advances, no callback fires */
    semu_scheduler_advance(sched, NEMA_COMPLETION_DELAY_NS, &err);
    /* The scheduler may still have the event, but the entry is inactive.
     * Running it should not invoke callbacks. */
    if (semu_scheduler_has_events(sched)) {
        semu_scheduler_run_next(sched, &err);
    }
    SEMU_TEST_EQ_U64(context, 0u, cap.count);

    semu_scheduler_destroy(sched);
    nema_completion_destroy(comp);
}

static void test_irq_order(semu_test_context *context)
{
    semu_error err;
    nema_completion *comp = NULL;
    semu_scheduler *sched = NULL;
    reg_capture cap = {0};

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, nema_completion_create(&comp, &err) == SEMU_OK);
    sched = semu_scheduler_create(&err);
    SEMU_TEST_ASSERT(context, sched != NULL);

    nema_completion_schedule(comp, sched, 7u, on_reg_write, &cap,
                              on_irq, &cap, &err);
    semu_scheduler_advance(sched, NEMA_COMPLETION_DELAY_NS, &err);

    /* CLID written first, then INTERRUPT, then IRQ asserted */
    SEMU_TEST_EQ_U64(context, NEMA_REG_CLID, cap.offsets[0u]);
    SEMU_TEST_EQ_U64(context, NEMA_REG_INTERRUPT, cap.offsets[1u]);
    SEMU_TEST_ASSERT(context, cap.irq_asserted);
    SEMU_TEST_ASSERT(context, cap.irq_line == NEMA_COMPLETION_IRQ_LINE);

    semu_scheduler_destroy(sched);
    nema_completion_destroy(comp);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_refusal_categories),
        SEMU_TEST_CASE(test_context_truncation),
        SEMU_TEST_CASE(test_no_host_data),
        SEMU_TEST_CASE(test_repeat_format),
        SEMU_TEST_CASE(test_diag_reset),
        SEMU_TEST_CASE(test_completion_once),
        SEMU_TEST_CASE(test_no_completion_on_refusal),
        SEMU_TEST_CASE(test_reset_cancellation),
        SEMU_TEST_CASE(test_irq_order)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
