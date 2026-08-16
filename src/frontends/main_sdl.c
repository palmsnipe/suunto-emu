#include "cli.h"

#include <SDL3/SDL.h>

#include "sdl_present_core.c"
#include "sdl_present.c"
#include "sdl_input.c"
#include "semantic_input.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct sdl_frontend {
    sdl_presenter *presenter;
    semu_sdl_input_adapter *input_adapter;
    semu_input_mapper *input_mapper;
    int failed;
} sdl_frontend;

static void publish_frame(void *context, const semu_frame *frame)
{
    sdl_frontend *frontend = (sdl_frontend *)context;
    semu_error error;
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
}

static semu_stop_reason poll_input(void *context, semu_machine *machine,
                                    semu_error *error)
{
    sdl_frontend *frontend = (sdl_frontend *)context;
    SDL_Event event;

    if (frontend == NULL || machine == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "SDL input poll has no runtime context");
        return SEMU_STOP_DEVICE_REFUSED;
    }
    while (SDL_PollEvent(&event)) {
        semu_normalized_key key;
        semu_input_event input;
        semu_input_event releases[3];
        uint32_t release_count;
        uint32_t release_index;
        int quit = 0;
        int has_key;
        semu_error_clear(error);
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            if (semu_input_mapper_focus_loss(frontend->input_mapper,
                                             releases, 3u, &release_count,
                                             error) != SEMU_OK) {
                return SEMU_STOP_DEVICE_REFUSED;
            }
            for (release_index = 0u; release_index < release_count;
                 ++release_index) {
                if (semu_machine_input(machine,
                                       &releases[release_index],
                                       error) != SEMU_OK) {
                    return SEMU_STOP_DEVICE_REFUSED;
                }
            }
            continue;
        }
        has_key = semu_sdl_input_process(frontend->input_adapter, &event,
                                          &key, &quit, error);
        if (quit) {
            return SEMU_STOP_USER;
        }
        if (!has_key) {
            continue;
        }
        if (semu_input_mapper_process(frontend->input_mapper, &key, &input,
                                      &has_key, error) != SEMU_OK) {
            return SEMU_STOP_DEVICE_REFUSED;
        }
        if (has_key && semu_machine_input(machine, &input, error) != SEMU_OK) {
            return SEMU_STOP_DEVICE_REFUSED;
        }
    }
    return SEMU_STOP_NONE;
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
    frontend.input_adapter = semu_sdl_input_create(&error);
    frontend.input_mapper = semu_input_mapper_create(&error);
    if (frontend.input_adapter == NULL || frontend.input_mapper == NULL) {
        fprintf(stderr, "SDL input: %s\n", error.text);
        semu_sdl_input_destroy(frontend.input_adapter);
        semu_input_mapper_destroy(frontend.input_mapper);
        SDL_Quit();
        return 2;
    }
    frontend.presenter = sdl_presenter_create(scale, &error);
    if (frontend.presenter == NULL) {
        fprintf(stderr, "SDL presenter: %s\n", error.text);
        semu_sdl_input_destroy(frontend.input_adapter);
        semu_input_mapper_destroy(frontend.input_mapper);
        SDL_Quit();
        return 2;
    }
    result = semu_cli_main(argc, argv, publish_frame, &frontend,
                           poll_input, &frontend);
    sdl_presenter_destroy(frontend.presenter);
    semu_sdl_input_destroy(frontend.input_adapter);
    semu_input_mapper_destroy(frontend.input_mapper);
    SDL_Quit();
    return result != 0 ? result : (frontend.failed ? 3 : 0);
}
