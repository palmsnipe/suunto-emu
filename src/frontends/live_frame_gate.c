#include "live_frame_gate.h"

#include <string.h>

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

static void reset_stability(semu_live_frame_gate *gate)
{
    gate->last_frame_time = 0u;
    gate->candidate_frame_count = 0u;
    gate->candidate_valid = 0;
}

void semu_live_frame_gate_init(semu_live_frame_gate *gate,
    int required_button)
{
    if (gate == NULL) {
        return;
    }
    memset(gate, 0, sizeof(*gate));
    gate->required_button = required_button;
}

void semu_live_frame_gate_note_input(semu_live_frame_gate *gate,
    uint64_t frame_count, const semu_input_event *input)
{
    if (gate == NULL || input == NULL || gate->required_button < 0 ||
        gate->input_seen || gate->ready || gate->consumed ||
        input->kind != SEMU_INPUT_BUTTON || input->value != 0 ||
        input->code != (uint32_t)gate->required_button) {
        return;
    }
    gate->input_seen = 1;
    gate->frame_baseline = frame_count;
    reset_stability(gate);
}

int semu_live_frame_gate_observe(semu_live_frame_gate *gate,
    uint64_t frame_count, uint64_t now_ns, const semu_frame *frame)
{
    if (gate == NULL || frame == NULL || gate->required_button < 0 ||
        !gate->input_seen || gate->ready || gate->consumed ||
        frame_count <= gate->frame_baseline || !frame_has_pixels(frame)) {
        return 0;
    }
    gate->last_frame_time = now_ns;
    gate->candidate_frame_count = frame_count;
    gate->candidate_valid = 1;
    return 0;
}

int semu_live_frame_gate_settle(semu_live_frame_gate *gate,
    uint64_t frame_count, uint64_t now_ns)
{
    uint64_t deadline;
    if (gate == NULL || gate->ready || gate->consumed ||
        !gate->candidate_valid || frame_count != gate->candidate_frame_count ||
        now_ns < gate->last_frame_time) {
        return 0;
    }
    if (gate->last_frame_time > UINT64_MAX - SEMU_LIVE_FRAME_SETTLE_NS) {
        deadline = UINT64_MAX;
    } else {
        deadline = gate->last_frame_time + SEMU_LIVE_FRAME_SETTLE_NS;
    }
    if (now_ns < deadline) {
        return 0;
    }
    gate->ready = 1;
    return 1;
}

void semu_live_frame_gate_consume(semu_live_frame_gate *gate)
{
    if (gate == NULL) {
        return;
    }
    gate->ready = 0;
    gate->consumed = 1;
}
