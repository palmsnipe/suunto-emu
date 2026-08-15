#include "cli.h"

#include <SDL3/SDL.h>

#include "sdl_present_core.c"
#include "sdl_present.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct sdl_frontend {
    sdl_presenter *presenter;
    int failed;
} sdl_frontend;

static void publish_frame(void *context, const semu_frame *frame)
{
    sdl_frontend *frontend = (sdl_frontend *)context;
    semu_error error;
    SDL_Event event;
    if (frontend->failed) {
        return;
    }
    semu_error_clear(&error);
    if (sdl_presenter_present(frontend->presenter, frame, &error) !=
        SEMU_OK) {
        fprintf(stderr, "SDL present: %s\n", error.text);
        frontend->failed = 1;
        return;
    }
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            frontend->failed = 1;
        }
    }
}

static uint32_t parse_scale(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc - 1; ++i) {
        if (strcmp(argv[i], "--scale") == 0) {
            char *end;
            unsigned long v = strtoul(argv[i + 1], &end, 0);
            if (end != argv[i + 1] && *end == '\0' && v >= 1u && v <= 8u) {
                return (uint32_t)v;
            }
        }
    }
    return 2u;
}

int main(int argc, char **argv)
{
    sdl_frontend frontend;
    semu_error error;
    int result;
    uint32_t scale;
    memset(&frontend, 0, sizeof(frontend));
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL initialization: %s\n", SDL_GetError());
        return 2;
    }
    scale = parse_scale(argc, argv);
    semu_error_clear(&error);
    frontend.presenter = sdl_presenter_create(scale, &error);
    if (frontend.presenter == NULL) {
        fprintf(stderr, "SDL presenter: %s\n", error.text);
        SDL_Quit();
        return 2;
    }
    result = semu_cli_main(argc, argv, publish_frame, &frontend);
    sdl_presenter_destroy(frontend.presenter);
    SDL_Quit();
    return result != 0 ? result : (frontend.failed ? 3 : 0);
}
