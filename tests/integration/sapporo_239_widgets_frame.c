/* Optional authentic runner helper; never contains firmware or frame pixels. */
#include "frontends/cli.h"
#include "semu/hash.h"
#include <stdio.h>
#include <string.h>

typedef struct frame_check {
    unsigned count;
    int failed;
} frame_check;

static void check_frame(void *context, const semu_frame *frame)
{
    frame_check *check = (frame_check *)context;
    uint8_t digest[SEMU_SHA256_SIZE];
    char hash[65];
    ++check->count;
    if (frame == NULL || frame->pixels == NULL ||
        frame->format != SEMU_PIXEL_RGB565_LE || frame->width != 240u ||
        frame->height != 240u || frame->stride != 480u ||
        frame->size != 115200u || frame->generation != 2u) {
        check->failed = 1;
        return;
    }
    semu_sha256(frame->pixels, frame->size, digest);
    semu_sha256_format(digest, hash);
    if (strcmp(hash,
        "3eff811736aa1890e78095f31d88ad95a8a457d41caa0ccb3e527555c8ecf373") ||
        semu_crc32(0u, frame->pixels, frame->size) != UINT32_C(0x4979f432))
        check->failed = 1;
    fprintf(stderr, "widgets-frame width=240 height=240 generation=2 "
                    "sha256=%s\n", hash);
}

int main(int argc, char **argv)
{
    frame_check check = {0u, 0};
    int result = semu_cli_main(argc, argv, check_frame, &check, NULL, NULL);
    if (result != 0 || check.failed || check.count != 1u) {
        fprintf(stderr, "error: exact Widgets startup frame was not observed\n");
        return 1;
    }
    return 0;
}
