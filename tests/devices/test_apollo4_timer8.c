#include "../../src/soc/apollo4/timer.h"
#include "../../src/core/scheduler_internal.h"
#include "test.h"
#include <string.h>

/* E-EMU-SAP235-TIMER8-003: 45 paired lane observations, including
 * compare boundaries, retained fractions and the UINT32_MAX limit. */
static void test_timer8_lane_and_restore(semu_test_context *c)
{
    static const struct { uint32_t offset; uint64_t value; uint32_t count; } steps[] = {
        {0x0u, UINT64_C(125000), 750u},
        {0x0u, UINT64_C(633666), 4551u},
        {0x0u, UINT64_C(1), 4552u},
        {0x0u, UINT64_C(333), 4553u},
        {0x0u, UINT64_C(1000), 4559u},
        {0x300u, UINT64_C(321), 4559u},
        {0x0u, UINT64_C(1), 4560u},
        {0x300u, UINT64_C(320), 0u},
        {0x0u, UINT64_C(1000), 0u},
        {0x300u, UINT64_C(321), 0u},
        {0x0u, UINT64_C(166), 1u},
        {0x0u, UINT64_C(1), 1u},
        {0x300u, UINT64_C(322), 0u},
        {0x308u, UINT64_C(3409), 0u},
        {0x300u, UINT64_C(321), 0u},
        {0x0u, UINT64_C(568166), 3409u},
        {0x0u, UINT64_C(1), 3409u},
        {0x0u, UINT64_C(433), 3411u},
        {0x308u, UINT64_C(3037), 3411u},
        {0x0u, UINT64_C(1000000), 9411u},
        {0x300u, UINT64_C(322), 0u},
        {0x308u, UINT64_C(3037), 0u},
        {0x300u, UINT64_C(321), 0u},
        {0x0u, UINT64_C(506166), 3037u},
        {0x0u, UINT64_C(1), 3037u},
        {0x0u, UINT64_C(333), 3039u},
        {0x0u, UINT64_C(715827376000), 0u},
        {0x0u, UINT64_C(1), 0u},
        {0x0u, UINT64_C(500), 3u},
        {0x0u, UINT64_C(715827882501), 3u},
        {0x300u, UINT64_C(320), 0u},
        {0x308u, UINT64_C(4294967295), 0u},
        {0x30cu, UINT64_C(0), 0u},
        {0x300u, UINT64_C(322), 0u},
        {0x300u, UINT64_C(321), 0u},
        {0x0u, UINT64_C(715827882499), 0u},
        {0x0u, UINT64_C(1), 0u},
        {0x0u, UINT64_C(1), 0u},
        {0x0u, UINT64_C(500), 0u},
        {0x300u, UINT64_C(320), 0u},
        {0x308u, UINT64_C(0), 0u},
        {0x300u, UINT64_C(321), 0u},
        {0x0u, UINT64_C(715827882500), 0u},
        {0x0u, UINT64_C(500), 0u},
        {0x300u, UINT64_C(320), 0u},
    };
    semu_error error;
    semu_scheduler *clock[2];
    semu_apollo4_timer *timer[2];
    size_t i, j;
    semu_error_clear(&error);
    for (j = 0; j < 2; ++j) {
        clock[j] = semu_scheduler_create(&error);
        SEMU_TEST_ASSERT(c, clock[j] != NULL);
        timer[j] = semu_apollo4_timer_create(clock[j], NULL, NULL, &error);
        SEMU_TEST_ASSERT(c, timer[j] != NULL);
        SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_write(timer[j], 0x10u, 4u, 0x27ffu, &error));
        SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_write(timer[j], 0x310u, 4u, 0x100u, &error));
        SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_write(timer[j], 0x308u, 4u, 0x11c8u, &error));
        SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_write(timer[j], 0x30cu, 4u, 0x2eeu, &error));
        SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_write(timer[j], 0x300u, 4u, 0x140u, &error));
        SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_write(timer[j], 0x300u, 4u, 0x142u, &error));
        SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_write(timer[j], 0x300u, 4u, 0x141u, &error));
    }
    for (i = 0; i < SEMU_ARRAY_LEN(steps); ++i) {
        semu_snapshot_writer saved;
        semu_snapshot_reader reader;
        for (j = 0; j < 2; ++j) {
            uint32_t value = 0u;
            if (steps[i].offset == 0u) {
                SEMU_TEST_EQ_U64(c, SEMU_OK, semu_scheduler_advance(clock[j], steps[i].value, &error));
            } else {
                SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_write(timer[j],
                    steps[i].offset, 4u, (uint32_t)steps[i].value, &error));
            }
            SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_read(timer[j], 0x304u, 4u, &value, &error));
            SEMU_TEST_EQ_U64(c, steps[i].count, value);
            SEMU_TEST_EQ_U64(c, 0u, semu_scheduler_event_count(clock[j]));
            SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_read(timer[j], 0x60u, 4u, &value, &error));
            SEMU_TEST_EQ_U64(c, 0u, value);
        }
        semu_snapshot_writer_init(&saved);
        SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_snapshot_write(timer[0], &saved, &error));
        semu_snapshot_reader_init(&reader, saved.data, saved.size);
        SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_snapshot_read(timer[1], &reader, &error));
        SEMU_TEST_ASSERT(c, semu_snapshot_reader_done(&reader));
        semu_snapshot_writer_destroy(&saved);
    }
    for (j = 0; j < 2; ++j) {
        semu_apollo4_timer_destroy(timer[j]);
        semu_scheduler_destroy(clock[j]);
    }
}

