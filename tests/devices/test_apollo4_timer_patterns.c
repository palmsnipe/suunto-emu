#include "../../src/soc/apollo4/timer.h"
#include "../../src/core/scheduler_internal.h"

#include "semu/scheduler.h"
#include "test.h"

typedef struct irq_log {
    unsigned count;
} irq_log;

static void irq(void *context, unsigned channel, int level)
{
    irq_log *log = (irq_log *)context;
    (void)channel;
    (void)level;
    ++log->count;
}

static semu_apollo4_timer *make_timer(semu_scheduler **scheduler,
                                      irq_log *log, semu_error *error)
{
    *scheduler = semu_scheduler_create(error);
    if (*scheduler == NULL) {
        return NULL;
    }
    return semu_apollo4_timer_create(*scheduler, irq, log, error);
}

static void observed_pattern_register(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_apollo4_timer *timer;
    uint32_t value = 0u;
    irq_log log = { 0u };

    semu_error_clear(&error);
    timer = make_timer(&scheduler, &log, &error);
    SEMU_TEST_ASSERT(context, timer != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x104u, 4u,
                                              0x10300u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x104u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0x10300u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x104u, 4u,
                                              0x10301u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x104u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0x10301u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x104u, 4u,
                                              0x12301u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x104u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0x12301u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x104u, 4u,
                                              0x12300u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x104u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0x12300u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_apollo4_timer_write(timer, 0x104u, 4u,
                                              0x10302u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x104u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0x12300u, value);
    semu_apollo4_timer_destroy(timer);
    semu_scheduler_destroy(scheduler);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(observed_pattern_register)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
