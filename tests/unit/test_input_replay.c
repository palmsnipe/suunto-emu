#include "test.h"

/*
 * input_replay.c lives in src/frontends/ which is excluded from
 * libsemu.a; include the implementation directly for unit testing.
 */
#include "../../src/frontends/input_replay.c"

#include <stdio.h>
#include <string.h>

typedef struct {
    semu_input_event events[16u];
    uint64_t times[16u];
    uint32_t ordinals[16u];
    size_t count;
    int refuse_at;
} sink_ctx;

static int test_sink(void *context, const semu_input_event *event,
    uint64_t time_ns, uint32_t ordinal)
{
    sink_ctx *ctx = (sink_ctx *)context;
    if (ctx->refuse_at > 0 && ctx->count >= (size_t)ctx->refuse_at) {
        return 1;
    }
    if (ctx->count < 16u) {
        ctx->events[ctx->count] = *event;
        ctx->times[ctx->count] = time_ns;
        ctx->ordinals[ctx->count] = ordinal;
    }
    ++ctx->count;
    return 0;
}

static void test_valid_sequence(semu_test_context *context)
{
    semu_error err;
    semu_input_replay *r;
    sink_ctx sc = {0};
    static const char text[] =
        "# replay\n"
        "1000 button upper press\n"
        "2000 button middle press\n"
        "3000 button middle release\n"
        "4000 button upper release\n";
    semu_error_clear(&err);
    r = semu_input_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_input_replay_parse(r, text, sizeof(text) - 1u, &err));
    SEMU_TEST_EQ_U64(context, 4u, semu_input_replay_count(r));
    SEMU_TEST_EQ_U64(context, 4u,
        semu_input_replay_pump(r, 5000u, test_sink, &sc, NULL));
    SEMU_TEST_EQ_U64(context, SEMU_BUTTON_UPPER, sc.events[0].code);
    SEMU_TEST_EQ_U64(context, 0u, sc.events[0].value);
    SEMU_TEST_EQ_U64(context, SEMU_BUTTON_MIDDLE, sc.events[1].code);
    SEMU_TEST_EQ_U64(context, 1000u, sc.times[0]);
    SEMU_TEST_EQ_U64(context, 4000u, sc.times[3]);
    semu_input_replay_destroy(r);
}

static void test_equal_time_stable_order(semu_test_context *context)
{
    semu_error err;
    semu_input_replay *r;
    sink_ctx sc = {0};
    static const char text[] =
        "1000 button upper press\n"
        "2000 button upper release\n"
        "2001 button lower press\n"
        "2002 button lower release\n";
    semu_error_clear(&err);
    r = semu_input_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_input_replay_parse(r, text, sizeof(text) - 1u, &err));
    SEMU_TEST_EQ_U64(context, 4u,
        semu_input_replay_pump(r, 3000u, test_sink, &sc, NULL));
    SEMU_TEST_EQ_U64(context, 0u, sc.ordinals[0]);
    SEMU_TEST_EQ_U64(context, 1u, sc.ordinals[1]);
    SEMU_TEST_EQ_U64(context, 2u, sc.ordinals[2]);
    SEMU_TEST_EQ_U64(context, 3u, sc.ordinals[3]);
    semu_input_replay_destroy(r);
}

static void test_malformed(semu_test_context *context)
{
    semu_error err;
    semu_input_replay *r;
    static const char valid[] = "1000 button upper press\n";
    static const char malformed[] = "2000 button upper release\nbad\n";
    static const char overflow[] =
        "18446744073709551616 button upper press\n";
    semu_error_clear(&err);
    r = semu_input_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_input_replay_parse(r, valid, sizeof(valid) - 1u, &err));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_input_replay_parse(r, malformed, sizeof(malformed) - 1u, &err));
    SEMU_TEST_EQ_U64(context, 1u, semu_input_replay_count(r));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_input_replay_parse(r, overflow, sizeof(overflow) - 1u, &err));
    SEMU_TEST_EQ_U64(context, 1u, semu_input_replay_count(r));
    semu_input_replay_destroy(r);
}

static void test_out_of_order(semu_test_context *context)
{
    semu_error err;
    semu_input_replay *r;
    static const char text[] =
        "2000 button upper press\n"
        "1000 button upper release\n";
    semu_error_clear(&err);
    r = semu_input_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_input_replay_parse(r, text, sizeof(text) - 1u, &err));
    semu_input_replay_destroy(r);
}

static void test_duplicate_time(semu_test_context *context)
{
    semu_error err;
    semu_input_replay *r;
    static const char text[] =
        "1000 button upper press\n"
        "1000 button lower press\n";
    semu_error_clear(&err);
    r = semu_input_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_input_replay_parse(r, text, sizeof(text) - 1u, &err));
    semu_input_replay_destroy(r);
}

static void test_impossible_release(semu_test_context *context)
{
    semu_error err;
    semu_input_replay *r;
    static const char text[] = "1000 button upper release\n";
    semu_error_clear(&err);
    r = semu_input_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_input_replay_parse(r, text, sizeof(text) - 1u, &err));
    semu_input_replay_destroy(r);
}

