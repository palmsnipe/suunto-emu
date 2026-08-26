#include "cli.h"
#include "sdl_options.h"

#include <SDL3/SDL.h>

#include "semu/hash.h"

#include "sdl_present_core.c"
#include "sdl_options.c"
#include "sdl_present.c"
#include "sdl_input.c"
#include "semantic_input.c"
#include "live_frame_gate.c"
#include "sdl_button_hold.c"
#include "sdl_live_test.c"
#include "sdl_ppm_dump.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct sdl_frontend {
    sdl_presenter *presenter;
    semu_sdl_input_adapter *input_adapter;
    semu_input_mapper *input_mapper;
    semu_live_frame_gate live_checkpoint;
    semu_sdl_button_hold button_hold;
    semu_machine *machine;
    unsigned long frame_count;
    uint64_t last_frame_generation;
    uint32_t last_frame_crc;
    uint32_t scale;
    uint32_t viewport_height;
    int failed;
    int window_closed;
    const char *live_checkpoint_label;
    const char *ppm_dump_dir;
    uint32_t ppm_last_crc;
    semu_sdl_live_test live_test;
} sdl_frontend;

static const char *live_checkpoint_name(const sdl_frontend *frontend)
{
    if (frontend != NULL && frontend->live_checkpoint_label != NULL) {
        return frontend->live_checkpoint_label;
    }
    if (frontend != NULL &&
        frontend->live_checkpoint.required_button == SEMU_BUTTON_MIDDLE) {
        return "middle-language";
    }
    if (frontend != NULL &&
        frontend->live_checkpoint.required_button == SEMU_BUTTON_LOWER) {
        return "lower-transition";
    }
    return "unknown";
}

static int parse_live_checkpoint(int argc, char **argv, const char **label)
{
    const char *until = NULL;
    int has_replay = 0;
    int index;
    if (label != NULL) {
        *label = NULL;
    }
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
        if (label != NULL) {
            *label = "middle-language";
        }
        return SEMU_BUTTON_MIDDLE;
    }
    if (strcmp(until, "setup-next") == 0) {
        if (label != NULL) {
            *label = "setup-next";
        }
        return SEMU_BUTTON_MIDDLE;
    }
    if (strcmp(until, "lower-transition") == 0) {
        if (label != NULL) {
            *label = "lower-transition";
        }
        return SEMU_BUTTON_LOWER;
    }
    return -1;
}

static void observe_live_input(sdl_frontend *frontend,
    const semu_input_event *input)
{
    if (frontend == NULL || input == NULL) {
        return;
    }
    semu_live_frame_gate_note_input(&frontend->live_checkpoint,
                                    frontend->frame_count, input);
}

static void publish_frame(void *context, const semu_frame *frame)
{
    sdl_frontend *frontend = (sdl_frontend *)context;
    semu_error error;
    uint64_t now_ns = 0u;
    uint32_t crc = 0u;
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
    frontend->viewport_height = frame->height * frontend->scale;
    semu_sdl_input_set_viewport_height(frontend->input_adapter,
                                       frontend->viewport_height);
    if (frontend->frame_count == 0u || frontend->live_test.enabled) {
        crc = semu_crc32(0u, frame->pixels, frame->size);
    }
    if (frontend->frame_count == 0u) {
        fprintf(stderr,
                "SDL first-frame width=%u height=%u generation=%llu "
                "crc32=%08x\n",
                frame->width, frame->height,
                (unsigned long long)frame->generation,
                crc);
    }
    frontend->last_frame_generation = frame->generation;
    frontend->last_frame_crc = crc;
    if (frontend->machine != NULL) {
        now_ns = semu_machine_virtual_time(frontend->machine);
    }
    if (frontend->ppm_dump_dir != NULL &&
        (frontend->frame_count == 1u || frontend->live_test.enabled)) {
        uint32_t dump_crc = crc;
        if (dump_crc == 0u) {
            dump_crc = semu_crc32(0u, frame->pixels, frame->size);
        }
        semu_sdl_ppm_dump(frontend->ppm_dump_dir, &frontend->ppm_last_crc,
                          dump_crc, now_ns, frame);
    }
    ++frontend->frame_count;
    semu_live_frame_gate_observe(&frontend->live_checkpoint,
                                 frontend->frame_count, now_ns, frame);
}

