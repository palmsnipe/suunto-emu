/* Validated ring plans and callback-compatible child framing (ticket 761). */
#include "nema_framing.h"
#include "nema_state.h"
#include <stdlib.h>
#include <string.h>

semu_status nema_command_read_word(semu_bus *bus, uint32_t address,
    uint32_t *value, semu_error *error)
{
    uint8_t bytes[4]; semu_status status;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "nema: null command word output");
        return SEMU_ERR_ARGUMENT;
    }
    status = semu_bus_copy_out(bus, address, bytes, sizeof(bytes), error);
    if (status != SEMU_OK) return status;
    *value = (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8u |
        (uint32_t)bytes[2] << 16u | (uint32_t)bytes[3] << 24u;
    return SEMU_OK;
}

static semu_status unsupported(semu_error *error, const char *text)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "nema: %s", text);
    return SEMU_ERR_UNSUPPORTED;
}

static semu_status append_list(nema_ring_plan *p, uint32_t address,
    uint32_t words, uint32_t flags, semu_error *error)
{
    if (flags != 0u && p->list_count != 0u) {
        semu_display_list *last = &p->lists[p->list_count - 1u];
        if (last->flags == flags && (uint64_t)last->address +
            (uint64_t)last->word_count * 4u == address &&
            words <= NEMA_MAX_LIST_WORDS - last->word_count) {
            last->word_count += words;
            return SEMU_OK;
        }
    }
    if (p->list_count == SEMU_DISPLAY_MAX_LISTS)
        return unsupported(error, "too many command spans");
    p->lists[p->list_count++] = (semu_display_list){address, words, flags};
    return SEMU_OK;
}

static semu_status stage_child(semu_bus *bus, uint32_t address, uint32_t words,
    nema_record *records, size_t *count, semu_error *error)
{
    uint32_t j;
    for (j = 0u; j < words; j += 2u) {
        uint32_t reg, value, at = address + j * 4u;
        semu_status st = nema_command_read_word(bus, at, &reg, error);
        if (st != SEMU_OK) return st;
        if ((reg >> 24u) != 0u && (reg >> 24u) != 0xffu)
            return unsupported(error, "bad child prefix");
        if ((reg & 3u) != 0u)
            return unsupported(error, "child register not aligned");
        st = nema_command_read_word(bus, at + 4u, &value, error);
        if (st != SEMU_OK) return st;
        if (*count == NEMA_MAX_RECORDS) return unsupported(error, "record overflow");
        records[(*count)++] = (nema_record){
            (uint8_t)(reg >> 24u), reg & 0xffffffu, value, at + 4u};
    }
    return SEMU_OK;
}

