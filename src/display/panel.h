#ifndef SEMU_DISPLAY_PANEL_H
#define SEMU_DISPLAY_PANEL_H

#include "semu/frame.h"
#include "semu/types.h"

/*
 * Physical panel assembly (ticket 513).
 * Assembles 115200-byte (240x240x2 RGB565LE) physical panel frames
 * from region events.  Publication requires E-NEMA-PANEL-001
 * (currently missing); publish() always refuses and returns
 * SEMU_ERR_UNSUPPORTED.
 */

#define PANEL_WIDTH  240u
#define PANEL_HEIGHT 240u
#define PANEL_BYTES  (PANEL_WIDTH * PANEL_HEIGHT * 2u)

typedef struct semu_panel semu_panel;

semu_panel *semu_panel_create(semu_error *error);
void semu_panel_destroy(semu_panel *panel);
void semu_panel_reset(semu_panel *panel);

/*
 * Commit a rectangular region of RGB565LE pixels to the panel buffer.
 * Validates bounds before mutation.  Sets complete flag when all
 * 240 rows have been committed.
 */
semu_status semu_panel_commit_region(semu_panel *panel,
    uint32_t x, uint32_t y, uint32_t width, uint32_t height,
    const uint8_t *rgb565_le, uint32_t stride,
    semu_error *error);

/* Whether the panel has a complete 115200-byte frame. */
int semu_panel_is_complete(const semu_panel *panel);

/*
 * Publish the physical panel frame via callback.  Returns
 * SEMU_ERR_UNSUPPORTED because E-NEMA-PANEL-001 is missing:
 * renderer output is not physical-panel evidence.  The callback
 * is never invoked.
 */
semu_status semu_panel_publish(semu_panel *panel,
    semu_frame_callback callback, void *frame_context,
    semu_error *error);

/* Borrow the panel frame (read-only). */
const semu_frame *semu_panel_frame(const semu_panel *panel);

#endif
