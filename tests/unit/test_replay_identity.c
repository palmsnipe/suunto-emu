#include "semu/trace.h"
#include "test.h"

#include <stddef.h>

static void test_embedded_nul_profile_refuses(semu_test_context *context)
{
    static const char invalid[] =
        "version=1\n"
        "profile=ok\0hidden\n"
        "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
        "events=0\n";
    semu_error error;
    semu_replay *replay;

    semu_error_clear(&error);
    replay = semu_replay_create(&error);
    SEMU_TEST_ASSERT(context, replay != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_replay_parse(replay, invalid, sizeof(invalid) - 1u,
                                       &error));
    SEMU_TEST_EQ_U64(context, 0u, semu_replay_event_count(replay));
    semu_replay_destroy(replay);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_embedded_nul_profile_refuses)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
