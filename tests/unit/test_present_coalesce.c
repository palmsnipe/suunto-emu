#include "../../src/display/present_coalesce.h"
#include "semu/types.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

#define W 4u
#define STRIDE (W * 2u)
#define SIZE (STRIDE * W)
#define THRESHOLD UINT64_C(10000000)

static semu_frame make_frame(const uint8_t *px, uint64_t generation)
{
    semu_frame f;
    f.format = SEMU_PIXEL_RGB565_LE;
    f.width = W;
    f.height = W;
    f.stride = STRIDE;
    f.generation = generation;
    f.pixels = px;
    f.size = SIZE;
    return f;
}

static void test_burst_release_and_flush(semu_test_context *context)
{
    semu_error error;
    semu_present_coalescer *co = semu_present_coalescer_create(THRESHOLD,
        &error);
    static const uint8_t a_px[SIZE] = {0xA1u}, b_px[SIZE] = {0xB2u};
    static const uint8_t c_px[SIZE] = {0xC3u}, d_px[SIZE] = {0xD4u};
    const semu_frame *out;
    semu_frame a = make_frame(a_px, 1u), b = make_frame(b_px, 2u);
    semu_frame c = make_frame(c_px, 3u), d = make_frame(d_px, 4u);
    SEMU_TEST_ASSERT(context, co != NULL);

    SEMU_TEST_ASSERT(context, semu_present_coalescer_observe(co, &a, 0u,
        &error) == NULL);
    SEMU_TEST_ASSERT(context, semu_present_coalescer_observe(co, &b, 1000000u,
        &error) == NULL);
    SEMU_TEST_ASSERT(context, semu_present_coalescer_observe(co, &c, 2000000u,
        &error) == NULL);
    /* Next burst start releases the burst-final frame C. */
    out = semu_present_coalescer_observe(co, &d, 25000000u, &error);
    SEMU_TEST_ASSERT(context, out != NULL);
    SEMU_TEST_EQ_U64(context, 3u, out->generation);
    SEMU_TEST_ASSERT(context, memcmp(out->pixels, c_px, SIZE) == 0);
    /* In-burst replacement must not release anything. */
    SEMU_TEST_ASSERT(context, semu_present_coalescer_observe(co, &a,
        30000000u, &error) == NULL);
    out = semu_present_coalescer_observe(co, &b, 50000000u, &error);
    SEMU_TEST_ASSERT(context, out != NULL);
    SEMU_TEST_EQ_U64(context, 1u, out->generation);
    SEMU_TEST_ASSERT(context, memcmp(out->pixels, a_px, SIZE) == 0);
    out = semu_present_coalescer_flush(co);
    SEMU_TEST_ASSERT(context, out != NULL);
    SEMU_TEST_EQ_U64(context, 2u, out->generation);
    SEMU_TEST_ASSERT(context, semu_present_coalescer_flush(co) == NULL);
    semu_present_coalescer_destroy(co);
}

static void test_time_discontinuity_releases(semu_test_context *context)
{
    semu_error error;
    semu_present_coalescer *co = semu_present_coalescer_create(THRESHOLD,
        &error);
    const semu_frame *out;
    static const uint8_t a_px[SIZE] = {0x11u}, b_px[SIZE] = {0x22u};
    semu_frame a = make_frame(a_px, 1u), b = make_frame(b_px, 2u);
    SEMU_TEST_ASSERT(context, co != NULL);
    SEMU_TEST_ASSERT(context, semu_present_coalescer_observe(co, &a,
        100000000u, &error) == NULL);
    out = semu_present_coalescer_observe(co, &b, 50000000u, &error);
    SEMU_TEST_ASSERT(context, out != NULL);
    SEMU_TEST_EQ_U64(context, 1u, out->generation);
    semu_present_coalescer_destroy(co);
}

static void test_invalid_frames_refused(semu_test_context *context)
{
    semu_error error;
    semu_present_coalescer *co = semu_present_coalescer_create(THRESHOLD,
        &error);
    static const uint8_t a_px[SIZE] = {0x33u};
    semu_frame a = make_frame(a_px, 1u);
    semu_frame bad;
    SEMU_TEST_ASSERT(context, co != NULL);
    SEMU_TEST_ASSERT(context, semu_present_coalescer_observe(co, &a, 0u,
        &error) == NULL);
    bad = a;
    bad.pixels = NULL;
    SEMU_TEST_ASSERT(context, semu_present_coalescer_observe(co, &bad,
        100000000u, &error) == NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT, (uint64_t)error.code);
    bad = a;
    bad.stride = W; /* below width*2 */
    SEMU_TEST_ASSERT(context, semu_present_coalescer_observe(co, &bad,
        100000000u, &error) == NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT, (uint64_t)error.code);
    /* Refusals left the hold untouched: a valid late frame still releases
     * the original held frame. */
    {
        const semu_frame *out = semu_present_coalescer_observe(co, &a,
            200000000u, &error);
        SEMU_TEST_ASSERT(context, out != NULL);
        SEMU_TEST_EQ_U64(context, 1u, out->generation);
    }
    SEMU_TEST_ASSERT(context, semu_present_coalescer_observe(NULL, &a, 0u,
        &error) == NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT, (uint64_t)error.code);
    SEMU_TEST_ASSERT(context, semu_present_coalescer_flush(NULL) == NULL);
    semu_present_coalescer_destroy(co);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_burst_release_and_flush),
        SEMU_TEST_CASE(test_time_discontinuity_releases),
        SEMU_TEST_CASE(test_invalid_frames_refused)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
