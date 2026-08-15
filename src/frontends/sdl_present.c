/*
 * SDL3 frame presenter (ticket 515).
 * Wraps sdl_present_core validation with SDL3 window/renderer/texture
 * lifecycle.  Recreates texture only on dimension/scale change.
 */

#include "sdl_present.h"

#include <stdlib.h>

struct sdl_presenter {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    uint32_t width;
    uint32_t height;
    uint32_t scale;
    int failed;
};

sdl_presenter *sdl_presenter_create(uint32_t scale, semu_error *error)
{
    sdl_presenter *p;
    if (scale < SDL_PRESENT_MIN_SCALE ||
        scale > SDL_PRESENT_MAX_SCALE) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "sdl_presenter: scale %u out of range", scale);
        return NULL;
    }
    p = (sdl_presenter *)calloc(1u, sizeof(*p));
    if (p == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "sdl_presenter: cannot allocate");
        return NULL;
    }
    p->scale = scale;
    return p;
}

void sdl_presenter_destroy(sdl_presenter *p)
{
    if (p == NULL) {
        return;
    }
    if (p->texture != NULL) {
        SDL_DestroyTexture(p->texture);
    }
    if (p->renderer != NULL) {
        SDL_DestroyRenderer(p->renderer);
    }
    if (p->window != NULL) {
        SDL_DestroyWindow(p->window);
    }
    free(p);
}

static int ensure_resources(sdl_presenter *p, uint32_t w, uint32_t h,
                             semu_error *error)
{
    if (p->window != NULL && p->width == w && p->height == h) {
        return 1;
    }
    if (p->texture != NULL) {
        SDL_DestroyTexture(p->texture);
        p->texture = NULL;
    }
    if (p->renderer != NULL) {
        SDL_DestroyRenderer(p->renderer);
        p->renderer = NULL;
    }
    if (p->window != NULL) {
        SDL_DestroyWindow(p->window);
        p->window = NULL;
    }
    if (!SDL_CreateWindowAndRenderer("suunto-emu",
        (int)w * (int)p->scale, (int)h * (int)p->scale, 0,
        &p->window, &p->renderer)) {
        semu_error_set(error, SEMU_ERR_IO,
                       "sdl_presenter: window: %s", SDL_GetError());
        p->failed = 1;
        return 0;
    }
    p->texture = SDL_CreateTexture(p->renderer,
        SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
        (int)w, (int)h);
    if (p->texture == NULL) {
        semu_error_set(error, SEMU_ERR_IO,
                       "sdl_presenter: texture: %s", SDL_GetError());
        p->failed = 1;
        return 0;
    }
    SDL_SetTextureScaleMode(p->texture, SDL_SCALEMODE_NEAREST);
    p->width = w;
    p->height = h;
    return 1;
}

semu_status sdl_presenter_present(sdl_presenter *p,
    const semu_frame *frame, semu_error *error)
{
    sdl_present_descriptor desc;
    semu_status st;
    if (p == NULL || frame == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "sdl_presenter: null argument");
        return SEMU_ERR_ARGUMENT;
    }
    if (p->failed) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "sdl_presenter: presenter in failed state");
        return SEMU_ERR_STATE;
    }
    st = sdl_present_core_validate(frame->format,
        frame->width, frame->height,
        frame->stride, frame->size,
        p->scale, &desc, error);
    if (st != SEMU_OK) {
        return st;
    }
    if (!ensure_resources(p, desc.width, desc.height, error)) {
        return SEMU_ERR_IO;
    }
    if (!SDL_UpdateTexture(p->texture, NULL,
            frame->pixels, (int)frame->stride) ||
        !SDL_RenderClear(p->renderer) ||
        !SDL_RenderTexture(p->renderer, p->texture, NULL, NULL) ||
        !SDL_RenderPresent(p->renderer)) {
        semu_error_set(error, SEMU_ERR_IO,
                       "sdl_presenter: render: %s", SDL_GetError());
        p->failed = 1;
        return SEMU_ERR_IO;
    }
    return SEMU_OK;
}
