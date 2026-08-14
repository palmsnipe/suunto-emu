#ifndef SEMU_DISPLAY_H
#define SEMU_DISPLAY_H

#include "semu/bus.h"
#include "semu/frame.h"
#include "semu/types.h"

typedef struct semu_surface semu_surface;

typedef semu_transaction_result (*semu_display_backend_submit_fn)(
    void *context, semu_bus *bus, uint32_t command_ring_address,
    uint32_t command_word_count, uint64_t virtual_time_ns,
    semu_frame_callback frame_callback, void *frame_context,
    semu_error *error);

semu_surface *semu_surface_create(uint32_t width, uint32_t height,
                                  semu_error *error);
void semu_surface_destroy(semu_surface *surface);
void semu_surface_clear(semu_surface *surface, uint16_t rgb565);
semu_status semu_surface_write(semu_surface *surface, uint32_t x, uint32_t y,
                               uint32_t width, uint32_t height,
                               const uint8_t *rgb565_le, uint32_t stride,
                               semu_error *error);
const semu_frame *semu_surface_frame(semu_surface *surface);

#endif
