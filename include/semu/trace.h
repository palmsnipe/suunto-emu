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

#endif
