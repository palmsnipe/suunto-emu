#include "../../src/soc/apollo4/timer.h"

#include "semu/scheduler.h"
#include "test.h"

typedef struct irq_log {
    unsigned count;
    unsigned channel[8];
    int level[8];
} irq_log;

static void irq(void *context, unsigned channel, int level)
{
    irq_log *log = (irq_log *)context;
    if (log->count < SEMU_ARRAY_LEN(log->channel)) {
        log->channel[log->count] = channel;
        log->level[log->count] = level;
    }
    ++log->count;
}

static semu_apollo4_timer *make_timer(semu_scheduler **scheduler,
                                      irq_log *log, semu_error *error)
{
    *scheduler = semu_scheduler_create(error);
    if (*scheduler == NULL) return NULL;
    return semu_apollo4_timer_create(*scheduler, irq, log, error);
}

static void reset_and_refuse(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_apollo4_timer *timer;
    uint32_t value = 99u;
    irq_log log = { 0u };
    semu_error_clear(&error);
    timer = make_timer(&scheduler, &log, &error);
    SEMU_TEST_ASSERT(context, timer != NULL);
    semu_apollo4_timer_reset(timer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x10u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0x7ffu, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x10u, 4u, 0x27ffu,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x10u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0x27ffu, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_apollo4_timer_write(timer, 0x10u, 4u, 0x2800u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x10u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0x27ffu, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x200u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_apollo4_timer_write(timer, 0x204u, 2u, 1u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_apollo4_timer_read(timer, 0x21cu, 4u, &value,
                                             &error));
    semu_apollo4_timer_destroy(timer);
    semu_scheduler_destroy(scheduler);
}

static void compare_irq_and_clear(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_apollo4_timer *timer;
    uint32_t value;
    irq_log log = { 0u };
    semu_error_clear(&error);
    timer = make_timer(&scheduler, &log, &error);
    SEMU_TEST_ASSERT(context, timer != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x10u, 4u, 1u << 0,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x210u, 4u, 0x100u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x208u, 4u, 5u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x200u, 4u, 1u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, 4u, &error));
    SEMU_TEST_EQ_U64(context, 0u, log.count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, 1u, &error));
    SEMU_TEST_EQ_U64(context, 1u, log.count);
    SEMU_TEST_EQ_U64(context, 0u, log.channel[0]);
    SEMU_TEST_EQ_U64(context, 1u, log.level[0]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x60u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 1u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x68u, 4u, 1u,
                                              &error));
    SEMU_TEST_EQ_U64(context, 2u, log.count);
    SEMU_TEST_EQ_U64(context, 0u, log.level[1]);
    semu_apollo4_timer_destroy(timer);
    semu_scheduler_destroy(scheduler);
}

static void pwm_and_atomic_refusal(semu_test_context *context)
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
                     semu_apollo4_timer_write(timer, 0x320u, 4u, 0xa44u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_apollo4_timer_write(timer, 0x320u, 4u, 0xffffffffu,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x320u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0xa40u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, 100u, &error));
    SEMU_TEST_EQ_U64(context, 0u, log.count);
    semu_apollo4_timer_destroy(timer);
    semu_scheduler_destroy(scheduler);
}

static void observed_control_three(semu_test_context *context)
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
                     semu_apollo4_timer_write(timer, 0x200u, 4u, 3u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x200u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 3u, value);
    SEMU_TEST_EQ_U64(context, 0u, log.count);
    semu_apollo4_timer_destroy(timer);
    semu_scheduler_destroy(scheduler);
}

static void observed_ctimer8_readback(semu_test_context *context)
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
                     semu_apollo4_timer_write(timer, 0x300u, 4u, 0x144u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x300u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0x140u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x300u, 4u, 0x142u,
                                              &error));
    semu_apollo4_timer_destroy(timer);
    semu_scheduler_destroy(scheduler);
}

static void observed_ctimer0_readback(semu_test_context *context)
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
                     semu_apollo4_timer_write(timer, 0x200u, 4u, 0x222u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x200u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0x220u, value);
    semu_apollo4_timer_destroy(timer);
    semu_scheduler_destroy(scheduler);
}

