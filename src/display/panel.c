/*
 * Physical panel assembly (ticket 513).
 * Assembles 115200-byte RGB565LE physical panel frames from region
 * events.  Publication always refuses because E-NEMA-PANEL-001
 * is missing: renderer output is not physical-panel evidence.
 */

#include "panel.h"

#include <stdlib.h>
#include <string.h>

struct semu_panel {
    uint8_t *pixels;
    semu_frame frame;
    uint32_t rows_committed;
    int complete;
};

semu_panel *semu_panel_create(semu_error *error)
{
    semu_panel *panel;
    panel = (semu_panel *)calloc(1u, sizeof(*panel));
    if (panel == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate panel");
        return NULL;
    }
    panel->pixels = (uint8_t *)calloc(PANEL_BYTES, 1u);
    if (panel->pixels == NULL) {
        free(panel);
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate panel pixels");
        return NULL;
    }
    panel->frame.format = SEMU_PIXEL_RGB565_LE;
    panel->frame.width = PANEL_WIDTH;
    panel->frame.height = PANEL_HEIGHT;
    panel->frame.stride = PANEL_WIDTH * 2u;
    panel->frame.pixels = panel->pixels;
    panel->frame.size = PANEL_BYTES;
    panel->frame.generation = 0u;
    panel->rows_committed = 0u;
    panel->complete = 0;
    return panel;
}

void semu_panel_destroy(semu_panel *panel)
{
    if (panel != NULL) {
        free(panel->pixels);
        free(panel);
    }
}

void semu_panel_reset(semu_panel *panel)
{
    if (panel == NULL) {
        return;
    }
    memset(panel->pixels, 0, PANEL_BYTES);
    panel->frame.generation = 0u;
    panel->rows_committed = 0u;
    panel->complete = 0;
}

semu_status semu_panel_commit_region(semu_panel *panel,
    uint32_t x, uint32_t y, uint32_t width, uint32_t height,
    const uint8_t *rgb565_le, uint32_t stride,
    semu_error *error)
{
    uint32_t row;
    if (panel == NULL || rgb565_le == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "panel commit: null argument");
        return SEMU_ERR_ARGUMENT;
    }
    if (width == 0u || height == 0u || stride < width * 2u ||
        x > PANEL_WIDTH || y > PANEL_HEIGHT ||
        width > PANEL_WIDTH - x || height > PANEL_HEIGHT - y) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "panel commit: region out of bounds");
        return SEMU_ERR_RANGE;
    }
    for (row = 0u; row < height; ++row) {
        size_t dst = (size_t)(y + row) * panel->frame.stride + x * 2u;
        memcpy(panel->pixels + dst,
               rgb565_le + (size_t)row * stride, (size_t)width * 2u);
    }
    if (x == 0u && width == PANEL_WIDTH) {
        uint32_t new_rows = panel->rows_committed + height;
        if (new_rows >= PANEL_HEIGHT) {
            panel->rows_committed = PANEL_HEIGHT;
            panel->complete = 1;
        } else {
            panel->rows_committed = new_rows;
        }
    }
    return SEMU_OK;
}

int semu_panel_is_complete(const semu_panel *panel)
{
    return panel != NULL ? panel->complete : 0;
}

semu_status semu_panel_publish(semu_panel *panel,
    semu_frame_callback callback, void *frame_context,
    semu_error *error)
{
    (void)panel;
    (void)callback;
    (void)frame_context;
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "panel: E-NEMA-PANEL-001 evidence is missing; "
                   "physical publication refused");
    return SEMU_ERR_UNSUPPORTED;
}

const semu_frame *semu_panel_frame(const semu_panel *panel)
{
    return panel != NULL ? &panel->frame : NULL;
}
