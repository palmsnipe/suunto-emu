#include "test.h"

/*
 * semantic_input.c lives in src/frontends/ which is excluded from
 * libsemu.a; include the implementation directly for unit testing.
 */
#include "../../src/frontends/semantic_input.c"

#include <string.h>

static semu_normalized_key *make_key(semu_normalized_key *k, uint32_t key,
    int32_t down, int32_t repeat)
{
    memset(k, 0, sizeof(*k));
    k->key = key;
    k->down = down;
    k->repeat = repeat;
    return k;
}

static void test_press_all_three(semu_test_context *context)
{
    semu_error err;
    semu_input_mapper *m;
    semu_normalized_key nk;
    semu_input_event ev;
    int32_t has;
    semu_error_clear(&err);
    m = semu_input_mapper_create(&err);
    SEMU_TEST_ASSERT(context, m != NULL);

    has = 0; memset(&ev, 0, sizeof(ev));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_input_mapper_process(m, make_key(&nk, SEMU_INPUT_KEY_UPPER, 1, 0),
                                   &ev, &has, &err));
    SEMU_TEST_EQ_U64(context, 1u, has);
    SEMU_TEST_EQ_U64(context, SEMU_INPUT_BUTTON, ev.kind);
    SEMU_TEST_EQ_U64(context, SEMU_BUTTON_UPPER, ev.code);
    SEMU_TEST_EQ_U64(context, 0u, ev.value);

    has = 0; memset(&ev, 0, sizeof(ev));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_input_mapper_process(m, make_key(&nk, SEMU_INPUT_KEY_MIDDLE, 1, 0),
                                   &ev, &has, &err));
    SEMU_TEST_EQ_U64(context, 1u, has);
    SEMU_TEST_EQ_U64(context, SEMU_BUTTON_MIDDLE, ev.code);

    has = 0; memset(&ev, 0, sizeof(ev));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_input_mapper_process(m, make_key(&nk, SEMU_INPUT_KEY_LOWER, 1, 0),
                                   &ev, &has, &err));
    SEMU_TEST_EQ_U64(context, 1u, has);
    SEMU_TEST_EQ_U64(context, SEMU_BUTTON_LOWER, ev.code);
    semu_input_mapper_destroy(m);
}

static void test_release_after_press(semu_test_context *context)
{
    semu_error err;
    semu_input_mapper *m;
    semu_normalized_key nk;
    semu_input_event ev;
    int32_t has;
    semu_error_clear(&err);
    m = semu_input_mapper_create(&err);
    SEMU_TEST_ASSERT(context, m != NULL);

    semu_input_mapper_process(m, make_key(&nk, SEMU_INPUT_KEY_UPPER, 1, 0),
                               &ev, &has, &err);
    SEMU_TEST_EQ_U64(context, 1u, has);

    has = 0; memset(&ev, 0xFF, sizeof(ev));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_input_mapper_process(m, make_key(&nk, SEMU_INPUT_KEY_UPPER, 0, 0),
                                   &ev, &has, &err));
    SEMU_TEST_EQ_U64(context, 1u, has);
    SEMU_TEST_EQ_U64(context, SEMU_INPUT_BUTTON, ev.kind);
    SEMU_TEST_EQ_U64(context, SEMU_BUTTON_UPPER, ev.code);
    SEMU_TEST_EQ_U64(context, 1u, ev.value);
    semu_input_mapper_destroy(m);
}

static void test_repeat_ignored(semu_test_context *context)
{
    semu_error err;
    semu_input_mapper *m;
    semu_normalized_key nk;
    semu_input_event ev;
    int32_t has;
    semu_error_clear(&err);
    m = semu_input_mapper_create(&err);
    SEMU_TEST_ASSERT(context, m != NULL);

    has = 0; memset(&ev, 0, sizeof(ev));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_input_mapper_process(m, make_key(&nk, SEMU_INPUT_KEY_MIDDLE, 1, 1),
                                   &ev, &has, &err));
    SEMU_TEST_EQ_U64(context, 0u, has);
    semu_input_mapper_destroy(m);
}