static void observed_ctimer13_controls(semu_test_context *context)
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
                     semu_apollo4_timer_write(timer, 0x3a0u, 4u, 0x1d20u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x3a0u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0x1d20u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x3acu, 4u, 0x3cu,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x3acu, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0x3cu, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, 100u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x3a4u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_apollo4_timer_write(timer, 0x3a0u, 4u, 0xdeadu,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x3a0u, 4u, 0x1c22u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x3a0u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0x1c20u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x3a0u, 4u, 0x1c21u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, 2500u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x3a4u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, 120000u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x3a4u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 1u, value);
    semu_apollo4_timer_destroy(timer);
    semu_scheduler_destroy(scheduler);
}

static void observed_status_writes(semu_test_context *context)
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
                     semu_apollo4_timer_write(timer, 0x60u, 4u, 0x8000000u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x60u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0x8000000u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_apollo4_timer_write(timer, 0x60u, 4u, 4u,
                                             &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0x60u, 4u, 1u,
                                              &error));
    semu_apollo4_timer_destroy(timer);
    semu_scheduler_destroy(scheduler);
}

static void observed_value_write(semu_test_context *context)
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
                     semu_apollo4_timer_write(timer, 0x3a4u, 4u, 0u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0x3a4u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    semu_apollo4_timer_destroy(timer);
    semu_scheduler_destroy(scheduler);
}

static void observed_auxiliary_register(semu_test_context *context)
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
                     semu_apollo4_timer_read(timer, 0xe8u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0xe8u, 4u, 0x12u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0xe8u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0x12u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_apollo4_timer_write(timer, 0xe8u, 4u, 1u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0xe8u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0x12u, value);
    semu_apollo4_timer_destroy(timer);
    semu_scheduler_destroy(scheduler);
}

static void observed_b4_register(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_apollo4_timer *timer;
    uint32_t value = UINT32_C(0xffffffff);
    irq_log log = { 0u };

    semu_error_clear(&error);
    timer = make_timer(&scheduler, &log, &error);
    SEMU_TEST_ASSERT(context, timer != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0xb4u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0xb4u, 4u, 0u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0xb4u, 4u,
                                              UINT32_C(0x10000000), &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_apollo4_timer_write(timer, 0xb4u, 4u, 1u,
                                              &error));
    semu_apollo4_timer_destroy(timer);
    semu_scheduler_destroy(scheduler);
}

static void observed_d8_register(semu_test_context *context)
{
    semu_error error;
    semu_scheduler *scheduler;
    semu_apollo4_timer *timer;
    uint32_t value = 99u;
    irq_log log = { 0u };

    semu_error_clear(&error);
    timer = make_timer(&scheduler, &log, &error);
    SEMU_TEST_ASSERT(context, timer != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0xd8u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0xd8u, 4u,
                                              0x1f000000u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_read(timer, 0xd8u, 4u, &value,
                                             &error));
    SEMU_TEST_EQ_U64(context, 0x1f000000u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_timer_write(timer, 0xd8u, 4u, 0u,
                                              &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_apollo4_timer_write(timer, 0xd8u, 4u, 1u,
                                              &error));
    semu_apollo4_timer_destroy(timer);
    semu_scheduler_destroy(scheduler);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(reset_and_refuse),
        SEMU_TEST_CASE(compare_irq_and_clear),
        SEMU_TEST_CASE(pwm_and_atomic_refusal),
        SEMU_TEST_CASE(observed_control_three),
        SEMU_TEST_CASE(observed_ctimer8_readback),
        SEMU_TEST_CASE(observed_ctimer0_readback),
        SEMU_TEST_CASE(observed_ctimer13_controls),
        SEMU_TEST_CASE(observed_status_writes),
        SEMU_TEST_CASE(observed_value_write),
        SEMU_TEST_CASE(observed_auxiliary_register),
        SEMU_TEST_CASE(observed_b4_register),
        SEMU_TEST_CASE(observed_d8_register)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
