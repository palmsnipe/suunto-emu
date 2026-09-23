#ifndef SEMU_TRACE_H
#define SEMU_TRACE_H

#include "semu/types.h"

#include <stddef.h>

/*
 * Bounded trace contract (ticket 600).
 * Frozen typed event schema with explicit capacity and overflow
 * behavior.  No host pointers, absolute paths, or wall time.
 *
 * Schema version 1.
 */

#define SEMU_TRACE_SCHEMA_VERSION 1u
#define SEMU_TRACE_MAX_RECORDS 4096u

typedef enum {
    SEMU_TRACE_KIND_INSTRUCTION = 0,
    SEMU_TRACE_KIND_MMIO_READ,
    SEMU_TRACE_KIND_MMIO_WRITE,
    SEMU_TRACE_KIND_DEVICE,
    SEMU_TRACE_KIND_COMPAT,
    SEMU_TRACE_KIND_INPUT,
    SEMU_TRACE_KIND_FRAME
} semu_trace_kind;

typedef enum {
    SEMU_TRACE_OVERFLOW_STOP = 0,
    SEMU_TRACE_OVERFLOW_TRUNCATE
} semu_trace_overflow_policy;

typedef struct {
    uint32_t schema_version;
    uint32_t kind;
    uint64_t virtual_time_ns;
    uint64_t sequence;
    uint32_t pc;
    uint32_t addr;
    uint32_t value;
    uint32_t event_code;
    uint32_t width;
} semu_trace_record;

typedef struct semu_trace semu_trace;

semu_trace *semu_trace_create(uint32_t capacity,
    semu_trace_overflow_policy policy, semu_error *error);
void semu_trace_destroy(semu_trace *trace);
void semu_trace_reset(semu_trace *trace);

/*
 * Append a record.  Sequence is assigned monotonically.
 * Under STOP policy, returns SEMU_ERR_RANGE when full.
 * Under TRUNCATE policy, oldest records are overwritten.
 */
semu_status semu_trace_append(semu_trace *trace,
    const semu_trace_record *record, semu_error *error);

/* Number of records currently stored. */
size_t semu_trace_count(const semu_trace *trace);

/* Whether the trace overflowed (records were dropped). */
int semu_trace_overflowed(const semu_trace *trace);

/*
 * Retrieve a record by index (0 = oldest surviving).
 * Returns NULL if index >= count.
 */
const semu_trace_record *semu_trace_get(const semu_trace *trace,
    size_t index);

/*
 * Format records to a buffer.  Output is stable: identical event
 * sequences produce byte-identical output.  Returns bytes written
 * (excluding NUL).  Writes at most buf_size bytes.
 */
size_t semu_trace_format(const semu_trace *trace,
    char *buf, size_t buf_size);

/*
 * CPU fault reports (ticket 605).
 * Reports borrow immutable trace history and CPU inspection state.
 * They never read guest memory after the fault or mutate stop state.
 * No host paths or raw firmware bytes appear in formatted output.
 */
typedef struct semu_report_fault {
    semu_stop_reason stop_reason;
    uint32_t fault_instruction;
    uint32_t fault_address;
    int has_fault_address;
    uint32_t r[16];
    uint32_t xpsr;
    uint32_t primask;
    uint32_t basepri;
    uint32_t faultmask;
    uint32_t control;
    uint32_t fpscr;
    uint64_t instructions;
} semu_report_fault;

/*
 * Format a fault report to a buffer.  Output is stable: identical
 * fault data and trace history produce byte-identical output.
 * If trace is non-NULL, bounded preceding history is appended.
 * Returns bytes written (excluding NUL).  Writes at most buf_size bytes.
 */
size_t semu_report_fault_format(const semu_report_fault *report,
    const semu_trace *trace, char *buf, size_t buf_size);

/*
 * Versioned input recording and replay (ticket 610).
 * Strict parser/formatter with format version, exact profile and
 * firmware identity binding, ordered integer virtual times, and
 * semantic events.  Duplicate times preserve file order; time reversal
 * is rejected.  No wall time, SDL polling, or best-effort load.
 */
#include "semu/input.h"

#define SEMU_REPLAY_FORMAT_VERSION 1u
#define SEMU_REPLAY_MAX_EVENTS 1024u
#define SEMU_REPLAY_HASH_HEX_LEN (SEMU_SHA256_SIZE * 2u + 1u)

typedef struct semu_replay_event {
    uint64_t virtual_time_ns;
    semu_input_kind kind;
    uint32_t code;
    int32_t value;
    int32_t x;
    int32_t y;
} semu_replay_event;