static void test_duplicate_press_suppressed(semu_test_context *context)
{
    semu_error err;
    semu_input_mapper *m;
    semu_normalized_key nk;
    semu_input_event ev;
    int32_t has;
    semu_error_clear(&err);
    m = semu_input_mapper_create(&err);
    SEMU_TEST_ASSERT(context, m != NULL);

    semu_input_mapper_process(m, make_key(&nk, SEMU_INPUT_KEY_UPPER, 1, 0),
                               &ev, &has, &err);
    SEMU_TEST_EQ_U64(context, 1u, has);

    has = 1;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_input_mapper_process(m, make_key(&nk, SEMU_INPUT_KEY_UPPER, 1, 0),
                                   &ev, &has, &err));
    SEMU_TEST_EQ_U64(context, 0u, has);
    semu_input_mapper_destroy(m);
}

static void test_duplicate_release_suppressed(semu_test_context *context)
{
    semu_error err;
    semu_input_mapper *m;
    semu_normalized_key nk;
    semu_input_event ev;
    int32_t has;
    semu_error_clear(&err);
    m = semu_input_mapper_create(&err);
    SEMU_TEST_ASSERT(context, m != NULL);

    has = 1;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_input_mapper_process(m, make_key(&nk, SEMU_INPUT_KEY_LOWER, 0, 0),
                                   &ev, &has, &err));
    SEMU_TEST_EQ_U64(context, 0u, has);
    semu_input_mapper_destroy(m);
}

static void test_focus_loss_order(semu_test_context *context)
{
    semu_error err;
    semu_input_mapper *m;
    semu_normalized_key nk;
    semu_input_event ev;
    semu_input_event releases[3];
    uint32_t count;
    int32_t has;
    semu_error_clear(&err);
    m = semu_input_mapper_create(&err);
    SEMU_TEST_ASSERT(context, m != NULL);

    /* Press in lower, middle, upper order */
    semu_input_mapper_process(m, make_key(&nk, SEMU_INPUT_KEY_LOWER, 1, 0),
                               &ev, &has, &err);
    semu_input_mapper_process(m, make_key(&nk, SEMU_INPUT_KEY_MIDDLE, 1, 0),
                               &ev, &has, &err);
    semu_input_mapper_process(m, make_key(&nk, SEMU_INPUT_KEY_UPPER, 1, 0),
                               &ev, &has, &err);

    count = 0;
    memset(releases, 0, sizeof(releases));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_input_mapper_focus_loss(m, releases, 3u, &count, &err));
    SEMU_TEST_EQ_U64(context, 3u, count);
    /* Must be in upper/middle/lower order */
    SEMU_TEST_EQ_U64(context, SEMU_BUTTON_UPPER, releases[0].code);
    SEMU_TEST_EQ_U64(context, 1u, releases[0].value);
    SEMU_TEST_EQ_U64(context, SEMU_BUTTON_MIDDLE, releases[1].code);
    SEMU_TEST_EQ_U64(context, 1u, releases[1].value);
    SEMU_TEST_EQ_U64(context, SEMU_BUTTON_LOWER, releases[2].code);
    SEMU_TEST_EQ_U64(context, 1u, releases[2].value);
    semu_input_mapper_destroy(m);
}

static void test_unmapped_key(semu_test_context *context)
{
    semu_error err;
    semu_input_mapper *m;
    semu_normalized_key nk;
    semu_input_event ev;
    int32_t has;
    semu_error_clear(&err);
    m = semu_input_mapper_create(&err);
    SEMU_TEST_ASSERT(context, m != NULL);

    has = 1;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_input_mapper_process(m, make_key(&nk, 999u, 1, 0),
                                   &ev, &has, &err));
    SEMU_TEST_EQ_U64(context, 0u, has);
    semu_input_mapper_destroy(m);
}

static void test_invalid_map_duplicate(semu_test_context *context)
{
    semu_error err;
    semu_input_mapper *m;
    semu_input_key_map_entry entries[] = {
        { 10u, SEMU_BUTTON_UPPER },
        { 10u, SEMU_BUTTON_MIDDLE },
    };
    semu_error_clear(&err);
    m = semu_input_mapper_create(&err);
    SEMU_TEST_ASSERT(context, m != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        semu_input_mapper_set_map(m, entries, 2u, &err));
    semu_input_mapper_destroy(m);
}

