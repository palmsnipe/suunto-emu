/*
 * Nema refusal diagnostics (ticket 506).
 * Bounded deterministic refusal records with stable formatting.
 */

#include "nema_diagnostics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct nema_diagnostics {
    nema_diag_record records[NEMA_DIAG_MAX_RECORDS];
    size_t count;
};

static const char *CATEGORY_NAMES[] = {
    "unknown_register",
    "unsupported_draw",
    "incomplete_state",
    "bad_prefix",
    "bad_alignment",
    "bad_size",
    "truncated_list",
    "framing_error"
};

#define CATEGORY_COUNT \
    (sizeof(CATEGORY_NAMES) / sizeof(CATEGORY_NAMES[0u]))

const char *nema_diag_category_name(nema_diag_category cat)
{
    if ((size_t)cat < CATEGORY_COUNT) {
        return CATEGORY_NAMES[cat];
    }
    return "unknown";
}

semu_status nema_diagnostics_create(nema_diagnostics **out,
                                     semu_error *error)
{
    nema_diagnostics *diag;

    diag = (nema_diagnostics *)calloc(1u, sizeof(*diag));
    if (diag == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "diag: alloc failed");
        return SEMU_ERR_NOMEM;
    }
    *out = diag;
    return SEMU_OK;
}

void nema_diagnostics_destroy(nema_diagnostics *diag)
{
    free(diag);
}

void nema_diagnostics_reset(nema_diagnostics *diag)
{
    if (diag == NULL) return;
    memset(diag->records, 0, sizeof(diag->records));
    diag->count = 0u;
}

semu_status nema_diagnostics_record(nema_diagnostics *diag,
                                     nema_diag_category category,
                                     uint32_t list_id,
                                     uint32_t source_addr,
                                     uint32_t reg_offset,
                                     uint32_t value,
                                     const uint32_t *context,
                                     uint32_t context_count,
                                     semu_error *error)
{
    nema_diag_record *rec;

    if (diag == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "diag: null");
        return SEMU_ERR_ARGUMENT;
    }
    if (diag->count >= NEMA_DIAG_MAX_RECORDS) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "diag: record overflow");
        return SEMU_ERR_UNSUPPORTED;
    }

    rec = &diag->records[diag->count];
    memset(rec, 0, sizeof(*rec));
    rec->category = category;
    rec->list_id = list_id;
    rec->source_addr = source_addr;
    rec->reg_offset = reg_offset;
    rec->value = value;

    if (context != NULL && context_count > 0u) {
        uint32_t to_copy = context_count;
        if (to_copy > NEMA_DIAG_MAX_CONTEXT) {
            to_copy = NEMA_DIAG_MAX_CONTEXT;
            rec->truncated = 1;
        }
        memcpy(rec->context, context, to_copy * sizeof(uint32_t));
        rec->context_count = to_copy;
    }

    ++diag->count;
    return SEMU_OK;
}

const nema_diag_record *nema_diagnostics_records(
    const nema_diagnostics *diag, size_t *count)
{
    if (count != NULL) *count = (diag != NULL) ? diag->count : 0u;
    return (diag != NULL) ? diag->records : NULL;
}

size_t nema_diag_format(const nema_diag_record *rec,
                         char *buf, size_t buf_size)
{
    size_t pos = 0u;
    size_t i;

    if (buf == NULL || buf_size == 0u || rec == NULL) return 0u;

    pos += (size_t)snprintf(buf + pos, buf_size - pos,
                            "refusal cat=%s list=%u addr=0x%08x "
                            "reg=0x%06x val=0x%08x ctx=%u",
                            nema_diag_category_name(rec->category),
                            rec->list_id, rec->source_addr,
                            rec->reg_offset, rec->value,
                            rec->context_count);
    if (pos >= buf_size) pos = buf_size - 1u;

    for (i = 0u; i < rec->context_count && pos + 12u < buf_size; ++i) {
        int n = snprintf(buf + pos, buf_size - pos,
                         " 0x%08x", rec->context[i]);
        if (n < 0 || (size_t)n >= buf_size - pos) break;
        pos += (size_t)n;
    }

    if (rec->truncated && pos + 12u < buf_size) {
        pos += (size_t)snprintf(buf + pos, buf_size - pos, " trunc=1");
    }

    if (pos >= buf_size) pos = buf_size - 1u;
    buf[pos] = '\0';
    return pos;
}
