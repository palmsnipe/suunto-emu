#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semu/hash.h"
#include "semu/machine.h"
#include "test.h"

#define PANEL_SIZE 466u

static const char hello_hash[] =
    "9d02076e5fe66dad7ceccf7b4b86c1c59ab5e5f2948906868ba87a1df7ff9c76";

typedef struct {
    uint64_t first_match;
    unsigned frames;
    unsigned matches;
    int invalid;
} capture;

static void capture_frame(void *context, const semu_frame *frame)
{
    capture *result = context;
    uint8_t digest[SEMU_SHA256_SIZE];
    char hash[65];

    ++result->frames;
    if (frame->format != SEMU_PIXEL_RGB565_LE || frame->width != PANEL_SIZE ||
        frame->height != PANEL_SIZE || frame->stride != PANEL_SIZE * 2u ||
        frame->size != PANEL_SIZE * PANEL_SIZE * 2u || frame->pixels == NULL ||
        frame->generation != result->frames) {
        result->invalid = 1;
        return;
    }
    semu_sha256(frame->pixels, frame->size, digest);
    semu_sha256_format(digest, hash);
    if (strcmp(hash, hello_hash) == 0) {
        if (result->matches == 0u) result->first_match = frame->generation;
        ++result->matches;
    }
}

static void check_transcript(semu_test_context *context, FILE *log)
{
    static const char *const expected[] = {
        "trigger=first-open-status", "trigger=ohr2-boot-mode", "trigger=ver-status",
        "trigger=reopen-status", "trigger=ohr2-boot-mode", "trigger=gsr-status",
        "trigger=gps-awake", "trigger=gps-awake"
    };
    char line[512];
    size_t hits = 0u;
    int reset_or_refusal = 0;

    rewind(log);
    while (fgets(line, sizeof(line), log) != NULL) {
        if (strstr(line, "machine-reset-request") != NULL ||
            strstr(line, "draw-refused") != NULL)
            reset_or_refusal = 1;
        if (strstr(line, "event=layer-hit layer=ulsan-2.44-gps-startup ") == NULL)
            continue;
        SEMU_TEST_ASSERT(context, hits < SEMU_ARRAY_LEN(expected));
        SEMU_TEST_ASSERT(context, strstr(line, expected[hits]) != NULL);
        ++hits;
    }
    SEMU_TEST_EQ_U64(context, 0u, ferror(log));
    SEMU_TEST_EQ_U64(context, 0u, reset_or_refusal);
    SEMU_TEST_EQ_U64(context, SEMU_ARRAY_LEN(expected), hits);
}

static void test_ulsan_244_hello_frame(semu_test_context *context)
{
    static const char *const layers[] = {
        "ulsan-2.44-production-data", "ulsan-2.44-emmc", "ulsan-2.44-gps-startup"
    };
    const char *manifest = getenv("SEMU_ULSAN_244_FIRMWARE_MANIFEST");
    const char *image = getenv("SEMU_ULSAN_244_EMMC_IMAGE");
    const semu_run_limits limits = {UINT64_C(2000000000), UINT64_C(31000000000)};
    semu_profile profile;
    semu_firmware_manifest firmware;
    semu_machine_options options = {0};
    semu_error error;
    unsigned boot;

    if (manifest == NULL || *manifest == '\0' || image == NULL || *image == '\0') {
        puts("SKIP Race S Hello capture: set SEMU_ULSAN_244_FIRMWARE_MANIFEST and SEMU_ULSAN_244_EMMC_IMAGE");
        return;
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_profile_load(
        "profiles/ulsan/2.44.52/profile.semu", &profile, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_manifest_load(manifest, &firmware, &error));
    options.profile = &profile;
    options.firmware = &firmware;
    options.layers = layers;
    options.layer_count = SEMU_ARRAY_LEN(layers);
    options.ulsan_emmc_path = image;
    options.frame_callback = capture_frame;

    for (boot = 1u; boot <= 2u; ++boot) {
        FILE *log = tmpfile();
        semu_logger logger;
        capture result = {0};
        semu_machine *machine;
        semu_stop_reason reason;
        uint64_t instructions, time_ns;
        uint32_t pc;

        SEMU_TEST_ASSERT(context, log != NULL);
        semu_log_init(&logger, log, SEMU_LOG_WARNING);
        options.logger = &logger;
        options.frame_context = &result;
        machine = semu_machine_create(&options, &error);
        if (machine == NULL) {
            fprintf(stderr, "Hello fixture rejected: %s\n", error.text);
            (void)fclose(log);
        }
        SEMU_TEST_ASSERT(context, machine != NULL);
        reason = semu_machine_run(machine, &limits, &error);
        instructions = semu_machine_instructions(machine);
        time_ns = semu_machine_virtual_time(machine);
        pc = semu_machine_program_counter(machine);
        semu_machine_destroy(machine);
        check_transcript(context, log);
        (void)fclose(log);

        SEMU_TEST_EQ_U64(context, SEMU_STOP_BUDGET, reason);
        SEMU_TEST_EQ_U64(context, 0u, result.invalid);
        SEMU_TEST_EQ_U64(context, 2u, result.first_match);
        SEMU_TEST_EQ_U64(context, 3u, result.matches);
        SEMU_TEST_EQ_U64(context, 185u, result.frames);
        SEMU_TEST_EQ_U64(context, 0x0009d150u, pc);
        SEMU_TEST_EQ_U64(context, UINT64_C(1730190171), instructions);
        SEMU_TEST_EQ_U64(context, UINT64_C(31000000000), time_ns);
        printf("boot=%u generation=%llu size=466x466 sha256=%s matches=%u frames=%u "
               "pc=%08x instructions=%llu time_ns=%llu\n",
               boot, (unsigned long long)result.first_match, hello_hash,
               result.matches, result.frames, (unsigned)pc,
               (unsigned long long)instructions, (unsigned long long)time_ns);
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_ulsan_244_hello_frame)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
