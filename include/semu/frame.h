#ifndef SEMU_FRAME_H
#define SEMU_FRAME_H

#include <stddef.h>
#include <stdint.h>

typedef enum semu_pixel_format {
    SEMU_PIXEL_RGB565_LE = 0
} semu_pixel_format;

typedef struct semu_frame {
    semu_pixel_format format;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint64_t generation;
    const uint8_t *pixels;
    size_t size;
} semu_frame;

typedef void (*semu_frame_callback)(void *context, const semu_frame *frame);

#endif
