#include "semu/display.h"

#include <stdio.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

int main(void)
{
    semu_error error;
    semu_surface *surface;
    const semu_frame *frame;
    const uint8_t patch[] = { 0x34u, 0x12u, 0x78u, 0x56u };
    semu_error_clear(&error);
    surface = semu_surface_create(4u, 3u, &error);
    CHECK(surface != NULL);
    semu_surface_clear(surface, 0x001fu);
    frame = semu_surface_frame(surface);
    CHECK(frame->size == 24u);
    CHECK(frame->stride == 8u);
    CHECK(frame->generation == 1u);
    CHECK(semu_surface_write(surface, 1u, 1u, 2u, 1u, patch, 4u, &error) ==
          SEMU_OK);
    CHECK(frame->pixels[10] == 0x34u && frame->pixels[11] == 0x12u);
    CHECK(frame->generation == 2u);
    CHECK(semu_surface_write(surface, 3u, 2u, 2u, 1u, patch, 4u, &error) ==
          SEMU_ERR_RANGE);
    semu_surface_destroy(surface);
    return failures != 0 ? 1 : 0;
}
