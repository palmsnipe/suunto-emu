#ifndef SEMU_NEMA_DIAGNOSTICS_H
#define SEMU_NEMA_DIAGNOSTICS_H

#include "semu/types.h"

/*
 * Nema refusal diagnostics (ticket 506).
 * Bounded deterministic refusal records for malformed framing and
 * unsupported draw state.  No host path, time, or pointer stored.
 *
 * Evidence: E-NEMA-RING-001, E-NEMA-LISTS-001.
 */

#define NEMA_DIAG_MAX_CONTEXT 16u
#define NEMA_DIAG_MAX_RECORDS 64u

typedef enum {
    NEMA_DIAG_UNKNOWN_REGISTER = 0,
    NEMA_DIAG_UNSUPPORTED_DRAW,
    NEMA_DIAG_INCOMPLETE_STATE,
    NEMA_DIAG_BAD_PREFIX,
    NEMA_DIAG_BAD_ALIGNMENT,
    NEMA_DIAG_BAD_SIZE,
    NEMA_DIAG_TRUNCATED_LIST,
    NEMA_DIAG_FRAMING_ERROR
} nema_diag_category;

typedef struct {
    nema_diag_category category;
    uint32_t list_id;
    uint32_t source_addr;
    uint32_t reg_offset;
    uint32_t value;
    uint32_t context[NEMA_DIAG_MAX_CONTEXT];
    uint32_t context_count;
    int truncated;
} nema_diag_record;

typedef struct nema_diagnostics nema_diagnostics;

semu_status nema_diagnostics_create(nema_diagnostics **out,
                                     semu_error *error);
void nema_diagnostics_destroy(nema_diagnostics *diag);
void nema_diagnostics_reset(nema_diagnostics *diag);

/*
 * Record a refusal.  Context is copied up to NEMA_DIAG_MAX_CONTEXT
 * words; truncated flag is set if context_count exceeds the limit.
 * No host path or pointer is stored.
 */
semu_status nema_diagnostics_record(nema_diagnostics *diag,
                                     nema_diag_category category,
                                     uint32_t list_id,
                                     uint32_t source_addr,
                                     uint32_t reg_offset,
                                     uint32_t value,
                                     const uint32_t *context,
                                     uint32_t context_count,
                                     semu_error *error);

const nema_diag_record *nema_diagnostics_records(
    const nema_diagnostics *diag, size_t *count);

const char *nema_diag_category_name(nema_diag_category cat);

/*
 * Format a record to a stable string.  Contains no host path, time,
 * or pointer — only category, list ID, addresses, register, value,
 * and context words.  Returns string length (excluding NUL).
 */
size_t nema_diag_format(const nema_diag_record *rec,
                         char *buf, size_t buf_size);

#endif
