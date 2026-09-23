#include "test.h"
#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/scheduler.h"

#include <string.h>

typedef struct rtc_fixture {
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4 *soc;
    semu_error error;
    unsigned rises;
} rtc_fixture;

static void irq(void *context, unsigned line, int level)
{
    rtc_fixture *f = context;
    if (line == 2u && level != 0) ++f->rises;
}

static int create(rtc_fixture *f, const char *profile)
{
    memset(f, 0, sizeof(*f));
    f->bus = semu_bus_create(&f->error);
    f->scheduler = semu_scheduler_create(&f->error);
    if (f->bus == NULL || f->scheduler == NULL) return 0;
    f->soc = semu_apollo4_create(f->bus, &f->error);
    return f->soc != NULL && semu_apollo4_init(f->soc, f->scheduler,
        irq, f, &f->error) == SEMU_OK &&
        semu_apollo4_select_profile(f->soc, profile, &f->error) == SEMU_OK;
}

static void destroy(rtc_fixture *f)
{
    semu_apollo4_destroy(f->soc);
    semu_bus_destroy(f->bus);
    semu_scheduler_destroy(f->scheduler);
}

static void wr(semu_test_context *c, rtc_fixture *f, uint32_t offset,
               uint32_t value)
{
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_write(f->bus,
        0x40004800u + offset, 4u, value, &f->error));
}

static void rd(semu_test_context *c, rtc_fixture *f, uint32_t offset,
               uint32_t expected)
{
    uint32_t value = 0u;
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_read(f->bus,
        0x40004800u + offset, 4u, &value, &f->error));
    SEMU_TEST_EQ_U64(c, expected, value);
}

static void test_rtc_other_profile_creation_preserves_state(semu_test_context *c)
{
    rtc_fixture a, b;
    SEMU_TEST_ASSERT(c, create(&a, "sapporo-2.35.34"));
    wr(c, &a, 0u, 0x10u);
    SEMU_TEST_ASSERT(c, create(&b, "sapporo-2.22.60"));
    rd(c, &a, 0u, 0x10u);
    semu_bus_reset(b.bus);
    rd(c, &a, 0u, 0x10u);
    destroy(&b);
    rd(c, &a, 0u, 0x10u);
    destroy(&a);
}

static void arm(semu_test_context *c, rtc_fixture *f)
{
    SEMU_TEST_EQ_U64(c, SEMU_OK,
        semu_scheduler_advance(f->scheduler, 10000000u, &f->error));
    wr(c, f, 0u, 0x0eu);
    wr(c, f, 0x200u, 1u);
    SEMU_TEST_ASSERT(c, semu_scheduler_has_events(f->scheduler));
}

static void test_rtc_independent_alarms_and_destruction(semu_test_context *c)
{
    rtc_fixture a, b;
    SEMU_TEST_ASSERT(c, create(&a, "sapporo-2.35.34"));
    SEMU_TEST_ASSERT(c, create(&b, "sapporo-2.35.34"));
    arm(c, &a);
    arm(c, &b);
    SEMU_TEST_EQ_U64(c, SEMU_OK,
        semu_scheduler_advance(a.scheduler, 990000000u, &a.error));
    SEMU_TEST_EQ_U64(c, 1u, a.rises);
    SEMU_TEST_EQ_U64(c, 0u, b.rises);
    rd(c, &a, 0x204u, 1u);
    rd(c, &b, 0x204u, 0u);
    semu_apollo4_destroy(a.soc);
    a.soc = NULL;
    SEMU_TEST_ASSERT(c, !semu_scheduler_has_events(a.scheduler));
    SEMU_TEST_EQ_U64(c, SEMU_OK,
        semu_scheduler_advance(a.scheduler, 2000000000u, &a.error));
    SEMU_TEST_EQ_U64(c, 1u, a.rises);
    SEMU_TEST_EQ_U64(c, SEMU_OK,
        semu_scheduler_advance(b.scheduler, 990000000u, &b.error));
    SEMU_TEST_EQ_U64(c, 1u, b.rises);
    rd(c, &b, 0x204u, 1u);
    destroy(&a);
    destroy(&b);
}

static void other_event(void *context, uint64_t now)
{
    unsigned *count = context;
    (void)now;
    ++*count;
}

static void test_rtc_destroy_preserves_reused_event_id(semu_test_context *c)
{
    rtc_fixture f;
    semu_event_id id;
    unsigned count = 0u;
    SEMU_TEST_ASSERT(c, create(&f, "sapporo-2.35.34"));
    arm(c, &f);
    semu_scheduler_reset(f.scheduler);
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_scheduler_schedule(f.scheduler,
        1u, other_event, &count, &id, &f.error));
    semu_apollo4_destroy(f.soc);
    f.soc = NULL;
    SEMU_TEST_EQ_U64(c, SEMU_OK,
        semu_scheduler_advance(f.scheduler, 1u, &f.error));
    SEMU_TEST_EQ_U64(c, 1u, count);
    destroy(&f);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_rtc_other_profile_creation_preserves_state),
        SEMU_TEST_CASE(test_rtc_independent_alarms_and_destruction),
        SEMU_TEST_CASE(test_rtc_destroy_preserves_reused_event_id)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
