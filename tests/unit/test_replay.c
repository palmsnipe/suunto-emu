#include "semu/trace.h"
#include "test.h"

#include <string.h>

static const char *VALID_REPLAY =
    "version=1\n"
    "profile=sapporo-2.22.60\n"
    "firmware=c8f2d9e4c114fef0774056a316ad09c42d31b95e2e956f887ed691c3c15a9bfc\n"
    "events=3\n"
    "1000000 button upper press\n"
    "2000000 button upper release\n"
    "3000000 button middle press\n";

static void test_round_trip(semu_test_context *context)
{
    semu_error err;
    semu_replay *r1, *r2;
    char buf[4096];
    size_t len;
    semu_status s;

    semu_error_clear(&err);
    r1 = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r1 != NULL);
    s = semu_replay_parse(r1, VALID_REPLAY, strlen(VALID_REPLAY), &err);
    SEMU_TEST_EQ_U64(context, SEMU_OK, s);
    SEMU_TEST_EQ_U64(context, 3u, semu_replay_event_count(r1));
    SEMU_TEST_EQ_U64(context, 1u, semu_replay_version(r1));
    SEMU_TEST_ASSERT(context,
        strcmp(semu_replay_profile_id(r1), "sapporo-2.22.60") == 0);

    len = semu_replay_format(r1, buf, sizeof(buf));
    SEMU_TEST_ASSERT(context, len > 0u);

    /* Parse the formatted output and compare. */
    r2 = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r2 != NULL);
    s = semu_replay_parse(r2, buf, len, &err);
    SEMU_TEST_EQ_U64(context, SEMU_OK, s);
    SEMU_TEST_EQ_U64(context, 3u, semu_replay_event_count(r2));
    {
        char buf2[4096];
        size_t len2 = semu_replay_format(r2, buf2, sizeof(buf2));
        SEMU_TEST_EQ_U64(context, len, len2);
        SEMU_TEST_ASSERT(context, memcmp(buf, buf2, len) == 0);
    }
    semu_replay_destroy(r1);
    semu_replay_destroy(r2);
}

static void test_byte_identical(semu_test_context *context)
{
    semu_error err;
    semu_replay *r1, *r2;
    char buf1[4096];
    char buf2[4096];
    size_t len1, len2;

    semu_error_clear(&err);
    r1 = semu_replay_create(&err);
    r2 = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r1 != NULL);
    SEMU_TEST_ASSERT(context, r2 != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_replay_parse(r1, VALID_REPLAY, strlen(VALID_REPLAY), &err));
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_replay_parse(r2, VALID_REPLAY, strlen(VALID_REPLAY), &err));

    len1 = semu_replay_format(r1, buf1, sizeof(buf1));
    len2 = semu_replay_format(r2, buf2, sizeof(buf2));
    SEMU_TEST_EQ_U64(context, len1, len2);
    SEMU_TEST_ASSERT(context, memcmp(buf1, buf2, len1) == 0);

    semu_replay_destroy(r1);
    semu_replay_destroy(r2);
}

