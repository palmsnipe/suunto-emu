#include "live_frame_gate.h"

#include <string.h>

static int frame_shape_is_valid(const semu_frame *frame)
{
    uint64_t row_bytes;
    uint64_t total_bytes;
    if (frame == NULL || frame->pixels == NULL ||
        frame->format != SEMU_PIXEL_RGB565_LE || frame->width == 0u ||
        frame->height == 0u) {
        return 0;
    }
    row_bytes = (uint64_t)frame->width * UINT64_C(2);
    total_bytes = (uint64_t)frame->stride * (uint64_t)frame->height;
    return frame->stride >= row_bytes && total_bytes <= (uint64_t)frame->size;
}

static int frame_has_visible_pixels(const semu_frame *frame)
{
    uint32_t y;
    uint64_t row_bytes;
    if (!frame_shape_is_valid(frame)) {
        return 0;
    }
    row_bytes = (uint64_t)frame->width * UINT64_C(2);
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

static void remember_generation(semu_live_frame_gate *gate,
    const semu_frame *frame)
{
    if (gate != NULL && frame_shape_is_valid(frame)) {
        gate->last_generation = frame->generation;
        gate->last_generation_valid = 1;
    }
}

static int generation_is_new(const semu_live_frame_gate *gate,
    const semu_frame *frame)
{
    if (gate == NULL || frame == NULL || !gate->generation_baseline_valid) {
        return 1;
    }
    return frame->generation != gate->generation_baseline;
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
        gate->input_seen || gate->ready ||
        input->kind != SEMU_INPUT_BUTTON || input->value != 0 ||
        (!gate->accept_any_button &&
         input->code != (uint32_t)gate->required_button)) {
        return;
    }
    gate->input_seen = 1;
    gate->frame_baseline = frame_count;
    gate->generation_baseline = gate->last_generation;
    gate->generation_baseline_valid = gate->last_generation_valid;
    reset_stability(gate);
}

int semu_live_frame_gate_accepts_button(
    const semu_live_frame_gate *gate, uint32_t button)
{
    if (gate == NULL || gate->required_button < 0 ||
        gate->input_seen || gate->ready || gate->accept_any_button) {
        return 1;
    }
    return button == (uint32_t)gate->required_button;
}

int semu_live_frame_gate_waiting(const semu_live_frame_gate *gate,
    uint64_t frame_count)
{
    if (gate == NULL || gate->required_button < 0) {
        return 0;
    }
    if (!gate->input_seen) {
        return frame_count != 0u;
    }
    return gate->ready;
}

int semu_live_frame_gate_observe(semu_live_frame_gate *gate,
    uint64_t frame_count, uint64_t now_ns, const semu_frame *frame)
{
    if (gate == NULL || frame == NULL || gate->required_button < 0) {
        return 0;
    }
    remember_generation(gate, frame);
    if (!gate->input_seen || gate->ready ||
        frame_count <= gate->frame_baseline ||
        !frame_has_visible_pixels(frame) || !generation_is_new(gate, frame)) {
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
    if (gate == NULL || gate->ready ||
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

void semu_live_frame_gate_consume(semu_live_frame_gate *gate,
    uint64_t frame_count)
{
    if (gate == NULL || !gate->ready) {
        return;
    }
    gate->ready = 0;
    gate->input_seen = 0;
    gate->accept_any_button = 1;
    gate->frame_baseline = frame_count;
    reset_stability(gate);
}