static semu_stop_reason process_normalized_key(sdl_frontend *frontend,
    semu_machine *machine, const semu_normalized_key *key,
    int wait_for_button, semu_error *error)
{
    semu_input_event input;
    semu_button_id button;
    int has_input = 0;
    if (key->down && !key->repeat &&
        semu_input_mapper_button_for_key(frontend->input_mapper,
                                         key->key, &button) &&
        !semu_live_frame_gate_accepts_button(
            &frontend->live_checkpoint, frontend->frame_count, button)) {
        return SEMU_STOP_NONE;
    }
    if (semu_input_mapper_process(frontend->input_mapper, key, &input,
                                  &has_input, error) != SEMU_OK) {
        return SEMU_STOP_DEVICE_REFUSED;
    }
    if (!has_input) {
        return SEMU_STOP_NONE;
    }
    if (semu_machine_input(machine, &input, error) != SEMU_OK) {
        return SEMU_STOP_DEVICE_REFUSED;
    }
    if (input.value == 0) {
        if (wait_for_button && frontend->live_checkpoint.ready) {
            semu_live_frame_gate_consume(&frontend->live_checkpoint,
                                         frontend->frame_count);
        }
        semu_sdl_button_hold_note_press(&frontend->button_hold, &input, key);
        observe_live_input(frontend, &input);
    }
    return SEMU_STOP_NONE;
}

static semu_stop_reason flush_button_releases(sdl_frontend *frontend,
    semu_machine *machine, uint64_t now_ns, semu_error *error)
{
    semu_normalized_key keys[SEMU_SDL_BUTTON_COUNT];
    uint32_t count;
    uint32_t index;
    count = semu_sdl_button_hold_flush(&frontend->button_hold, now_ns,
                                       keys, SEMU_SDL_BUTTON_COUNT);
    for (index = 0u; index < count; ++index) {
        if (process_normalized_key(frontend, machine, &keys[index], 0,
                                   error) != SEMU_STOP_NONE) {
            return SEMU_STOP_DEVICE_REFUSED;
        }
    }
    return SEMU_STOP_NONE;
}

