/*
 * CLI frame checkpoints and replay input observation.
 * Included by cli.c so the normal headless build keeps one frontend object.
 */

typedef struct first_frame_gate {
    semu_frame_callback callback;
    void *callback_context;
    int reached;
    int wait_for_input;
    uint32_t required_button;
    int input_seen;
} first_frame_gate;

typedef struct replay_input_context {
    semu_machine *machine;
    first_frame_gate *frame_gate;
} replay_input_context;

static void first_frame_gate_note_input(first_frame_gate *gate,
    const semu_input_event *event)
{
    if (gate == NULL || event == NULL || !gate->wait_for_input ||
        event->kind != SEMU_INPUT_BUTTON ||
        event->code != gate->required_button || event->value != 0) {
        return;
    }
    gate->input_seen = 1;
}

static int first_frame_has_visible_pixels(const semu_frame *frame)
{
    uint32_t y;
    uint64_t row_bytes;
    uint64_t total_bytes;
    if (frame == NULL || frame->pixels == NULL ||
        frame->format != SEMU_PIXEL_RGB565_LE || frame->width == 0u ||
        frame->height == 0u) {
        return 0;
    }
    row_bytes = (uint64_t)frame->width * UINT64_C(2);
    total_bytes = (uint64_t)frame->stride * (uint64_t)frame->height;
    if (frame->stride < row_bytes || total_bytes > (uint64_t)frame->size) {
        return 0;
    }
    for (y = 0u; y < frame->height; ++y) {
        uint64_t offset = (uint64_t)y * (uint64_t)frame->stride;
        uint64_t x;
        for (x = 0u; x < row_bytes; ++x) {
            if (frame->pixels[(size_t)(offset + x)] != 0u) {
                return 1;
            }
        }
    }
    return 0;
}

static void first_frame_gate_publish(void *context, const semu_frame *frame)
{
    first_frame_gate *gate = (first_frame_gate *)context;
    if (gate == NULL || frame == NULL) {
        return;
    }
    if (gate->wait_for_input && !gate->input_seen) {
        return;
    }
    if (!first_frame_has_visible_pixels(frame)) {
        return;
    }
    if (gate->callback != NULL) {
        gate->callback(gate->callback_context, frame);
    }
    gate->reached = 1;
}

static int replay_sink(void *context, const semu_input_event *event,
    uint64_t time_ns, uint32_t ordinal)
{
    replay_input_context *replay_context =
        (replay_input_context *)context;
    semu_error err;
    (void)time_ns;
    (void)ordinal;
    semu_error_clear(&err);
    if (semu_machine_input(replay_context->machine, event, &err) != SEMU_OK) {
        return 1;
    }
    first_frame_gate_note_input(replay_context->frame_gate, event);
    return 0;
}
