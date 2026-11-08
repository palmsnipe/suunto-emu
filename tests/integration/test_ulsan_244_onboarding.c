#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semu/hash.h"
#include "semu/machine.h"
#include "test.h"

#define PANEL_SIZE 466u
#define RUN_NS UINT64_C(65000000000)
#define RUN_INSNS UINT64_C(8000000000)

typedef struct {
    const char *name, *sha;
    unsigned budget_ms;
} checkpoint;

typedef struct {
    const char *expected;
    char sha[65];
    uint64_t frames;
    uint64_t matched;
    uint64_t height;
    int invalid;
} capture;

static const char height_sha[] =
    "0524d291bd3bd1719f38beadf31c6aff655ea74d8579b26477cc6791d3e527c3";

static void capture_frame(void *context, const semu_frame *frame)
{
    capture *result = context;
    uint8_t digest[SEMU_SHA256_SIZE];

    ++result->frames;
    if (frame->format != SEMU_PIXEL_RGB565_LE || frame->width != PANEL_SIZE ||
        frame->height != PANEL_SIZE || frame->stride != PANEL_SIZE * 2u ||
        frame->size != PANEL_SIZE * PANEL_SIZE * 2u || frame->pixels == NULL ||
        frame->generation != result->frames) {
        result->invalid = 1;
        return;
    }
    semu_sha256(frame->pixels, frame->size, digest);
    semu_sha256_format(digest, result->sha);
    if (result->expected != NULL && result->matched == 0u &&
        strcmp(result->sha, result->expected) == 0)
        result->matched = frame->generation;
    if (strcmp(result->sha, height_sha) == 0)
        result->height = frame->generation;
}

static semu_stop_reason poll_frame(void *context, semu_machine *machine, semu_error *error)
{
    const capture *result = context;
    (void)machine;
    (void)error;
    return result->matched != 0u ? SEMU_STOP_USER : SEMU_STOP_NONE;
}

static int run(semu_machine *machine, capture *result, unsigned ms, const checkpoint *stage)
{
    const uint64_t now = semu_machine_virtual_time(machine);
    const uint64_t instructions = semu_machine_instructions(machine);
    semu_run_limits limit;
    semu_error error = {0};
    semu_stop_reason reason;

    if (now >= RUN_NS || instructions >= RUN_INSNS) return 0;
    limit.max_instructions = RUN_INSNS - instructions;
    limit.max_virtual_time_ns = (uint64_t)ms * UINT64_C(1000000);
    if (limit.max_virtual_time_ns > RUN_NS - now)
        limit.max_virtual_time_ns = RUN_NS - now;
    result->expected = stage == NULL ? NULL : stage->sha;
    result->matched = 0u;
    reason = semu_machine_run(machine, &limit, &error);
    if (result->invalid || reason != (stage == NULL ? SEMU_STOP_BUDGET : SEMU_STOP_USER)) {
        fprintf(stderr, "%s stopped at pc=%08x: %s frame=%s (%s)\n",
                stage == NULL ? "input delivery" : stage->name,
                (unsigned)semu_machine_program_counter(machine), semu_stop_reason_name(reason),
                result->sha, error.text);
        return 0;
    }
    if (stage != NULL)
        printf("stage=%s generation=%llu time_ns=%llu sha256=%s\n",
               stage->name, (unsigned long long)result->matched,
               (unsigned long long)semu_machine_virtual_time(machine), stage->sha);
    return 1;
}

static int send(semu_machine *machine, semu_input_event event, semu_status expected)
{
    semu_error error = {0};
    const semu_status status = semu_machine_input(machine, &event, &error);
    if (status == expected) return 1;
    fprintf(stderr, "input %u (x=%d y=%d value=%d) returned %u instead of %u: %s\n",
            (unsigned)event.kind, (int)event.x, (int)event.y, (int)event.value,
            (unsigned)status, (unsigned)expected, error.text);
    return 0;
}