static void test_duplicate_times(semu_test_context *context)
{
    static const char *text =
        "version=1\n"
        "profile=test\n"
        "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
        "events=2\n"
        "1000000 button upper press\n"
        "1000000 button middle press\n";
    semu_error err;
    semu_replay *r;
    char buf[4096];
    size_t len;

    semu_error_clear(&err);
    r = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_replay_parse(r, text, strlen(text), &err));
    SEMU_TEST_EQ_U64(context, 2u, semu_replay_event_count(r));

    /* Both events have the same time; file order is preserved. */
    {
        const semu_replay_event *e0 = semu_replay_event_get(r, 0u);
        const semu_replay_event *e1 = semu_replay_event_get(r, 1u);
        SEMU_TEST_ASSERT(context, e0 != NULL && e1 != NULL);
        SEMU_TEST_EQ_U64(context, 1000000u, e0->virtual_time_ns);
        SEMU_TEST_EQ_U64(context, 1000000u, e1->virtual_time_ns);
        SEMU_TEST_EQ_U64(context, SEMU_BUTTON_UPPER, e0->code);
        SEMU_TEST_EQ_U64(context, SEMU_BUTTON_MIDDLE, e1->code);
    }

    /* Format and re-parse to verify stability. */
    len = semu_replay_format(r, buf, sizeof(buf));
    SEMU_TEST_ASSERT(context, len > 0u);
    {
        semu_replay *r2 = semu_replay_create(&err);
        SEMU_TEST_ASSERT(context, r2 != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_replay_parse(r2, buf, len, &err));
        {
            char buf2[4096];
            size_t len2 = semu_replay_format(r2, buf2, sizeof(buf2));
            SEMU_TEST_EQ_U64(context, len, len2);
            SEMU_TEST_ASSERT(context, memcmp(buf, buf2, len) == 0);
        }
        semu_replay_destroy(r2);
    }
    semu_replay_destroy(r);
}

static void test_time_reversal(semu_test_context *context)
{
    static const char *text =
        "version=1\n"
        "profile=test\n"
        "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
        "events=2\n"
        "2000000 button upper press\n"
        "1000000 button upper release\n";
    semu_error err;
    semu_replay *r;
    semu_status s;

    semu_error_clear(&err);
    r = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    s = semu_replay_parse(r, text, strlen(text), &err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT, s);
    semu_replay_destroy(r);
}

static void test_bad_version(semu_test_context *context)
{
    static const char *text =
        "version=2\n"
        "profile=test\n"
        "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
        "events=0\n";
    semu_error err;
    semu_replay *r;

    semu_error_clear(&err);
    r = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_replay_parse(r, text, strlen(text), &err));
    semu_replay_destroy(r);
}

static void test_duplicate_header_refuses(semu_test_context *context)
{
    static const char invalid[] =
        "version=1\n"
        "profile=sapporo-2.22.60\n"
        "profile=other\n"
        "firmware=c8f2d9e4c114fef0774056a316ad09c42d31b95e2e956f887ed691c3c15a9bfc\n"
        "events=0\n";
    semu_error err;
    semu_replay *replay;
    char before[4096];
    char after[4096];
    size_t before_len;
    size_t after_len;

    semu_error_clear(&err);
    replay = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, replay != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_replay_parse(replay, VALID_REPLAY,
                                       strlen(VALID_REPLAY), &err));
    before_len = semu_replay_format(replay, before, sizeof(before));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_replay_parse(replay, invalid, sizeof(invalid) - 1u,
                                       &err));
    after_len = semu_replay_format(replay, after, sizeof(after));
    SEMU_TEST_EQ_U64(context, before_len, after_len);
    SEMU_TEST_ASSERT(context, memcmp(before, after, before_len) == 0);
    semu_replay_destroy(replay);
}

static void test_uint64_overflow(semu_test_context *context)
{
    static const char *header_overflow =
        "version=1\n"
        "profile=test\n"
        "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
        "events=18446744073709551616\n";
    static const char *time_overflow =
        "version=1\n"
        "profile=test\n"
        "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
        "events=1\n"
        "18446744073709551616 button upper press\n";
    semu_error err;
    semu_replay *r;

    semu_error_clear(&err);
    r = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_replay_parse(r, header_overflow, strlen(header_overflow), &err));
    SEMU_TEST_EQ_U64(context, 0u, semu_replay_event_count(r));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_replay_parse(r, time_overflow, strlen(time_overflow), &err));
    SEMU_TEST_EQ_U64(context, 0u, semu_replay_event_count(r));
    semu_replay_destroy(r);
}

