#include "cli.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

typedef struct sdl_frontend {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    uint32_t width;
    uint32_t height;
    int failed;
} sdl_frontend;

static void destroy_frontend(sdl_frontend *frontend)
{
    SDL_DestroyTexture(frontend->texture);
    SDL_DestroyRenderer(frontend->renderer);
    SDL_DestroyWindow(frontend->window);
    SDL_Quit();
}

static int prepare_frontend(sdl_frontend *frontend, const semu_frame *frame)
{
    if (frontend->window != NULL && frontend->width == frame->width &&
        frontend->height == frame->height) {
        return 1;
    }
    SDL_DestroyTexture(frontend->texture);
    SDL_DestroyRenderer(frontend->renderer);
    SDL_DestroyWindow(frontend->window);
    frontend->texture = NULL;
    frontend->renderer = NULL;
    frontend->window = NULL;
    if (!SDL_CreateWindowAndRenderer("suunto-emu", (int)frame->width * 2,
                                     (int)frame->height * 2, 0,
                                     &frontend->window,
                                     &frontend->renderer)) {
        fprintf(stderr, "SDL window: %s\n", SDL_GetError());
        return 0;
    }
    frontend->texture = SDL_CreateTexture(frontend->renderer,
                                          SDL_PIXELFORMAT_RGB565,
                                          SDL_TEXTUREACCESS_STREAMING,
                                          (int)frame->width,
                                          (int)frame->height);
    if (frontend->texture == NULL) {
        fprintf(stderr, "SDL texture: %s\n", SDL_GetError());
        return 0;
    }
    SDL_SetTextureScaleMode(frontend->texture, SDL_SCALEMODE_NEAREST);
    frontend->width = frame->width;
    frontend->height = frame->height;
    return 1;
}

static void publish_frame(void *context, const semu_frame *frame)
{
    sdl_frontend *frontend = (sdl_frontend *)context;
    SDL_Event event;
    if (frontend->failed || frame->format != SEMU_PIXEL_RGB565_LE ||
        !prepare_frontend(frontend, frame)) {
        frontend->failed = 1;
        return;
    }
    if (!SDL_UpdateTexture(frontend->texture, NULL, frame->pixels,
                           (int)frame->stride) ||
        !SDL_RenderClear(frontend->renderer) ||
        !SDL_RenderTexture(frontend->renderer, frontend->texture, NULL, NULL) ||
        !SDL_RenderPresent(frontend->renderer)) {
        fprintf(stderr, "SDL render: %s\n", SDL_GetError());
        frontend->failed = 1;
        return;
    }
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            frontend->failed = 1;
        }
    }
}

int main(int argc, char **argv)
{
    sdl_frontend frontend;
    int result;
    memset(&frontend, 0, sizeof(frontend));
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL initialization: %s\n", SDL_GetError());
        return 2;
    }
    result = semu_cli_main(argc, argv, publish_frame, &frontend);
    destroy_frontend(&frontend);
    return result != 0 ? result : (frontend.failed ? 3 : 0);
}
