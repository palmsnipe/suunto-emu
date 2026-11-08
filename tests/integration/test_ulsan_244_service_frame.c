#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semu/hash.h"
#include "semu/machine.h"
#include "test.h"

#define PANEL_SIZE 466u
#define EXPECTED_FRAMES 3u

typedef struct {
    char hash[65];
    unsigned nonzero;
    unsigned left, top, right, bottom;
} frame_record;

typedef struct {
    frame_record frames[EXPECTED_FRAMES];
    unsigned count;
    int invalid;
} frame_capture;

static void capture_frame(void *context, const semu_frame *frame)
{
    frame_capture *capture = context;
    unsigned ordinal = ++capture->count;
    frame_record *record;
    uint8_t digest[SEMU_SHA256_SIZE];
    unsigned x, y;

    if (ordinal > EXPECTED_FRAMES || frame->format != SEMU_PIXEL_RGB565_LE ||
        frame->width != PANEL_SIZE || frame->height != PANEL_SIZE ||
        frame->stride != PANEL_SIZE * 2u || frame->pixels == NULL ||
        frame->size != PANEL_SIZE * PANEL_SIZE * 2u ||
        frame->generation != ordinal) {
        capture->invalid = 1;
        return;
    }
    record = &capture->frames[ordinal - 1u];
    record->left = PANEL_SIZE;
    record->top = PANEL_SIZE;
    semu_sha256(frame->pixels, frame->size, digest);
    semu_sha256_format(digest, record->hash);
    for (y = 0u; y < PANEL_SIZE; ++y) {
        for (x = 0u; x < PANEL_SIZE; ++x) {
            size_t pixel = (size_t)y * frame->stride + x * 2u;
            if ((frame->pixels[pixel] | frame->pixels[pixel + 1u]) == 0u)
                continue;
            ++record->nonzero;
            if (x < record->left) record->left = x;
            if (x > record->right) record->right = x;
            if (y < record->top) record->top = y;
            record->bottom = y;
        }
    }
}

static void test_ulsan_244_service_frame(semu_test_context *context)
{
    static const char *const hashes[] = {
        "6cc911ec30cb65f8bd344e37320439eed8e62fcb79c6010ec125dd4ee959ee2a",
        "02e0e0115b1ea61c60a48ed1a57b5c2db8ee425bb5ac10b7e08181579f2d2b1c",
        "bc071732e63081b938b4d98e74cdb693a5781365e92abc14bcff99fa924ce526"
    };
    const char *manifest = getenv("SEMU_ULSAN_244_FIRMWARE_MANIFEST");
    const semu_run_limits limits = {160000000u, UINT64_C(8000000000)};
    semu_profile profile;
    semu_firmware_manifest firmware;
    semu_machine_options options = {0};
    semu_error error;
    unsigned pass, index;

    if (manifest == NULL || manifest[0] == '\0') {
        puts("SKIP Race S Service Mode capture: set SEMU_ULSAN_244_FIRMWARE_MANIFEST");
        return;
    }
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_profile_load(
        "profiles/ulsan/2.44.52/profile.semu", &profile, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_manifest_load(manifest, &firmware, &error));
    options.profile = &profile;
    options.firmware = &firmware;
    options.frame_callback = capture_frame;

    for (pass = 0u; pass < 2u; ++pass) {
        frame_capture capture = {0};
        semu_machine *machine;
        semu_stop_reason reason;
        uint64_t instructions, time_ns;
        uint32_t pc;

        options.frame_context = &capture;
        machine = semu_machine_create(&options, &error);
        SEMU_TEST_ASSERT(context, machine != NULL);
        reason = semu_machine_run(machine, &limits, &error);
        instructions = semu_machine_instructions(machine);
        time_ns = semu_machine_virtual_time(machine);
        pc = semu_machine_program_counter(machine);
        semu_machine_destroy(machine);

        SEMU_TEST_EQ_U64(context, SEMU_STOP_BUDGET, reason);
        SEMU_TEST_EQ_U64(context, EXPECTED_FRAMES, capture.count);
        SEMU_TEST_EQ_U64(context, 0u, capture.invalid);
        SEMU_TEST_EQ_U64(context, 98756036u, instructions);
        SEMU_TEST_EQ_U64(context, UINT64_C(8014847988), time_ns);
        SEMU_TEST_EQ_U64(context, 0x000d47f0u, pc);
        for (index = 0u; index < EXPECTED_FRAMES; ++index)
            SEMU_TEST_ASSERT(context,
                strcmp(capture.frames[index].hash, hashes[index]) == 0);
        SEMU_TEST_EQ_U64(context, 3150u, capture.frames[2].nonzero);
        SEMU_TEST_EQ_U64(context, 90u, capture.frames[2].left);
        SEMU_TEST_EQ_U64(context, 373u, capture.frames[2].right);
        SEMU_TEST_EQ_U64(context, 6u, capture.frames[2].top);
        SEMU_TEST_EQ_U64(context, 32u, capture.frames[2].bottom);
        for (index = 0u; index < EXPECTED_FRAMES; ++index)
            printf("boot=%u generation=%u size=466x466 nonzero=%u sha256=%s\n",
                   pass + 1u, index + 1u, capture.frames[index].nonzero,
                   capture.frames[index].hash);
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_ulsan_244_service_frame)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