static void test_refusal_is_atomic(semu_test_context *context)
{
    static const char invalid[] =
        "version=1\n"
        "profile=test\n"
        "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
        "events=1\n"
        "1000000 crown rotate -\n";
    semu_error err;
    semu_replay *r;
    char before[4096];
    char after[4096];
    size_t before_len;
    size_t after_len;

    semu_error_clear(&err);
    r = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_replay_parse(r, VALID_REPLAY, strlen(VALID_REPLAY), &err));
    before_len = semu_replay_format(r, before, sizeof(before));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_replay_parse(r, invalid, sizeof(invalid) - 1u, &err));
    after_len = semu_replay_format(r, after, sizeof(after));
    SEMU_TEST_EQ_U64(context, before_len, after_len);
    SEMU_TEST_ASSERT(context, memcmp(before, after, before_len) == 0);
    SEMU_TEST_EQ_U64(context, 3u, semu_replay_event_count(r));
    semu_replay_destroy(r);
}

static void test_bad_hash(semu_test_context *context)
{
    static const char *text =
        "version=1\n"
        "profile=test\n"
        "firmware=XYZ000000000000000000000000000000000000000000000000000000000000\n"
        "events=0\n";
    semu_error err;
    semu_replay *r;

    semu_error_clear(&err);
    r = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_replay_parse(r, text, strlen(text), &err));
    semu_replay_destroy(r);
}

static void test_bad_hash_length(semu_test_context *context)
{
    static const char *text =
        "version=1\n"
        "profile=test\n"
        "firmware=abc\n"
        "events=0\n";
    semu_error err;
    semu_replay *r;

    semu_error_clear(&err);
    r = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_replay_parse(r, text, strlen(text), &err));
    semu_replay_destroy(r);
}

static void test_unknown_kind(semu_test_context *context)
{
    static const char *text =
        "version=1\n"
        "profile=test\n"
        "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
        "events=1\n"
        "1000000 sensor upper press\n";
    semu_error err;
    semu_replay *r;

    semu_error_clear(&err);
    r = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_replay_parse(r, text, strlen(text), &err));
    semu_replay_destroy(r);
}

static void test_unknown_button_code(semu_test_context *context)
{
    static const char *text =
        "version=1\n"
        "profile=test\n"
        "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
        "events=1\n"
        "1000000 button sideways press\n";
    semu_error err;
    semu_replay *r;

    semu_error_clear(&err);
    r = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_replay_parse(r, text, strlen(text), &err));
    semu_replay_destroy(r);
}

static void test_missing_header(semu_test_context *context)
{
    static const char *text =
        "version=1\n"
        "profile=test\n"
        "events=0\n";
    semu_error err;
    semu_replay *r;

    semu_error_clear(&err);
    r = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_replay_parse(r, text, strlen(text), &err));
    semu_replay_destroy(r);
}

static void test_event_count_mismatch(semu_test_context *context)
{
    static const char *text =
        "version=1\n"
        "profile=test\n"
        "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
        "events=3\n"
        "1000000 button upper press\n"
        "2000000 button upper release\n";
    semu_error err;
    semu_replay *r;

    semu_error_clear(&err);
    r = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_replay_parse(r, text, strlen(text), &err));
    semu_replay_destroy(r);
}