static int click(semu_machine *machine, capture *result, unsigned settle, unsigned hold)
{
    semu_input_event middle = {.kind = SEMU_INPUT_BUTTON, .code = SEMU_BUTTON_MIDDLE, .value = 0};
    if ((settle != 0u && !run(machine, result, settle, NULL)) ||
        !send(machine, middle, SEMU_OK) ||
        !run(machine, result, hold, NULL))
        return 0;
    middle.value = 1;
    return send(machine, middle, SEMU_OK);
}

static int rotate(semu_machine *machine, capture *result, unsigned count)
{
    const semu_input_event detent = {.kind = SEMU_INPUT_CROWN_ROTATE, .value = 24};
    unsigned i;

    for (i = 0u; i < count; ++i) {
        if (!send(machine, detent, SEMU_OK) ||
            !run(machine, result, 125u, NULL))
            return 0;
    }
    return 1;
}

static int at_checkpoint(semu_machine *machine, const capture *result,
                         const char *name, const char *sha)
{
    if (strcmp(result->sha, sha) != 0) {
        fprintf(stderr, "missing %s: last frame=%s generation=%llu\n", name,
                result->sha, (unsigned long long)result->frames);
        return 0;
    }
    printf("stage=%s generation=%llu time_ns=%llu sha256=%s\n", name,
           (unsigned long long)result->frames,
           (unsigned long long)semu_machine_virtual_time(machine), sha);
    return 1;
}

static int swipe_up(semu_machine *machine, capture *result)
{
    const int points[] = {330, 195, 100};
    unsigned i;

    for (i = 0u; i < SEMU_ARRAY_LEN(points); ++i) {
        const semu_input_event touch = {
            .kind = SEMU_INPUT_TOUCH, .x = 230, .y = points[i], .value = 100
        };
        if (!send(machine, touch, SEMU_OK) || !run(machine, result, 80u, NULL))
            return 0;
    }
    return send(machine, (semu_input_event){
        .kind = SEMU_INPUT_TOUCH, .x = 230, .y = 100
    }, SEMU_OK);
}

