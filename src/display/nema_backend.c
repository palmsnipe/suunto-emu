/*
 * Nema backend and panel frame publication (ticket 513).
 * Wires framing -> state -> texture/draw -> completion into the
 * frozen backend callback.  Renders to an internal 240x240 RGB565LE
 * surface; publishes renderer output to the frontend when a callback
 * is provided.  Physical panel publication remains refused until
 * E-NEMA-PANEL-001 is verified (MSPI/DIAP transport missing).
 */

#include "nema_backend.h"
#include "raster.h"
#include "sampling.h"
#include "blend.h"
#include "nema_texture.h"

#include <stdlib.h>
#include <string.h>

struct semu_nema_backend {
    semu_surface *surface;
    nema_state *state;
    nema_diagnostics *diag;
    uint8_t *backup;
    int draw_failed;
};

static void on_draw(void *context, const nema_draw_snapshot *snap)
{
    semu_nema_backend *backend = (semu_nema_backend *)context;
    raster_target target;
    raster_bounds clip;
    uint32_t stride;
    uint32_t dst_x, dst_y, w, h;

    if (backend->draw_failed) {
        return;
    }

    if (snap->target_format != NEMA_FMT_RGB565) {
        backend->draw_failed = 1;
        return;
    }

    target.pixels = semu_surface_pixels(backend->surface, &stride);
    if (target.pixels == NULL) {
        backend->draw_failed = 1;
        return;
    }
    target.width = NEMA_BACKEND_PANEL_WIDTH;
    target.height = NEMA_BACKEND_PANEL_HEIGHT;
    target.stride = stride;

    clip.min_x = snap->clip_min_x;
    clip.min_y = snap->clip_min_y;
    clip.max_x = snap->clip_max_x;
    clip.max_y = snap->clip_max_y;

    dst_x = snap->point0_x >> RASTER_FP_SHIFT;
    dst_y = snap->point0_y >> RASTER_FP_SHIFT;
    w = (snap->point2_x >> RASTER_FP_SHIFT);
    h = (snap->point2_y >> RASTER_FP_SHIFT);
    if (w > dst_x) {
        w -= dst_x;
    } else {
        w = 0u;
    }
    if (h > dst_y) {
        h -= dst_y;
    } else {
        h = 0u;
    }

    if (snap->src_present) {
        nema_texture_desc src;
        semu_error err;
        semu_status st;

        src.base = snap->src_base;
        src.format = snap->src_format;
        src.sampling = snap->src_sampling;
        src.stride = snap->src_stride;
        src.width = snap->src_width;
        src.height = snap->src_height;

        if (snap->src_format == NEMA_FMT_RGB565) {
            st = draw_texture(&target, &clip, NULL, &src, 0u, 0u,
                               dst_x, dst_y, w, h, NULL, &err);
        } else if (snap->src_format == NEMA_FMT_A2LE) {
            st = draw_mask(&target, &clip, NULL, &src, 0u, 0u,
                            dst_x, dst_y, w, h,
                            NEMA_BL_SIMPLE, snap->tex_color, NULL, &err);
        } else {
            backend->draw_failed = 1;
            return;
        }
        if (st != SEMU_OK) {
            backend->draw_failed = 1;
        }
    } else {
        uint16_t color = (uint16_t)(snap->draw_color & 0xFFFFu);
        semu_error err;
        if (raster_rect(&target, &clip, dst_x, dst_y, w, h,
                         color, NULL, &err) != SEMU_OK) {
            backend->draw_failed = 1;
        }
    }
}

semu_nema_backend *semu_nema_backend_create(semu_error *error)
{
    semu_nema_backend *backend;
    backend = (semu_nema_backend *)calloc(1u, sizeof(*backend));
    if (backend == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate backend");
        return NULL;
    }
    backend->surface = semu_surface_create(NEMA_BACKEND_PANEL_WIDTH,
                                            NEMA_BACKEND_PANEL_HEIGHT, error);
    if (backend->surface == NULL) {
        free(backend);
        return NULL;
    }
    if (nema_state_create(&backend->state, error) != SEMU_OK) {
        semu_surface_destroy(backend->surface);
        free(backend);
        return NULL;
    }
    if (nema_diagnostics_create(&backend->diag, error) != SEMU_OK) {
        nema_state_destroy(backend->state);
        semu_surface_destroy(backend->surface);
        free(backend);
        return NULL;
    }
    backend->backup = (uint8_t *)calloc(NEMA_BACKEND_PANEL_BYTES, 1u);
    if (backend->backup == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate backup");
        nema_diagnostics_destroy(backend->diag);
        nema_state_destroy(backend->state);
        semu_surface_destroy(backend->surface);
        free(backend);
        return NULL;
    }
    return backend;
}

void semu_nema_backend_destroy(semu_nema_backend *backend)
{
    if (backend == NULL) {
        return;
    }
    free(backend->backup);
    nema_diagnostics_destroy(backend->diag);
    nema_state_destroy(backend->state);
    semu_surface_destroy(backend->surface);
    free(backend);
}

void semu_nema_backend_reset(semu_nema_backend *backend)
{
    if (backend == NULL) {
        return;
    }
    nema_state_reset(backend->state);
    nema_diagnostics_reset(backend->diag);
    backend->draw_failed = 0;
    semu_surface_clear(backend->surface, 0u);
}

static semu_status read_word(semu_bus *bus, uint32_t addr, uint32_t *out,
                              semu_error *error)
{
    return semu_bus_read(bus, addr, 4u, out, error);
}