static semu_status scan(semu_bus *bus, uint32_t base, uint32_t words,
    uint32_t old, uint32_t end, nema_ring_plan *p, nema_record *records,
    size_t *record_count, semu_error *error)
{
    uint32_t i, span = end >= old ? end - old : words - old + end;
    for (i = 0u; i < span;) {
        uint32_t idx = (old + i) % words, w[4], k, length = 2u;
        uint32_t address = base + idx * 4u;
        semu_status st = nema_command_read_word(bus, address, &w[0], error);
        if (st != SEMU_OK) return st;
        if (w[0] == NEMA_CL_NOP) { ++i; continue; }
        if (w[0] == NEMA_REG_CMDADDR || w[0] == NEMA_REG_CLID ||
            w[0] == (NEMA_HOLDCMD | NEMA_REG_CMDADDR)) length = 4u;
        else if (w[0] != NEMA_REG_INTERRUPT &&
            (((w[0] >> 24u) != 0u && (w[0] >> 24u) != 0xffu) ||
             !nema_state_register_supported(w[0] & 0xffffffu)))
            return unsupported(error, "unsupported inline register or padding");
        if (span - i < length) return unsupported(error, "truncated ring command");
        for (k = 1u; k < length; ++k) {
            st = nema_command_read_word(bus, base + ((old + i + k) % words) * 4u,
                &w[k], error);
            if (st != SEMU_OK) return st;
        }
        if (w[0] == NEMA_REG_CMDADDR) {
            if (w[2] != (NEMA_CL_PUSH | NEMA_REG_CMDSIZE))
                return unsupported(error, "bad CL_PUSH|CMDSIZE");
            if ((w[1] & 3u) != 0u) return unsupported(error, "child address not aligned");
            if (w[3] == 0u || (w[3] & 1u) != 0u || w[3] > NEMA_MAX_LIST_WORDS ||
                w[3] > (UINT32_MAX - w[1]) / 4u) {
                semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                    "nema: child entries %u out of range", w[3]);
                return SEMU_ERR_UNSUPPORTED;
            }
            if (p->child_count == NEMA_MAX_CHILDREN)
                return unsupported(error, "too many child lists");
            st = stage_child(bus, w[1], w[3], records, record_count, error);
            if (st != SEMU_OK) return st;
            st = append_list(p, w[1], w[3], 0u, error);
            if (st != SEMU_OK) return st;
            ++p->child_count;
        } else if (w[0] == NEMA_REG_CLID) {
            if (w[2] != NEMA_REG_INTERRUPT || w[3] != 1u)
                return unsupported(error, "invalid completion marker");
            if (p->marker_count == NEMA_MAX_MARKERS)
                return unsupported(error, "too many completion markers");
            p->markers[p->marker_count++] = w[1];
        } else if (w[0] == (NEMA_HOLDCMD | NEMA_REG_CMDADDR)) {
            if ((w[1] != base && w[1] != base + ((old + i + 4u) % words) * 4u) ||
                w[2] != (NEMA_HOLDCMD | NEMA_REG_CMDSIZE) || w[3] != words * 4u)
                return unsupported(error, "invalid held control");
        } else if (w[0] == NEMA_REG_INTERRUPT) {
            if (w[1] != 0u) return unsupported(error, "unpaired interrupt request");
            p->quiet = 1;
        } else {
            /* Native wrap trailers keep graphics pairs contiguous. Do not
             * invent a continuation value across an unobserved pair split. */
            if (idx + 1u == words)
                return unsupported(error, "inline pair crosses physical ring end");
            st = append_list(p, address, 2u, SEMU_DISPLAY_LIST_INLINE, error);
            if (st != SEMU_OK) return st;
        }
        i += length;
    }
    return SEMU_OK;
}

static semu_status parse(semu_bus *bus, uint32_t base, uint32_t words,
    uint32_t old, uint32_t end, nema_ring_plan *output,
    nema_child_fn on_child, void *child_context,
    nema_record_fn on_record, void *record_context, semu_error *error)
{
    nema_ring_plan plan = {0};
    nema_record *records;
    size_t count = 0u, i;
    semu_status st;
    if (bus == NULL || (base & 3u) != 0u || words == 0u ||
        words > UINT32_MAX / 4u ||
        (uint64_t)base + (uint64_t)words * 4u > UINT64_C(0x100000000) ||
        old >= words || end >= words) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "nema: invalid ring arguments");
        return SEMU_ERR_ARGUMENT;
    }
    records = calloc(NEMA_MAX_RECORDS, sizeof(*records));
    if (records == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "nema: cannot allocate records");
        return SEMU_ERR_NOMEM;
    }
    st = scan(bus, base, words, old, end, &plan, records, &count, error);
    if (st == SEMU_OK) {
        if (output != NULL) *output = plan;
        for (i = 0u; i < plan.list_count; ++i) {
            const semu_display_list *list = &plan.lists[i];
            if (list->flags == 0u && on_child != NULL)
                on_child(child_context, list->address, list->word_count);
        }
        for (i = 0u; i < count; ++i)
            if (on_record != NULL) on_record(record_context, &records[i]);
        semu_error_clear(error);
    }
    free(records);
    return st;
}

semu_status nema_framing_prepare(semu_bus *bus, uint32_t base, uint32_t words,
    uint32_t old, uint32_t end, nema_ring_plan *plan, semu_error *error)
{
    if (plan == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "nema: null ring plan");
        return SEMU_ERR_ARGUMENT;
    }
    return parse(bus, base, words, old, end, plan, NULL, NULL, NULL, NULL, error);
}

semu_status nema_framing_parse(semu_bus *bus, uint32_t base, uint32_t words,
    uint32_t old, uint32_t end, nema_child_fn on_child, void *child_context,
    nema_record_fn on_record, void *record_context, semu_error *error)
{
    return parse(bus, base, words, old, end, NULL, on_child, child_context,
        on_record, record_context, error);
}