static semu_stop_reason poll_input(void *context, semu_machine *machine,
                                    semu_error *error)
{
    sdl_frontend *frontend = (sdl_frontend *)context;
    SDL_Event event;
    int wait_for_button;
    uint64_t now_ns;

    if (frontend == NULL || machine == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "SDL input poll has no runtime context");
        return SEMU_STOP_DEVICE_REFUSED;
    }
    if (frontend->failed) {
        semu_error_set(error, SEMU_ERR_IO,
                       "SDL presenter failed while publishing a frame");
        return SEMU_STOP_DEVICE_REFUSED;
    }
    frontend->machine = machine;
    now_ns = semu_machine_virtual_time(machine);
    if (semu_live_frame_gate_settle(&frontend->live_checkpoint,
                                    frontend->frame_count, now_ns)) {
        fprintf(stderr,
                "SDL live checkpoint %s ready; press Up, Down, or "
                "Return/Enter to continue\n",
                live_checkpoint_name(frontend));
    }
    if (flush_button_releases(frontend, machine, now_ns, error) !=
        SEMU_STOP_NONE) {
        return SEMU_STOP_DEVICE_REFUSED;
    }
    wait_for_button = semu_live_frame_gate_waiting(
                          &frontend->live_checkpoint,
                          frontend->frame_count) &&
                      !semu_sdl_button_hold_waiting(&frontend->button_hold,
                                                    now_ns);
    if (!semu_sdl_live_test_queue(&frontend->live_test, wait_for_button,
            frontend->live_checkpoint.ready, frontend->viewport_height,
            frontend->last_frame_generation, frontend->last_frame_crc,
            now_ns, error)) {
        return SEMU_STOP_DEVICE_REFUSED;
    }
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
        semu_input_event releases[3];
        uint32_t release_count;
        uint32_t release_index;
        int quit = 0;
        int has_key;
        semu_error_clear(error);
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            frontend->window_closed = 1;
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
            semu_sdl_button_hold_reset(&frontend->button_hold);
            continue;
        }
        has_key = semu_sdl_input_process(frontend->input_adapter, &event,
                                          &key, &quit, error);
        if (quit) {
            frontend->window_closed = 1;
            return SEMU_STOP_USER;
        }
        if (!has_key) {
            continue;
        }
        now_ns = semu_machine_virtual_time(machine);
        if (!key.down && semu_sdl_button_hold_note_release(
                &frontend->button_hold, &key, now_ns)) {
            continue;
        }
        if (key.down && !key.repeat &&
            !semu_sdl_button_hold_press_allowed(&frontend->button_hold,
                                                &key, now_ns)) {
            continue;
        }
        if (process_normalized_key(frontend, machine, &key,
                                   wait_for_button, error) != SEMU_STOP_NONE) {
            return SEMU_STOP_DEVICE_REFUSED;
        }
        if (wait_for_button && !semu_live_frame_gate_waiting(
                &frontend->live_checkpoint, frontend->frame_count)) {
            return SEMU_STOP_NONE;
        }
    }
    return SEMU_STOP_NONE;
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
    const char *live_test;
    memset(&frontend, 0, sizeof(frontend));
    semu_live_frame_gate_init(&frontend.live_checkpoint, -1);
    semu_sdl_button_hold_init(&frontend.button_hold);
    live_test = getenv("SEMU_SDL_LIVE_TEST");
    if (live_test != NULL && strcmp(live_test, "setup-walk") != 0 &&
        strcmp(live_test, "middle-language") != 0) {
        fputs("SDL live test: SEMU_SDL_LIVE_TEST accepts only "
              "middle-language or setup-walk\n", stderr);
        return 2;
    }
    frontend.live_test.enabled = live_test != NULL;
    frontend.live_test.setup_walk = live_test != NULL &&
                                    strcmp(live_test, "setup-walk") == 0;
    frontend.ppm_dump_dir = getenv("SEMU_SDL_PPM_DIR");
    {
        const char *post = getenv("SEMU_SDL_SETUP_WALK_POST");
        size_t n;
        int ok = 1;
        size_t i;
        frontend.live_test.post_buttons = NULL;
        if (post != NULL) {
            n = strlen(post);
            for (i = 0; i < n; ++i) {
                char c = post[i];
                if (c != 'u' && c != 'm' && c != 'l') {
                    ok = 0;
                    break;
                }
            }
            if (!ok || n == 0u || n > 32u) {
                fputs("SDL setup-walk: SEMU_SDL_SETUP_WALK_POST must be a "
                      "non-empty run of u/m/l letters (max 32)\n", stderr);
                SDL_Quit();
                return 2;
            }
            frontend.live_test.post_buttons = post;
        }
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL initialization: %s\n", SDL_GetError());
        return 2;
    }
    filtered_argc = filter_sdl_options(argc, argv, filtered_argv,
                                       &wait_for_quit);
    semu_live_frame_gate_init(&frontend.live_checkpoint,
                              parse_live_checkpoint(filtered_argc,
                                                   filtered_argv,
                                                   &frontend.live_checkpoint_label));
    if (frontend.live_test.enabled && strcmp(live_test, "middle-language") == 0 &&
        frontend.live_checkpoint.required_button != SEMU_BUTTON_MIDDLE) {
        fputs("SDL live test requires --until middle-language without "
              "--input-replay\n", stderr);
        SDL_Quit();
        return 2;
    }
    if (frontend.live_test.setup_walk &&
        (frontend.live_checkpoint.required_button != SEMU_BUTTON_MIDDLE ||
         frontend.live_checkpoint_label == NULL ||
         strcmp(frontend.live_checkpoint_label, "setup-next") != 0)) {
        fputs("SDL setup-walk requires --until setup-next without "
              "--input-replay\n", stderr);
        SDL_Quit();
        return 2;
    }
    semu_error_clear(&error);
    if (semu_sdl_parse_scale(filtered_argc, filtered_argv, &scale, &error) !=
        SEMU_OK) {
        fprintf(stderr, "SDL options: %s\n", error.text);
        SDL_Quit();
        return 2;
    }
    frontend.scale = scale;
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
        !frontend.failed && !frontend.window_closed) {
        fputs("SDL frame ready; close the window to exit\n", stderr);
        wait_for_window_close();
    }
    sdl_presenter_destroy(frontend.presenter);
    semu_sdl_input_destroy(frontend.input_adapter);
    semu_input_mapper_destroy(frontend.input_mapper);
    SDL_Quit();
    return result != 0 ? result : (frontend.failed ? 3 : 0);
}
