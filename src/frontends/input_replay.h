#ifndef SEMU_FRONTENDS_INPUT_REPLAY_H
#define SEMU_FRONTENDS_INPUT_REPLAY_H

#include "semu/input.h"
#include "semu/types.h"

#include <stddef.h>

/*
 * Deterministic input replay (ticket 518).
 * Parses a bounded, dependency-free ASCII replay format into a
 * queue of semantic button events.  Validates time ordering and
 * press/release state before scheduling.  No JSON, SDL, or GPIO.
 *
 * Evidence: E-SAP-INPUT-REPLAY-001 (native middle/lower order).
 */

#define INPUT_REPLAY_MAX_EVENTS 1024u

/* Sink callback for replay events.  Return 0 to continue,
 * non-zero to stop replay (refusal). */
typedef int (*semu_input_replay_sink)(void *context,
                                        const semu_input_event *event,
                                        uint64_t time_ns,
                                        uint32_t ordinal);

typedef struct semu_input_replay semu_input_replay;

semu_input_replay *semu_input_replay_create(semu_error *error);
void semu_input_replay_destroy(semu_input_replay *replay);
void semu_input_replay_reset(semu_input_replay *replay);

/*
 * Parse ASCII text into the replay queue.
 * Format: one event per line, "TIME_NS button CODE VALUE".
 * CODE: upper|middle|lower.  VALUE: press|release.
 * '#' starts a comment.  Blank lines are skipped.
 * Validates nondecreasing time and press/release consistency.
 * Returns SEMU_OK or SEMU_ERR_FORMAT with a line-number message.
 */
semu_status semu_input_replay_parse(semu_input_replay *replay,
    const char *text, size_t text_size, semu_error *error);

/* Read and parse a bounded replay file without truncating its contents. */
semu_status semu_input_replay_parse_file(semu_input_replay *replay,
    const char *path, semu_error *error);

/* Number of events parsed. */
size_t semu_input_replay_count(const semu_input_replay *replay);

/*
 * Emit events whose time <= current_time_ns, in order.
 * Calls sink for each event.  Stops on sink refusal (out_refused=1).
 * Returns number of events emitted.
 */
size_t semu_input_replay_pump(semu_input_replay *replay,
    uint64_t current_time_ns,
    semu_input_replay_sink sink, void *sink_context,
    int *out_refused);

#endif
