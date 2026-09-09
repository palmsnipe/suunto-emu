/* Presentation glue for the SDL frontend (included by main_sdl.c, matching
 * the single-translation-unit frontend composition). Owns the visual-sink
 * burst coalescing: the emulator publishes one frame per guest publish-
 * flagged transaction, while a physical panel only ever latches at frame
 * boundaries. Animation redraws arrive as bursts of partial composites
 * minutes apart in virtual time (measured on the 2.22.60 w-ltim GPS search
 * ring: up to 84 composites per redraw tick, 0.36 ms median intra-burst
 * gap); presenting every intermediate strobes. Only the SDL texture and the
 * PPM dump consume the coalesced stream — the live checkpoint, setup-walk,
 * frame gate and counters keep observing every raw publish, so every
 * deterministic checkpoint is untouched. */

static void publish_frame(void *context, const semu_frame *frame)
{
    sdl_frontend *frontend = (sdl_frontend *)context;
    semu_error error;
    uint64_t now_ns = 0u;
    uint32_t crc = 0u;
    const semu_frame *out = frame;
    if (frontend->failed) {
        return;
    }
    if (frontend->machine != NULL) {
        now_ns = semu_machine_virtual_time(frontend->machine);
    }
    frontend->last_now_ns = now_ns;
    semu_error_clear(&error);
    if (frontend->coalesce != NULL) {
        out = semu_present_coalescer_observe(frontend->coalesce, frame,
                                             now_ns, &error);
        if (out == NULL && error.code != SEMU_OK) {
            fprintf(stderr, "SDL coalesce: %s\n", error.text);
            frontend->failed = 1;
            return;
        }
    }
    if (out != NULL) {
        semu_error_clear(&error);
        if (sdl_presenter_present(frontend->presenter, out, &error) !=
            SEMU_OK) {
            fprintf(stderr, "SDL present: %s\n", error.text);
            frontend->failed = 1;
            return;
        }
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
    if (frontend->ppm_dump_dir != NULL && out != NULL &&
        (frontend->frame_count == 1u || frontend->live_test.enabled)) {
        uint32_t dump_crc = out == frame ? crc : semu_crc32(0u, out->pixels,
                                                            out->size);
        if (dump_crc == 0u) {
            dump_crc = semu_crc32(0u, out->pixels, out->size);
        }
        semu_sdl_ppm_dump(frontend->ppm_dump_dir, &frontend->ppm_last_crc,
                          dump_crc, now_ns, out);
    }
    ++frontend->frame_count;
    semu_live_frame_gate_observe(&frontend->live_checkpoint,
                                 frontend->frame_count, now_ns, frame);
}

/* Opt-in threshold knob; default on at 10 ms of virtual time. Returns 0 on
 * success or 2 after reporting a refusal. */
static int configure_present_coalescing(sdl_frontend *frontend)
{
    const char *coalesce_ns = getenv("SEMU_SDL_PRESENT_COALESCE_NS");
    uint64_t threshold = UINT64_C(10000000);
    semu_error error;
    if (coalesce_ns != NULL) {
        char *end = NULL;
        unsigned long long v = strtoull(coalesce_ns, &end, 10);
        if (end == coalesce_ns || *end != '\0') {
            fputs("SDL coalesce: SEMU_SDL_PRESENT_COALESCE_NS must be a "
                  "virtual-time nanosecond integer\n", stderr);
            return 2;
        }
        threshold = (uint64_t)v;
    }
    if (threshold == 0u) {
        return 0;
    }
    semu_error_clear(&error);
    frontend->coalesce = semu_present_coalescer_create(threshold, &error);
    if (frontend->coalesce == NULL) {
        fprintf(stderr, "SDL coalesce: %s\n", error.text);
        return 2;
    }
    return 0;
}

/* Presents the final held composite once the run has stopped. */
static void flush_present_coalescing(sdl_frontend *frontend)
{
    const semu_frame *tail;
    semu_error error;
    if (frontend->coalesce == NULL || frontend->failed) {
        return;
    }
    tail = semu_present_coalescer_flush(frontend->coalesce);
    if (tail == NULL) {
        return;
    }
    semu_error_clear(&error);
    if (sdl_presenter_present(frontend->presenter, tail, &error) !=
        SEMU_OK) {
        fprintf(stderr, "SDL present: %s\n", error.text);
        frontend->failed = 1;
    } else if (frontend->ppm_dump_dir != NULL &&
        (frontend->frame_count == 1u || frontend->live_test.enabled)) {
        semu_sdl_ppm_dump(frontend->ppm_dump_dir, &frontend->ppm_last_crc,
            semu_crc32(0u, tail->pixels, tail->size),
            frontend->last_now_ns, tail);
    }
}