semu_transaction_result semu_nema_backend_submit(
    void *context, semu_bus *bus, uint32_t command_ring_address,
    uint32_t command_word_count, uint64_t virtual_time_ns,
    semu_frame_callback frame_callback, void *frame_context,
    semu_error *error)
{
    semu_nema_backend *backend = (semu_nema_backend *)context;
    uint32_t i;
    uint8_t *pixels;
    uint32_t stride;

    (void)virtual_time_ns;

    if (backend == NULL || bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "backend submit: null");
        return SEMU_TRANSACTION_REFUSE;
    }
    if (command_word_count == 0u) {
        return SEMU_TRANSACTION_OK;
    }
    if (command_word_count > NEMA_MAX_LIST_WORDS) {
        nema_diagnostics_record(backend->diag, NEMA_DIAG_BAD_SIZE, 0u,
                                 command_ring_address, 0u, command_word_count,
                                 NULL, 0u, error);
        return SEMU_TRANSACTION_REFUSE;
    }
    if ((command_ring_address & 3u) != 0u) {
        nema_diagnostics_record(backend->diag, NEMA_DIAG_BAD_ALIGNMENT, 0u,
                                 command_ring_address, 0u,
                                 command_ring_address, NULL, 0u, error);
        return SEMU_TRANSACTION_REFUSE;
    }

    pixels = semu_surface_pixels(backend->surface, &stride);
    if (pixels == NULL) {
        semu_error_set(error, SEMU_ERR_STATE, "backend: no surface pixels");
        return SEMU_TRANSACTION_REFUSE;
    }
    memcpy(backend->backup, pixels, NEMA_BACKEND_PANEL_BYTES);

    backend->draw_failed = 0;
    nema_state_begin_list(backend->state, 0u);

    for (i = 0u; i + 1u < command_word_count; i += 2u) {
        uint32_t reg_word, val_word;
        nema_record rec;
        semu_status st;
        uint8_t prefix;

        st = read_word(bus, command_ring_address + i * 4u, &reg_word, error);
        if (st != SEMU_OK) {
            memcpy(pixels, backend->backup, NEMA_BACKEND_PANEL_BYTES);
            nema_diagnostics_record(backend->diag, NEMA_DIAG_FRAMING_ERROR,
                                     0u, command_ring_address + i * 4u,
                                     0u, 0u, NULL, 0u, error);
            return SEMU_TRANSACTION_REFUSE;
        }
        st = read_word(bus, command_ring_address + (i + 1u) * 4u,
                        &val_word, error);
        if (st != SEMU_OK) {
            memcpy(pixels, backend->backup, NEMA_BACKEND_PANEL_BYTES);
            nema_diagnostics_record(backend->diag, NEMA_DIAG_FRAMING_ERROR,
                                     0u, command_ring_address + (i + 1u) * 4u,
                                     0u, 0u, NULL, 0u, error);
            return SEMU_TRANSACTION_REFUSE;
        }

        prefix = (uint8_t)(reg_word >> 24u);
        if (prefix != 0x00u && prefix != 0xFFu) {
            memcpy(pixels, backend->backup, NEMA_BACKEND_PANEL_BYTES);
            nema_diagnostics_record(backend->diag, NEMA_DIAG_BAD_PREFIX, 0u,
                                     command_ring_address + i * 4u,
                                     reg_word, val_word, NULL, 0u, error);
            return SEMU_TRANSACTION_REFUSE;
        }
        if ((reg_word & 3u) != 0u) {
            memcpy(pixels, backend->backup, NEMA_BACKEND_PANEL_BYTES);
            nema_diagnostics_record(backend->diag, NEMA_DIAG_BAD_ALIGNMENT,
                                     0u, command_ring_address + i * 4u,
                                     reg_word, val_word, NULL, 0u, error);
            return SEMU_TRANSACTION_REFUSE;
        }

        rec.prefix = prefix;
        rec.reg_offset = reg_word & 0x00FFFFFFu;
        rec.value = val_word;
        rec.source_addr = command_ring_address + (i + 1u) * 4u;

        st = nema_state_record(backend->state, &rec, on_draw, backend, error);
        if (st != SEMU_OK || backend->draw_failed) {
            memcpy(pixels, backend->backup, NEMA_BACKEND_PANEL_BYTES);
            if (!backend->draw_failed) {
                nema_diagnostics_record(backend->diag,
                                         NEMA_DIAG_UNKNOWN_REGISTER, 0u,
                                         rec.source_addr, rec.reg_offset,
                                         rec.value, NULL, 0u, error);
            } else {
                nema_diagnostics_record(backend->diag,
                                         NEMA_DIAG_UNSUPPORTED_DRAW, 0u,
                                         rec.source_addr, rec.reg_offset,
                                         rec.value, NULL, 0u, error);
            }
            return SEMU_TRANSACTION_REFUSE;
        }
    }

    /*
     * Publish the renderer surface to the frontend when a callback is
     * provided.  This is renderer output, not a physical-panel frame
     * (E-NEMA-PANEL-001 remains missing for the MSPI/DIAP transport).
     */
    if (frame_callback != NULL) {
        semu_surface_publish(backend->surface);
        frame_callback(frame_context,
                        semu_surface_frame(backend->surface));
    }

    return SEMU_TRANSACTION_OK;
}

const semu_frame *semu_nema_backend_frame(semu_nema_backend *backend)
{
    return backend != NULL ? semu_surface_frame(backend->surface) : NULL;
}

const nema_diagnostics *semu_nema_backend_diagnostics(
    semu_nema_backend *backend)
{
    return backend != NULL ? backend->diag : NULL;
}