static void test_crown_and_touch(semu_test_context *context)
{
    static const char *text =
        "version=1\n"
        "profile=test\n"
        "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
        "events=3\n"
        "1000000 crown rotate -5\n"
        "2000000 touch 1 120 240\n"
        "3000000 touch 0 0 0\n";
    semu_error err;
    semu_replay *r;
    char buf[4096];
    size_t len;

    semu_error_clear(&err);
    r = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_replay_parse(r, text, strlen(text), &err));
    SEMU_TEST_EQ_U64(context, 3u, semu_replay_event_count(r));

    {
        const semu_replay_event *e0 = semu_replay_event_get(r, 0u);
        const semu_replay_event *e1 = semu_replay_event_get(r, 1u);
        const semu_replay_event *e2 = semu_replay_event_get(r, 2u);
        SEMU_TEST_ASSERT(context, e0 != NULL && e1 != NULL && e2 != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_INPUT_CROWN_ROTATE, e0->kind);
        SEMU_TEST_EQ_U64(context, (uint64_t)-5, (uint64_t)e0->value);
        SEMU_TEST_EQ_U64(context, SEMU_INPUT_TOUCH, e1->kind);
        SEMU_TEST_EQ_U64(context, 1, e1->value);
        SEMU_TEST_EQ_U64(context, 120, e1->x);
        SEMU_TEST_EQ_U64(context, 240, e1->y);
        SEMU_TEST_EQ_U64(context, SEMU_INPUT_TOUCH, e2->kind);
        SEMU_TEST_EQ_U64(context, 0, e2->value);
    }

    /* Round-trip stability. */
    len = semu_replay_format(r, buf, sizeof(buf));
    SEMU_TEST_ASSERT(context, len > 0u);
    {
        semu_replay *r2 = semu_replay_create(&err);
        SEMU_TEST_ASSERT(context, r2 != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_replay_parse(r2, buf, len, &err));
        {
            char buf2[4096];
            size_t len2 = semu_replay_format(r2, buf2, sizeof(buf2));
            SEMU_TEST_EQ_U64(context, len, len2);
            SEMU_TEST_ASSERT(context, memcmp(buf, buf2, len) == 0);
        }
        semu_replay_destroy(r2);
    }
    semu_replay_destroy(r);
}

static void test_comments_and_blanks(semu_test_context *context)
{
    static const char *text =
        "# replay file\n"
        "version=1\n"
        "\n"
        "profile=test\n"
        "# identity\n"
        "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
        "events=1\n"
        "\n"
        "1000000 button upper press\n";
    semu_error err;
    semu_replay *r;

    semu_error_clear(&err);
    r = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_replay_parse(r, text, strlen(text), &err));
    SEMU_TEST_EQ_U64(context, 1u, semu_replay_event_count(r));
    semu_replay_destroy(r);
}

static void test_null_safety(semu_test_context *context)
{
    semu_error err;
    char buf[16];

    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        semu_replay_parse(NULL, "x", 1u, &err));
    {
        semu_replay *r = semu_replay_create(&err);
        SEMU_TEST_ASSERT(context, r != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
            semu_replay_parse(r, NULL, 0u, &err));
        semu_replay_destroy(r);
    }
    SEMU_TEST_EQ_U64(context, 0u, semu_replay_event_count(NULL));
    SEMU_TEST_ASSERT(context, semu_replay_event_get(NULL, 0u) == NULL);
    SEMU_TEST_ASSERT(context, semu_replay_profile_id(NULL) == NULL);
    SEMU_TEST_ASSERT(context, semu_replay_firmware_hash(NULL) == NULL);
    SEMU_TEST_EQ_U64(context, 0u, semu_replay_version(NULL));
    SEMU_TEST_EQ_U64(context, 0u, semu_replay_format(NULL, buf, sizeof(buf)));
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_round_trip),
        SEMU_TEST_CASE(test_byte_identical),
        SEMU_TEST_CASE(test_duplicate_times),
        SEMU_TEST_CASE(test_time_reversal),
        SEMU_TEST_CASE(test_bad_version),
        SEMU_TEST_CASE(test_duplicate_header_refuses),
        SEMU_TEST_CASE(test_uint64_overflow),
        SEMU_TEST_CASE(test_refusal_is_atomic),
        SEMU_TEST_CASE(test_bad_hash),
        SEMU_TEST_CASE(test_bad_hash_length),
        SEMU_TEST_CASE(test_unknown_kind),
        SEMU_TEST_CASE(test_unknown_button_code),
        SEMU_TEST_CASE(test_missing_header),
        SEMU_TEST_CASE(test_event_count_mismatch),
        SEMU_TEST_CASE(test_crown_and_touch),
        SEMU_TEST_CASE(test_comments_and_blanks),
        SEMU_TEST_CASE(test_null_safety)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