static int walk(semu_machine *machine, capture *result, int crown_only)
{
    /* E-ULS-0098 and E-ULS-0100: native frames, with standalone pins for
     * numeric selectors where the reference layout differs. */
    static const checkpoint setup[] = {
        {"hello", "9d02076e5fe66dad7ceccf7b4b86c1c59ab5e5f2948906868ba87a1df7ff9c76", 35000u},
        {"language-english", "c36ca7042f5a4b67c67eac41c400054749a53859d33298b0e517effdaedc9972", 5000u},
        {"define-profile", "233aa486b330e5531a32ad752e4a08a4e04d4355d0efa60b83e0c7b79ac32d38", 8000u},
        {"profile-purpose", "e0db0fa1a3ffc80b75fd4541f64f3d5281839c3777ea5e27a98808c9f4e42c58", 6000u},
        {"gender-female", "a3ea323f3daaf424e1a3ab2f72261d150437590226134c4aa377b0ef4e148d5a", 6000u},
    };
    static const checkpoint time_format = {
        "time-format-24h", "adf7afd6580f36e617a6f6851c8067ba50daf1db19e4d21591e6899b95a08992", 6000u
    };
    static const char birth[] = "05e3252405bb9124443df6f340f3d112eec6a010dcc1d2bd641820a4e6aa6b21";
    static const char units[] = "1fb75497658bbc503b943a21460e74f60c81326da3c9eb32f952edadbe214c1a";
    static const char weight[] = "8e1622377668c5bd8ccf7f3a6372aed929a3a659a6e3ddb28348f06739504547";
    static const char pairing[] = "4142652463adabae9d874faa34f57a61f010249e013f04e376058cfc19e85ec4";
    static const char guidance[] = "7a29d4351f23e6e0cc7be0985b224a5da0e700f16946091273005356c5a9efdd";
    static const char scrolled[] = "075d2337ca69338649a146f76d81f8d5bbd0d0d6f07b4cbcf251ffcde8527e59";
    static const char bottom[] = "d3014e708de945e59f5a41e226fd89156bb9ba9467c676ed4eef634229346ae4";
    const semu_input_event detent = {.kind = SEMU_INPUT_CROWN_ROTATE, .value = 24};
    const semu_input_event contact = {.kind = SEMU_INPUT_TOUCH, .x = 230, .y = 100, .value = 100};
    unsigned i;

    if (!send(machine, detent, SEMU_ERR_CONFLICT) || !send(machine, contact, SEMU_ERR_CONFLICT))
        return 0;
    for (i = 0u; i < SEMU_ARRAY_LEN(setup); ++i) {
        if (i != 0u && !click(machine, result, i == 1u ? 250u : 800u, 250u))
            return 0;
        if (!run(machine, result, setup[i].budget_ms, &setup[i]))
            return 0;
    }
    /* With native actuator completion, shorter presses select Gender and Birth Year. */
    if (!click(machine, result, 800u, 150u) || !run(machine, result, 2000u, NULL) ||
        !at_checkpoint(machine, result, "birth-year-1990", birth) ||
        !click(machine, result, 0u, 150u) || !run(machine, result, 2000u, NULL) ||
        !at_checkpoint(machine, result, "unit-metric", units) ||
        !click(machine, result, 800u, 250u) ||
        !run(machine, result, time_format.budget_ms, &time_format) ||
        !click(machine, result, 800u, 250u) || !run(machine, result, 1300u, NULL) ||
        !at_checkpoint(machine, result, "weight-70kg", weight))
        return 0;
    /* The height frame can commit during the next view's transition. */
    if (!click(machine, result, 800u, 250u) || !run(machine, result, 2300u, NULL) ||
        !click(machine, result, 800u, 250u) || !run(machine, result, 1200u, NULL))
        return 0;
    if (result->height == 0u || result->height >= result->frames) {
        fprintf(stderr, "missing height before pairing: last frame=%s generation=%llu\n",
                result->sha, (unsigned long long)result->frames);
        return 0;
    }
    printf("stage=height-170cm generation=%llu sha256=%s\n",
           (unsigned long long)result->height, height_sha);
    if (!at_checkpoint(machine, result, "pairing-connect", pairing) ||
        !run(machine, result, 800u, NULL) || !rotate(machine, result, 8u) ||
        !click(machine, result, 0u, 250u) || !run(machine, result, 1200u, NULL) ||
        !at_checkpoint(machine, result, "pairing-guidance", guidance))
        return 0;
    if (crown_only) {
        /* E-ULS-0099: keep the idle interval that exposed the stalled list
         * before asking the guest to consume another crown burst. */
        return rotate(machine, result, 6u) && run(machine, result, 650u, NULL) &&
               at_checkpoint(machine, result, "crown-scroll", scrolled) &&
               run(machine, result, 1650u, NULL) &&
               at_checkpoint(machine, result, "crown-scroll-after-idle", scrolled) &&
               rotate(machine, result, 6u) &&
               run(machine, result, 650u, NULL) &&
               at_checkpoint(machine, result, "crown-scroll-bottom", bottom);
    }
    return run(machine, result, 450u, NULL) && swipe_up(machine, result) &&
           run(machine, result, 600u, NULL) &&
           at_checkpoint(machine, result, "touch-scroll", scrolled) &&
           run(machine, result, 800u, NULL) && rotate(machine, result, 6u) &&
           run(machine, result, 650u, NULL) &&
           at_checkpoint(machine, result, "crown-scroll-bottom", bottom);
}

static int check_log(FILE *log)
{
    char line[512];
    rewind(log);
    while (fgets(line, sizeof(line), log) != NULL) {
        if (strstr(line, "machine-reset-request") != NULL || strstr(line, "draw-refused") != NULL) {
            fputs(line, stderr);
            return 0;
        }
    }
    return !ferror(log);
}