static void test_invalid_map_button_range(semu_test_context *context)
{
    semu_error err;
    semu_input_mapper *m;
    semu_input_key_map_entry entries[] = {
        { 10u, (semu_button_id)99 },
    };
    semu_error_clear(&err);
    m = semu_input_mapper_create(&err);
    SEMU_TEST_ASSERT(context, m != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        semu_input_mapper_set_map(m, entries, 1u, &err));
    semu_input_mapper_destroy(m);
}

static void test_reset_clears_state(semu_test_context *context)
{
    semu_error err;
    semu_input_mapper *m;
    semu_normalized_key nk;
    semu_input_event ev;
    int32_t has;
    semu_error_clear(&err);
    m = semu_input_mapper_create(&err);
    SEMU_TEST_ASSERT(context, m != NULL);

    semu_input_mapper_process(m, make_key(&nk, SEMU_INPUT_KEY_UPPER, 1, 0),
                               &ev, &has, &err);
    SEMU_TEST_EQ_U64(context, 1u, has);

    semu_input_mapper_reset(m);

    has = 1;
    semu_input_mapper_process(m, make_key(&nk, SEMU_INPUT_KEY_UPPER, 0, 0),
                               &ev, &has, &err);
    SEMU_TEST_EQ_U64(context, 0u, has);
    semu_input_mapper_destroy(m);
}

static void test_repeat_hash(semu_test_context *context)
{
    semu_error err;
    semu_input_mapper *m1;
    semu_input_mapper *m2;
    semu_normalized_key nk;
    semu_input_event ev1[8];
    semu_input_event ev2[8];
    uint32_t i;
    uint32_t n1 = 0u;
    uint32_t n2 = 0u;
    int32_t has;
    static const struct {
        uint32_t key;
        int32_t down;
        int32_t repeat;
    } sequence[] = {
        { SEMU_INPUT_KEY_UPPER, 1, 0 },
        { SEMU_INPUT_KEY_UPPER, 1, 1 },
        { SEMU_INPUT_KEY_MIDDLE, 1, 0 },
        { SEMU_INPUT_KEY_LOWER, 1, 0 },
        { SEMU_INPUT_KEY_UPPER, 0, 0 },
        { SEMU_INPUT_KEY_MIDDLE, 0, 0 },
        { SEMU_INPUT_KEY_LOWER, 0, 0 },
        { 999u, 1, 0 },
    };
    semu_error_clear(&err);
    m1 = semu_input_mapper_create(&err);
    m2 = semu_input_mapper_create(&err);
    SEMU_TEST_ASSERT(context, m1 != NULL && m2 != NULL);

    memset(ev1, 0, sizeof(ev1));
    memset(ev2, 0, sizeof(ev2));
    for (i = 0u; i < sizeof(sequence) / sizeof(sequence[0]); ++i) {
        has = 0;
        semu_input_mapper_process(m1,
            make_key(&nk, sequence[i].key, sequence[i].down, sequence[i].repeat),
            &ev1[n1], &has, &err);
        n1 += has ? 1u : 0u;

        has = 0;
        semu_input_mapper_process(m2,
            make_key(&nk, sequence[i].key, sequence[i].down, sequence[i].repeat),
            &ev2[n2], &has, &err);
        n2 += has ? 1u : 0u;
    }
    SEMU_TEST_EQ_U64(context, n1, n2);
    SEMU_TEST_EQ_U64(context, 0u, memcmp(ev1, ev2, n1 * sizeof(ev1[0])));
    semu_input_mapper_destroy(m1);
    semu_input_mapper_destroy(m2);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_press_all_three),
        SEMU_TEST_CASE(test_release_after_press),
        SEMU_TEST_CASE(test_repeat_ignored),
        SEMU_TEST_CASE(test_duplicate_press_suppressed),
        SEMU_TEST_CASE(test_duplicate_release_suppressed),
        SEMU_TEST_CASE(test_focus_loss_order),
        SEMU_TEST_CASE(test_unmapped_key),
        SEMU_TEST_CASE(test_invalid_map_duplicate),
        SEMU_TEST_CASE(test_invalid_map_button_range),
        SEMU_TEST_CASE(test_reset_clears_state),
        SEMU_TEST_CASE(test_repeat_hash)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
