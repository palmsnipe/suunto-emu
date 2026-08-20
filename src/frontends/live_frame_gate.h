#ifndef SEMU_FRONTENDS_LIVE_FRAME_GATE_H
#define SEMU_FRONTENDS_LIVE_FRAME_GATE_H

#include "semu/frame.h"
#include "semu/input.h"
#include "semu/types.h"

#define SEMU_LIVE_FRAME_SETTLE_NS UINT64_C(350000000)

/*
 * Live SDL checkpoint state.  This gate only observes published frames; it
 * never changes guest execution or renderer state.
 */
typedef struct semu_live_frame_gate {
    /* The named checkpoint button is required only for the first edge. */
    int required_button;
    /* After a settled frame, setup navigation accepts any mapped button. */
    int accept_any_button;
    uint64_t frame_baseline;
    uint64_t last_frame_time;
    uint64_t candidate_frame_count;
    int input_seen;
    int ready;
    int candidate_valid;
} semu_live_frame_gate;

void semu_live_frame_gate_init(semu_live_frame_gate *gate,
    int required_button);
void semu_live_frame_gate_note_input(semu_live_frame_gate *gate,
    uint64_t frame_count, const semu_input_event *input);
int semu_live_frame_gate_waiting(const semu_live_frame_gate *gate,
    uint64_t frame_count);
int semu_live_frame_gate_observe(semu_live_frame_gate *gate,
    uint64_t frame_count, uint64_t now_ns, const semu_frame *frame);
int semu_live_frame_gate_settle(semu_live_frame_gate *gate,
    uint64_t frame_count, uint64_t now_ns);
void semu_live_frame_gate_consume(semu_live_frame_gate *gate,
    uint64_t frame_count);

#endif