static void verify_onboarding(semu_test_context *context, int crown_only)
{
    static const char *const layers[] = {
        "ulsan-2.44-production-data", "ulsan-2.44-emmc", "ulsan-2.44-gps-startup"
    };
    const char *manifest = getenv("SEMU_ULSAN_244_FIRMWARE_MANIFEST");
    const char *image = getenv("SEMU_ULSAN_244_EMMC_IMAGE");
    semu_profile profile;
    semu_firmware_manifest firmware;
    semu_machine_options options = {0};
    semu_error error;
    uint64_t first_frames = 0u, first_time = 0u, first_instructions = 0u;
    unsigned boot;

    if ((manifest == NULL || manifest[0] == '\0') &&
        (image == NULL || image[0] == '\0')) {
        puts("SKIP Race S onboarding: set SEMU_ULSAN_244_FIRMWARE_MANIFEST and SEMU_ULSAN_244_EMMC_IMAGE");
        return;
    }
    SEMU_TEST_ASSERT(context, manifest != NULL && manifest[0] != '\0');
    SEMU_TEST_ASSERT(context, image != NULL && image[0] != '\0');
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_profile_load(
        "profiles/ulsan/2.44.52/profile.semu", &profile, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_manifest_load(manifest, &firmware, &error));
    options.profile = &profile;
    options.firmware = &firmware;
    options.layers = layers;
    options.layer_count = SEMU_ARRAY_LEN(layers);
    options.ulsan_emmc_path = image;
    options.frame_callback = capture_frame;
    options.input_poll = poll_frame;

    for (boot = 1u; boot <= 2u; ++boot) {
        FILE *log = tmpfile();
        semu_logger logger;
        semu_machine *machine;
        capture result = {0};
        uint64_t time_ns, instructions;
        int passed = 0;

        SEMU_TEST_ASSERT(context, log != NULL);
        semu_log_init(&logger, log, SEMU_LOG_WARNING);
        options.logger = &logger;
        options.frame_context = &result;
        options.input_poll_context = &result;
        machine = semu_machine_create(&options, &error);
        if (machine == NULL) {
            fprintf(stderr, "Race S onboarding fixture rejected: %s\n", error.text);
            (void)fclose(log);
        }
        SEMU_TEST_ASSERT(context, machine != NULL);

        printf("cold boot %u\n", boot);
        passed = walk(machine, &result, crown_only);
        time_ns = semu_machine_virtual_time(machine);
        instructions = semu_machine_instructions(machine);
        semu_machine_destroy(machine);
        passed = check_log(log) && passed;
        (void)fclose(log);
        SEMU_TEST_ASSERT(context, passed);
        SEMU_TEST_ASSERT(context, time_ns < RUN_NS && instructions < RUN_INSNS);
        printf("frames=%llu instructions=%llu time_ns=%llu\n",
               (unsigned long long)result.frames, (unsigned long long)instructions,
               (unsigned long long)time_ns);
        if (boot == 1u) {
            first_frames = result.frames;
            first_time = time_ns;
            first_instructions = instructions;
        } else {
            SEMU_TEST_EQ_U64(context, first_frames, result.frames);
            SEMU_TEST_EQ_U64(context, first_time, time_ns);
            SEMU_TEST_EQ_U64(context, first_instructions, instructions);
        }
    }
}

static void test_ulsan_244_crown_only(semu_test_context *context)
{
    verify_onboarding(context, 1);
}

static void test_ulsan_244_onboarding(semu_test_context *context)
{
    verify_onboarding(context, 0);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_ulsan_244_crown_only),
        SEMU_TEST_CASE(test_ulsan_244_onboarding)
    };
    (void)setvbuf(stdout, NULL, _IOLBF, 0u);
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
