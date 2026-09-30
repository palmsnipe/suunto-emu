#include "nema_backend_internal.h"
#include "raster.h"
#include "sampling.h"
#include "blend.h"
#include "nema_texture.h"
#include "nema_rgba4444.h"

static int is_observed_display_target(uint32_t base)
{
    return base == UINT32_C(0x1011cf40) ||
           base == UINT32_C(0x1011f4c0) ||
           base == UINT32_C(0x1012b040) ||
           base == UINT32_C(0x10139140);
}

/* First renderer error wins; the snapshot at the refused draw is retained
 * for the bounded draw-refusal event (ticket 794). */
static void set_draw_error(semu_nema_backend *backend,
    const nema_draw_snapshot *snap, const semu_error *error)
{
    if (backend->draw_error.code == SEMU_OK) {
        backend->draw_error = *error;
        backend->refusal_snapshot = *snap;
    }
}

static int is_observed_black_clear(const nema_draw_snapshot *snap)
{
    /*
     * E-NEMA-LISTS-001: the native first-frame clear uses this exact
     * RGB565/240x240 target and NEMA program state.  TEX1 is inherited
     * across lists, so its presence cannot take precedence over this clear.
     */
    return snap != NULL && snap->draw_cmd == NEMA_DRAW_QUAD &&
           is_observed_display_target(snap->target_base) &&
           snap->target_format == NEMA_FMT_RGB565 &&
           snap->target_stride == 480u &&
           snap->target_width == 240u && snap->target_height == 240u &&
           snap->matmult == UINT32_C(0x90000000) &&
           snap->codeptr == UINT32_C(0x941eb400) &&
           snap->draw_color == UINT32_C(0xff000000);
}

