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
    unsigned long frame_count;
    int live_checkpoint_button;
    uint64_t live_frame_baseline;
    int live_input_seen;
    int live_checkpoint_ready;
    int live_checkpoint_consumed;
    int failed;
} sdl_frontend;

static int frame_has_pixels(const semu_frame *frame)
{
    size_t index;
    if (frame == NULL || frame->pixels == NULL || frame->size == 0u) {
        return 0;
    }
    for (index = 0u; index < frame->size; ++index) {
        if (frame->pixels[index] != 0u) {
            return 1;
        }
    }
    return 0;
}

static const char *live_checkpoint_name(int button)
{
    return button == SEMU_BUTTON_MIDDLE ? "middle-language" :
           button == SEMU_BUTTON_LOWER ? "lower-transition" : "unknown";
}

static int parse_live_checkpoint(int argc, char **argv)
{
    const char *until = NULL;
    int has_replay = 0;
    int index;
    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--until") == 0 && index + 1 < argc) {
            until = argv[++index];
        } else if (strcmp(argv[index], "--input-replay") == 0 &&
                   index + 1 < argc) {
            has_replay = 1;
            ++index;
        }
    }
    if (has_replay || until == NULL) {
        return -1;
    }
    if (strcmp(until, "middle-language") == 0) {
        return SEMU_BUTTON_MIDDLE;
    }
    if (strcmp(until, "lower-transition") == 0) {
        return SEMU_BUTTON_LOWER;
    }
    return -1;
}

static void observe_live_input(sdl_frontend *frontend,
    const semu_input_event *input)
{
    if (frontend == NULL || input == NULL || frontend->live_checkpoint_button < 0 ||
        frontend->live_checkpoint_consumed || frontend->live_checkpoint_ready ||
        frontend->live_checkpoint_button != (int)input->code ||
        input->kind != SEMU_INPUT_BUTTON || input->value != 0) {
        return;
    }
    frontend->live_input_seen = 1;
    frontend->live_frame_baseline = frontend->frame_count;
}

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
    if (frontend->frame_count == 0u) {
        fprintf(stderr, "SDL first-frame width=%u height=%u generation=%llu\n",
                frame->width, frame->height,
                (unsigned long long)frame->generation);
    }
    ++frontend->frame_count;
    if (frontend->live_checkpoint_button >= 0 &&
        frontend->live_input_seen && !frontend->live_checkpoint_ready &&
        !frontend->live_checkpoint_consumed &&
        (uint64_t)frontend->frame_count > frontend->live_frame_baseline &&
        frame_has_pixels(frame)) {
        frontend->live_checkpoint_ready = 1;
        fprintf(stderr, "SDL live checkpoint %s ready; press a button to continue\n",
                live_checkpoint_name(frontend->live_checkpoint_button));
    }
}

static semu_stop_reason poll_input(void *context, semu_machine *machine,
                                    semu_error *error)
{
    sdl_frontend *frontend = (sdl_frontend *)context;
    SDL_Event event;
    int wait_for_button;

    if (frontend == NULL || machine == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "SDL input poll has no runtime context");
        return SEMU_STOP_DEVICE_REFUSED;
    }
    wait_for_button = frontend->live_checkpoint_ready &&
                      !frontend->live_checkpoint_consumed;
    for (;;) {
        if (wait_for_button) {
            if (!SDL_WaitEvent(&event)) {
                semu_error_set(error, SEMU_ERR_IO,
                               "SDL event wait failed: %s", SDL_GetError());
                return SEMU_STOP_DEVICE_REFUSED;
            }
        } else if (!SDL_PollEvent(&event)) {
            break;
        }
        semu_normalized_key key;
        semu_input_event input;
        semu_input_event releases[3];
        uint32_t release_count;
        uint32_t release_index;
        int quit = 0;
        int has_key;
        semu_error_clear(error);
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            return SEMU_STOP_USER;
        }
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
        if (has_key) {
            observe_live_input(frontend, &input);
            if (wait_for_button && input.value == 0) {
                frontend->live_checkpoint_ready = 0;
                frontend->live_checkpoint_consumed = 1;
                return SEMU_STOP_NONE;
            }
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

static int filter_sdl_options(int argc, char **argv, char **filtered,
                              int *wait_for_quit)
{
    int i;
    int filtered_count = 0;
    *wait_for_quit = 0;
    for (i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--wait-for-quit") == 0) {
            *wait_for_quit = 1;
            continue;
        }
        filtered[filtered_count++] = argv[i];
    }
    return filtered_count;
}

static void wait_for_window_close(void)
{
    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT ||
            event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            break;
        }
    }
}

int main(int argc, char **argv)
{
    char *filtered_argv[argc > 0 ? (size_t)argc : 1u];
    sdl_frontend frontend;
    semu_error error;
    int result;
    uint32_t scale;
    int wait_for_quit;
    int filtered_argc;
    memset(&frontend, 0, sizeof(frontend));
    frontend.live_checkpoint_button = -1;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL initialization: %s\n", SDL_GetError());
        return 2;
    }
    filtered_argc = filter_sdl_options(argc, argv, filtered_argv,
                                       &wait_for_quit);
    frontend.live_checkpoint_button = parse_live_checkpoint(filtered_argc,
                                                            filtered_argv);
    scale = parse_scale(filtered_argc, filtered_argv);
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
    result = semu_cli_main(filtered_argc, filtered_argv, publish_frame,
                           &frontend, poll_input, &frontend);
    if (wait_for_quit && result == 0 && frontend.frame_count > 0u &&
        !frontend.failed) {
        fputs("SDL frame ready; close the window to exit\n", stderr);
        wait_for_window_close();
    }
    sdl_presenter_destroy(frontend.presenter);
    semu_sdl_input_destroy(frontend.input_adapter);
    semu_input_mapper_destroy(frontend.input_mapper);
    SDL_Quit();
    return result != 0 ? result : (frontend.failed ? 3 : 0);
}
