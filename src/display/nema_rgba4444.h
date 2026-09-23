#ifndef SEMU_NEMA_RGBA4444_H
#define SEMU_NEMA_RGBA4444_H
#include "semu/bus.h"
#include "nema_state.h"
/* E-NEMA-RGBA4444-001 only. pixels owns 240*240 RGB565LE pixels.
 * Refusal leaves all bytes unchanged. Reads mapped memory, never MMIO. */
semu_status nema_rgba4444_draw(semu_bus *bus, const nema_draw_snapshot *s,
    uint8_t *pixels, uint32_t stride, semu_error *error);
#endif
