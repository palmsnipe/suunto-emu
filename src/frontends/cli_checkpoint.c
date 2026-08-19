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

static void first_frame_gate_publish(void *context, const semu_frame *frame)
{
    first_frame_gate *gate = (first_frame_gate *)context;
    size_t index;
    if (gate == NULL || frame == NULL) {
        return;
    }
    if (frame->pixels == NULL || frame->size == 0u) {
        return;
    }
    if (gate->wait_for_input && !gate->input_seen) {
        return;
    }
    for (index = 0u; index < frame->size; ++index) {
        if (frame->pixels[index] != 0u) {
            break;
        }
    }
    if (index == frame->size) {
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