static void test_duplicate_press(semu_test_context *context)
{
    semu_error err;
    semu_input_replay *r;
    static const char text[] =
        "1000 button upper press\n"
        "2000 button upper press\n";
    semu_error_clear(&err);
    r = semu_input_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_input_replay_parse(r, text, sizeof(text) - 1u, &err));
    semu_input_replay_destroy(r);
}

static void test_sink_refusal(semu_test_context *context)
{
    semu_error err;
    semu_input_replay *r;
    sink_ctx sc = {0};
    int refused = 0;
    static const char text[] =
        "1000 button upper press\n"
        "2000 button upper release\n"
        "3000 button lower press\n"
        "4000 button lower release\n";
    semu_error_clear(&err);
    r = semu_input_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    semu_input_replay_parse(r, text, sizeof(text) - 1u, &err);
    sc.refuse_at = 2;
    SEMU_TEST_EQ_U64(context, 2u,
        semu_input_replay_pump(r, 5000u, test_sink, &sc, &refused));
    SEMU_TEST_EQ_U64(context, 1, refused);
    semu_input_replay_destroy(r);
}

static void test_reset(semu_test_context *context)
{
    semu_error err;
    semu_input_replay *r;
    sink_ctx sc = {0};
    static const char text[] =
        "1000 button upper press\n"
        "2000 button upper release\n";
    semu_error_clear(&err);
    r = semu_input_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    semu_input_replay_parse(r, text, sizeof(text) - 1u, &err);
    semu_input_replay_reset(r);
    SEMU_TEST_EQ_U64(context, 0u, semu_input_replay_count(r));
    SEMU_TEST_EQ_U64(context, 0u,
        semu_input_replay_pump(r, 5000u, test_sink, &sc, NULL));
    semu_input_replay_destroy(r);
}

static void test_partial_pump(semu_test_context *context)
{
    semu_error err;
    semu_input_replay *r;
    sink_ctx sc = {0};
    static const char text[] =
        "1000 button upper press\n"
        "2000 button upper release\n";
    semu_error_clear(&err);
    r = semu_input_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    semu_input_replay_parse(r, text, sizeof(text) - 1u, &err);
    SEMU_TEST_EQ_U64(context, 1u,
        semu_input_replay_pump(r, 1500u, test_sink, &sc, NULL));
    SEMU_TEST_EQ_U64(context, 1u,
        semu_input_replay_pump(r, 3000u, test_sink, &sc, NULL));
    SEMU_TEST_EQ_U64(context, 2u, sc.count);
    semu_input_replay_destroy(r);
}

static void test_file_parse_does_not_truncate(semu_test_context *context)
{
    const char *path = "/tmp/suunto-emu-input-replay-test.txt";
    semu_error err;
    semu_input_replay *r;
    FILE *stream;
    size_t i;
    static const char suffix[] =
        "1000 button upper press\n"
        "2000 button upper release\n";

    semu_error_clear(&err);
    stream = fopen(path, "wb");
    SEMU_TEST_ASSERT(context, stream != NULL);
    if (stream == NULL) {
        return;
    }
    for (i = 0u; i < 9000u; ++i) {
        (void)fputc('#', stream);
    }
    (void)fputc('\n', stream);
    (void)fwrite(suffix, 1u, sizeof(suffix) - 1u, stream);
    SEMU_TEST_EQ_U64(context, 0u, (uint64_t)fclose(stream));

    r = semu_input_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    if (r != NULL) {
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_input_replay_parse_file(r, path, &err));
        SEMU_TEST_EQ_U64(context, 2u, semu_input_replay_count(r));

        stream = fopen(path, "wb");
        SEMU_TEST_ASSERT(context, stream != NULL);
        if (stream != NULL) {
            SEMU_TEST_EQ_U64(context, 0u,
                (uint64_t)fseek(stream,
                    (long)INPUT_REPLAY_MAX_FILE_SIZE + 1L, SEEK_SET));
            (void)fputc('x', stream);
            SEMU_TEST_EQ_U64(context, 0u, (uint64_t)fclose(stream));
            SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
                semu_input_replay_parse_file(r, path, &err));
            SEMU_TEST_EQ_U64(context, 2u, semu_input_replay_count(r));
        }
        semu_input_replay_destroy(r);
    }
    (void)remove(path);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_valid_sequence),
        SEMU_TEST_CASE(test_equal_time_stable_order),
        SEMU_TEST_CASE(test_malformed),
        SEMU_TEST_CASE(test_out_of_order),
        SEMU_TEST_CASE(test_duplicate_time),
        SEMU_TEST_CASE(test_impossible_release),
        SEMU_TEST_CASE(test_duplicate_press),
        SEMU_TEST_CASE(test_sink_refusal),
        SEMU_TEST_CASE(test_reset),
        SEMU_TEST_CASE(test_partial_pump),
        SEMU_TEST_CASE(test_file_parse_does_not_truncate)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
