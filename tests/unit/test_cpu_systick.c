#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "test.h"

#include <string.h>

#define SCS 0xe000e000u
#define SYSTICK_CTRL (SCS + 0x10u)
#define SYSTICK_RELOAD (SCS + 0x14u)
#define SYSTICK_CURRENT (SCS + 0x18u)
#define SYSTICK_CALIB (SCS + 0x1cu)
#define SYSTICK_ENABLE_SOURCE 5u
#define SYSTICK_COUNTFLAG (1u << 16)

static int write_word(semu_cpu_fixture *fixture, uint32_t address,
                      uint32_t value)
{
    return semu_bus_write(fixture->bus, address, 4u, value,
                          &fixture->error) == SEMU_OK;
}

static int read_word(semu_cpu_fixture *fixture, uint32_t address,
                     uint32_t *value)
{
    return semu_bus_read(fixture->bus, address, 4u, value,
                         &fixture->error) == SEMU_OK;
}

static void test_reload_countflag_and_calib(semu_test_context *context)
{
    static const uint32_t reloads[] = {0u, 1u, 0x00ffffffu};
    static const uint8_t program[] = {0x00u, 0xbeu};
    semu_cpu_fixture fixture;
    uint32_t value;
    size_t index;

    for (index = 0u; index < sizeof(reloads) / sizeof(reloads[0]); ++index) {
        SEMU_TEST_ASSERT(context,
                         semu_cpu_fixture_init(&fixture, program,
                                               sizeof(program)));
        SEMU_TEST_ASSERT(context,
                         write_word(&fixture, SYSTICK_RELOAD, reloads[index]));
        SEMU_TEST_ASSERT(context,
                         write_word(&fixture, SYSTICK_CTRL,
                                    SYSTICK_ENABLE_SOURCE));
        SEMU_TEST_ASSERT(context, read_word(&fixture, SYSTICK_RELOAD, &value));
        SEMU_TEST_EQ_U64(context, reloads[index], value);
        SEMU_TEST_ASSERT(context, read_word(&fixture, SYSTICK_CURRENT, &value));
        SEMU_TEST_EQ_U64(context, reloads[index], value);
        if (reloads[index] != 0u && reloads[index] != 0x00ffffffu) {
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                             semu_cpu_fixture_step(&fixture));
            SEMU_TEST_EQ_U64(context, 1u,
                             semu_scheduler_now(fixture.scheduler));
            SEMU_TEST_ASSERT(context, read_word(&fixture, SYSTICK_CTRL,
                                                &value));
            SEMU_TEST_ASSERT(context, (value & SYSTICK_COUNTFLAG) != 0u);
            SEMU_TEST_ASSERT(context, read_word(&fixture, SYSTICK_CTRL,
                                                &value));
            SEMU_TEST_ASSERT(context, (value & SYSTICK_COUNTFLAG) == 0u);
            SEMU_TEST_ASSERT(context, read_word(&fixture, SYSTICK_CURRENT,
                                                &value));
            SEMU_TEST_EQ_U64(context, reloads[index], value);
        }
        SEMU_TEST_ASSERT(context, read_word(&fixture, SYSTICK_CALIB, &value));
        SEMU_TEST_EQ_U64(context, 0u, value);
        semu_cpu_fixture_destroy(&fixture);
    }
}

static void test_reprogram_cancel_and_refusal(semu_test_context *context)
{
    static const uint8_t program[] = {0x00u, 0xbeu};
    semu_cpu_fixture fixture;
    uint32_t value;
    uint32_t before;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program, sizeof(program)));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SYSTICK_RELOAD, 1u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SYSTICK_CTRL,
                                         SYSTICK_ENABLE_SOURCE));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SYSTICK_RELOAD, 3u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SYSTICK_CURRENT, 0xffffffffu));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SYSTICK_CURRENT, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_ASSERT(context, write_word(&fixture, SYSTICK_CTRL, 0u));
    SEMU_TEST_ASSERT(context, !semu_scheduler_has_events(fixture.scheduler));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SYSTICK_CTRL, &before));
    SEMU_TEST_EQ_U64(context, 0u, before);

    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_write(fixture.bus, SYSTICK_CTRL, 4u, 1u,
                                    &fixture.error));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SYSTICK_CTRL, &value));
    SEMU_TEST_EQ_U64(context, before, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, fixture.error.code);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SYSTICK_RELOAD, &value));
    SEMU_TEST_EQ_U64(context, 3u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_write(fixture.bus, SYSTICK_CALIB, 4u, 1u,
                                    &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_write(fixture.bus, SYSTICK_CTRL + 1u, 2u, 1u,
                                    &fixture.error));
    semu_cpu_fixture_destroy(&fixture);
}

static void test_current_decrements_between_events(semu_test_context *context)
{
    static const uint8_t program[] = {
        0x00u, 0xbfu, 0x00u, 0xbfu, 0x00u, 0xbfu, 0x00u, 0xbeu
    };
    semu_cpu_fixture fixture;
    uint32_t value;

    SEMU_TEST_ASSERT(context,
                     semu_cpu_fixture_init(&fixture, program, sizeof(program)));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SYSTICK_RELOAD, 3u));
    SEMU_TEST_ASSERT(context, write_word(&fixture, SYSTICK_CTRL,
                                         SYSTICK_ENABLE_SOURCE));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SYSTICK_CURRENT, &value));
    SEMU_TEST_EQ_U64(context, 2u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SYSTICK_CURRENT, &value));
    SEMU_TEST_EQ_U64(context, 1u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_ASSERT(context, read_word(&fixture, SYSTICK_CTRL, &value));
    SEMU_TEST_ASSERT(context, (value & SYSTICK_COUNTFLAG) != 0u);
    SEMU_TEST_ASSERT(context, read_word(&fixture, SYSTICK_CURRENT, &value));
    SEMU_TEST_EQ_U64(context, 3u, value);
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_reload_countflag_and_calib),
        SEMU_TEST_CASE(test_reprogram_cancel_and_refusal),
        SEMU_TEST_CASE(test_current_decrements_between_events)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