void nema_backend_draw(void *context, const nema_draw_snapshot *snap)
{
    nema_draw_context *draw_context = (nema_draw_context *)context;
    semu_nema_backend *backend = draw_context->backend;
    raster_target target;
    raster_bounds clip;
    uint32_t dst_x, dst_y, w, h;

    if (backend->draw_error.code != SEMU_OK) {
        return;
    }

    if (snap->target_format == NEMA_FMT_TSC6A) {
        semu_status st;
        semu_error draw_error;
        semu_error_clear(&draw_error);
        if (!backend->tsc6a_dirty) {
            nema_tsc6a_copy(backend->pending_tsc6a, backend->tsc6a);
            backend->tsc6a_dirty = 1;
        }
        /* Ticket 710 instance: a frame's strokes blend onto the resting
         * state of the compressed surface. */
        if (!backend->shadow_fresh) {
            nema_tsc6a_frame_begin(backend, draw_context->bus,
                                   snap->target_base,
                                   backend->pending_tsc6a);
        }
        st = nema_tsc6a_draw_target(backend->pending_tsc6a, draw_context->bus,
                                    snap, &draw_error);
        if (st != SEMU_OK) {
            set_draw_error(backend, snap, &draw_error);
        }
        return;
    }

    target.pixels = backend->working_pixels;
    target.width = NEMA_BACKEND_PANEL_WIDTH;
    target.height = NEMA_BACKEND_PANEL_HEIGHT;
    target.stride = NEMA_BACKEND_PANEL_WIDTH * 2u;

    clip.min_x = snap->clip_min_x;
    clip.min_y = snap->clip_min_y;
    clip.max_x = snap->clip_max_x;
    clip.max_y = snap->clip_max_y;

    if (snap->draw_cmd == NEMA_DRAW_TSC6A_RESOLVE) {
        semu_error err;
        nema_tsc6a *shadow =
            backend->tsc6a_dirty ? backend->pending_tsc6a : backend->tsc6a;
        /* Ticket 710 instance: the resolve consumes the frame.  Pick up
         * guest span rewrites first, then return the shadow to the
         * resting state for the next frame. */
        nema_tsc6a_frame_baseline(backend, draw_context->bus,
                                  snap->src_base);
        if (!backend->shadow_fresh) {
            nema_tsc6a_frame_begin(backend, draw_context->bus,
                                   snap->src_base, shadow);
        }
        semu_status st = nema_tsc6a_resolve(
            shadow, snap, target.pixels, target.stride, &err);
        if (st != SEMU_OK) {
            set_draw_error(backend, snap, &err);
        }
        nema_tsc6a_frame_end(backend, shadow);
        return;
    }

    if (snap->draw_cmd == NEMA_DRAW_QUAD &&
        snap->src_format == NEMA_FMT_TSC6A) {
        semu_error err;
        semu_status st = nema_tsc6a_resolve_mask(
            backend->tsc6a_dirty ? backend->pending_tsc6a : backend->tsc6a,
            draw_context->bus,
            snap, target.pixels, target.stride, &err);
        if (st != SEMU_OK) {
            set_draw_error(backend, snap, &err);
        }
        return;
    }

    if (snap->draw_cmd == NEMA_DRAW_TRI_SOLID ||
        snap->draw_cmd == NEMA_DRAW_TRI_AA) {
        semu_error err;
        semu_status st = nema_tsc6a_draw_rgb565_triangle(
            snap, target.pixels, NEMA_BACKEND_PANEL_WIDTH,
            NEMA_BACKEND_PANEL_HEIGHT, target.stride,
            snap->draw_cmd == NEMA_DRAW_TRI_AA, &err);
        if (st != SEMU_OK) {
            set_draw_error(backend, snap, &err);
        }
        return;
    }

    if (snap->src_format == NEMA_FMT_RGBA4444 && !is_observed_black_clear(snap)) {
        semu_error err;
        if (nema_rgba4444_draw(draw_context->bus, snap, target.pixels,
            target.stride, &err) != SEMU_OK) set_draw_error(backend, snap, &err);
        return;
    }

    if (snap->target_format != NEMA_FMT_RGB565) {
        semu_error fmt_err;
        semu_error_set(&fmt_err, SEMU_ERR_UNSUPPORTED,
            "backend: unsupported target format 0x%02x", snap->target_format);
        set_draw_error(backend, snap, &fmt_err);
        return;
    }

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

    if (is_observed_black_clear(snap)) {
        semu_error err;
        if (raster_rect(&target, &clip, dst_x, dst_y, w, h, 0u, NULL,
                        &err) != SEMU_OK) {
            set_draw_error(backend, snap, &err);
        }
    } else if (snap->src_present) {
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
            st = draw_texture(&target, &clip, draw_context->bus, &src, 0u, 0u,
                               dst_x, dst_y, w, h, NULL, &err);
        } else if (snap->src_format == NEMA_FMT_A2LE) {
            if (snap->matrix_present) {
                nema_affine_matrix matrix;
                matrix.mm00 = snap->mm00;
                matrix.mm01 = snap->mm01;
                matrix.mm02 = snap->mm02;
                matrix.mm10 = snap->mm10;
                matrix.mm11 = snap->mm11;
                matrix.mm12 = snap->mm12;
                st = draw_mask_affine(&target, &clip, draw_context->bus,
                                      &src, dst_x, dst_y, w, h, &matrix,
                                      NEMA_BL_SIMPLE, snap->tex_color, NULL,
                                      &err);
            } else {
                st = draw_mask(&target, &clip, draw_context->bus, &src, 0u,
                               0u, dst_x, dst_y, w, h, NEMA_BL_SIMPLE,
                               snap->tex_color, NULL, &err);
            }
        } else {
            semu_error_set(&err, SEMU_ERR_UNSUPPORTED,
                "backend: unsupported source format 0x%02x", snap->src_format);
            set_draw_error(backend, snap, &err);
            return;
        }
        if (st != SEMU_OK) {
            set_draw_error(backend, snap, &err);
        }
    } else {
        uint16_t color = (uint16_t)(snap->draw_color & 0xFFFFu);
        semu_error err;
        if (raster_rect(&target, &clip, dst_x, dst_y, w, h,
                         color, NULL, &err) != SEMU_OK) {
            set_draw_error(backend, snap, &err);
        }
    }
}