static void test_timer8_routing_and_atomic_refusal(semu_test_context *c)
{
    semu_error error;
    semu_scheduler *clock = semu_scheduler_create(&error);
    semu_apollo4_timer *timer;
    semu_snapshot_writer saved, after;
    semu_snapshot_reader reader;
    uint32_t value = 0;
    size_t i;
    static const struct {uint32_t offset, value;} bad[] = {
        {0xb4u, 0x20000000u}, {0x104u, 0x12202u}, {0x2e0u, 0x141u},
        {0x300u, 0x143u}, {0x314u, 0u}, {0x310u, 1u}
    };
    SEMU_TEST_ASSERT(c, clock != NULL);
    timer = semu_apollo4_timer_create(clock, NULL, NULL, &error);
    SEMU_TEST_ASSERT(c, timer != NULL);
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_write(timer, 0xb4u, 4u, 0x3f000000u, &error));
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_write(timer, 0x104u, 4u, 0x10201u, &error));
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_write(timer, 0x104u, 4u, 0x12201u, &error));
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_write(timer, 0x104u, 4u, 0x10200u, &error));
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_write(timer, 0x104u, 4u, 0x12200u, &error));
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_write(timer, 0x300u, 4u, 0x141u, &error));
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_scheduler_advance(clock, 167u, &error));
    semu_snapshot_writer_init(&saved);
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_snapshot_write(timer, &saved, &error));
    for (i = 0; i < SEMU_ARRAY_LEN(bad); ++i) {
        SEMU_TEST_EQ_U64(c, SEMU_ERR_UNSUPPORTED, semu_apollo4_timer_write(timer, bad[i].offset, 4u, bad[i].value, &error));
        semu_snapshot_writer_init(&after);
        SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_snapshot_write(timer, &after, &error));
        SEMU_TEST_EQ_U64(c, saved.size, after.size);
        SEMU_TEST_ASSERT(c, memcmp(saved.data, after.data, saved.size) == 0);
        semu_snapshot_writer_destroy(&after);
    }
    semu_apollo4_timer_reset(timer);
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_read(timer, 0xb4u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(c, 0u, value);
    semu_snapshot_reader_init(&reader, saved.data, saved.size);
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_snapshot_read(timer, &reader, &error));
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_read(timer, 0xb4u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(c, 0x3f000000u, value);
    SEMU_TEST_EQ_U64(c, 646u, saved.size);
    for (i = 0; i < 3; ++i) {
        size_t offset = i == 0 ? 637u : (i == 1 ? 642u : 645u);
        uint8_t byte = saved.data[offset];
        saved.data[offset] = 255u;
        semu_snapshot_reader_init(&reader, saved.data, saved.size);
        SEMU_TEST_EQ_U64(c, SEMU_ERR_FORMAT, semu_apollo4_timer_snapshot_read(timer, &reader, &error));
        saved.data[offset] = byte;
        semu_snapshot_writer_init(&after);
        SEMU_TEST_EQ_U64(c, SEMU_OK, semu_apollo4_timer_snapshot_write(timer, &after, &error));
        SEMU_TEST_ASSERT(c, saved.size == after.size && memcmp(saved.data, after.data, saved.size) == 0);
        semu_snapshot_writer_destroy(&after);
    }
    /* A stopped channel cannot retain an active limit-stall latch. */
    saved.data[29u + 8u * 38u] = 0x40u;
    saved.data[645u] = 1u;
    semu_snapshot_reader_init(&reader, saved.data, saved.size);
    SEMU_TEST_EQ_U64(c, SEMU_ERR_FORMAT, semu_apollo4_timer_snapshot_read(timer, &reader, &error));
    semu_snapshot_writer_destroy(&saved);
    semu_apollo4_timer_destroy(timer);
    semu_scheduler_destroy(clock);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_timer8_lane_and_restore),
        SEMU_TEST_CASE(test_timer8_routing_and_atomic_refusal)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