struct semu_replay {
    uint32_t version;
    char profile_id[SEMU_ID_MAX];
    char firmware_hash[SEMU_REPLAY_HASH_HEX_LEN];
    semu_replay_event events[SEMU_REPLAY_MAX_EVENTS];
    size_t count;
};
typedef struct semu_replay semu_replay;

semu_replay *semu_replay_create(semu_error *error);
void semu_replay_destroy(semu_replay *replay);
void semu_replay_reset(semu_replay *replay);

/*
 * Parse strict ASCII replay text.  Validates version, identity,
 * event count, ordering, and all event fields before accepting.
 * Rejects unknown kinds/codes and time reversal.  Duplicate times
 * are allowed and preserve file order.
 * Returns SEMU_OK or an error code with a line-number message.
 */
semu_status semu_replay_parse(semu_replay *replay,
    const char *text, size_t text_size, semu_error *error);

/*
 * Format to stable byte-identical text.  Identical replays always
 * produce identical output.  Returns bytes written (excluding NUL).
 * Writes at most buf_size bytes.
 */
size_t semu_replay_format(const semu_replay *replay,
    char *buf, size_t buf_size);

size_t semu_replay_event_count(const semu_replay *replay);
const semu_replay_event *semu_replay_event_get(const semu_replay *replay,
    size_t index);
const char *semu_replay_profile_id(const semu_replay *replay);
const char *semu_replay_firmware_hash(const semu_replay *replay);
uint32_t semu_replay_version(const semu_replay *replay);

/*
 * Versioned machine snapshots (ticket 615).
 * Atomic serialize/restore with magic, version, identity binding,
 * and named binary sections.  Restore validates the entire image
 * before mutating the target.  No pointer serialization, compression
 * dependency, or built-in firmware bytes. Authentic machine snapshots are
 * private artifacts and must never be committed.
 */
#define SEMU_SNAPSHOT_MAGIC 0x53454D53u  /* "SEMS" */
#define SEMU_SNAPSHOT_VERSION 2u
#define SEMU_SNAPSHOT_MAX_SECTIONS 16u
#define SEMU_SNAPSHOT_MAX_SECTION_SIZE (4u * 1024u * 1024u)

/* Section IDs */
#define SEMU_SNAPSHOT_SECTION_CPU_STATE   0u
#define SEMU_SNAPSHOT_SECTION_RAM         1u
#define SEMU_SNAPSHOT_SECTION_VIRTUAL_TIME 2u
#define SEMU_SNAPSHOT_SECTION_STOP_REASON  3u
#define SEMU_SNAPSHOT_SECTION_SCHEDULER   4u
#define SEMU_SNAPSHOT_SECTION_SOC_STATE   5u
#define SEMU_SNAPSHOT_SECTION_DEVICES     6u
#define SEMU_SNAPSHOT_SECTION_STORAGE     7u
#define SEMU_SNAPSHOT_SECTION_NEMA        8u
#define SEMU_SNAPSHOT_SECTION_MACHINE     9u
#define SEMU_SNAPSHOT_SECTION_DISPLAY    10u

typedef struct semu_snapshot semu_snapshot;

semu_snapshot *semu_snapshot_create(semu_error *error);
void semu_snapshot_destroy(semu_snapshot *snap);
void semu_snapshot_reset(semu_snapshot *snap);

semu_status semu_snapshot_set_identity(semu_snapshot *snap,
    const char *profile_id, const char *firmware_hash,
    semu_error *error);

semu_status semu_snapshot_write_section(semu_snapshot *snap,
    uint32_t section_id, const uint8_t *data, size_t size,
    semu_error *error);

/*
 * Serialize to byte buffer (explicit little-endian, checked sizes).
 * Returns bytes written.  Writes at most buf_size bytes.
 */
size_t semu_snapshot_serialize(const semu_snapshot *snap,
    uint8_t *buf, size_t buf_size);

/*
 * Deserialize from a byte buffer.  Validates magic, version,
 * identity, all section lengths, and total size before accepting.
 * Returns SEMU_OK or an error code.  On failure, the snapshot is
 * left unchanged (atomic restore).
 */
semu_status semu_snapshot_deserialize(semu_snapshot *snap,
    const uint8_t *buf, size_t buf_size, semu_error *error);

semu_status semu_snapshot_read_section(const semu_snapshot *snap,
    uint32_t section_id, const uint8_t **out_data, size_t *out_size);

const char *semu_snapshot_profile_id(const semu_snapshot *snap);
const char *semu_snapshot_firmware_hash(const semu_snapshot *snap);
uint32_t semu_snapshot_version(const semu_snapshot *snap);
size_t semu_snapshot_section_count(const semu_snapshot *snap);

#endif
