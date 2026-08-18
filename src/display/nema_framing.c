/*
 * Nema ring and command-list framing parser (ticket 500).
 * Decodes verified ring/wrap/child-list framing into atomic
 * register-write records.  All records are staged before emission;
 * malformed input produces REFUSE with zero callbacks.
 */

#include "nema_framing.h"

#include <stdlib.h>
#include <string.h>

static semu_status read_word(semu_bus *bus, uint32_t addr, uint32_t *out,
                              semu_error *error)
{
    return semu_bus_read(bus, addr, 4u, out, error);
}

static semu_status scan_and_stage(semu_bus *bus,
                                   uint32_t ring_base, uint32_t ring_words,
                                   uint32_t old_word, uint32_t new_word,
                                   nema_record *records, size_t *record_count,
                                   uint32_t *child_infos,
                                   size_t *child_count,
                                   semu_error *error)
{
    size_t rc = 0u;
    size_t cc = 0u;
    uint32_t submitted;
    uint32_t i;

    if (old_word == new_word) {
        *record_count = 0u;
        *child_count = 0u;
        return SEMU_OK;
    }
    submitted = (new_word - old_word) % ring_words;
    if (submitted == 0u) submitted = ring_words;

    for (i = 0u; i < submitted; ++i) {
        uint32_t idx = (old_word + i) % ring_words;
        uint32_t w0, w2;
        uint32_t child_addr, child_entries, j;
        semu_status st;

        st = read_word(bus, ring_base + idx * 4u, &w0, error);
        if (st != SEMU_OK) return st;

        if ((w0 & 0xFFFFFF00u) == NEMA_CL_NOP) {
            continue;
        }
        if (w0 == (NEMA_HOLDCMD | NEMA_REG_CMDADDR)) {
            if (i + 3u >= submitted) {
                semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                               "nema: truncated wrap trailer");
                return SEMU_ERR_UNSUPPORTED;
            }
            i += 3u;
            continue;
        }
        if ((w0 & 0xFF000000u) == NEMA_HOLDCMD) {
            continue;
        }
        if (w0 != NEMA_REG_CMDADDR) {
            continue;
        }
        if (i + 3u >= submitted) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "nema: truncated child submission");
            return SEMU_ERR_UNSUPPORTED;
        }
        idx = (old_word + i + 1u) % ring_words;
        st = read_word(bus, ring_base + idx * 4u, &child_addr, error);
        if (st != SEMU_OK) return st;
        idx = (old_word + i + 2u) % ring_words;
        st = read_word(bus, ring_base + idx * 4u, &w2, error);
        if (st != SEMU_OK) return st;
        idx = (old_word + i + 3u) % ring_words;
        st = read_word(bus, ring_base + idx * 4u, &child_entries, error);
        if (st != SEMU_OK) return st;

        if ((w2 & 0xFFFF0000u) != NEMA_CL_PUSH ||
            (w2 & 0x0000FFFFu) != NEMA_REG_CMDSIZE) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "nema: bad CL_PUSH|CMDSIZE at ring word %u", i + 2u);
            return SEMU_ERR_UNSUPPORTED;
        }
        if ((child_addr & 3u) != 0u) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "nema: child address 0x%08x not aligned",
                           child_addr);
            return SEMU_ERR_UNSUPPORTED;
        }
        if (child_entries == 0u || child_entries > NEMA_MAX_LIST_WORDS ||
            child_entries > (UINT32_MAX - child_addr) / 4u) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "nema: child entries %u out of range",
                           child_entries);
            return SEMU_ERR_UNSUPPORTED;
        }
        i += 3u;

        if (cc >= 32u) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "nema: too many child lists");
            return SEMU_ERR_UNSUPPORTED;
        }
        child_infos[cc * 2u] = child_addr;
        child_infos[cc * 2u + 1u] = child_entries;
        ++cc;

        for (j = 0u; j < child_entries; j += 2u) {
            uint32_t reg_addr = child_addr + j * 4u;
            uint32_t reg_word, val_word;
            uint8_t prefix;

            st = read_word(bus, reg_addr, &reg_word, error);
            if (st != SEMU_OK) return st;

            prefix = (uint8_t)(reg_word >> 24u);
            if (prefix != 0x00u && prefix != 0xFFu) {
                semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                               "nema: bad prefix 0x%02x at child 0x%08x word %u",
                               prefix, child_addr, j);
                return SEMU_ERR_UNSUPPORTED;
            }
            if ((reg_word & 3u) != 0u) {
                semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                               "nema: register 0x%06x not aligned",
                               reg_word & 0x00FFFFFFu);
                return SEMU_ERR_UNSUPPORTED;
            }
            if (j + 1u >= child_entries) {
                if (prefix == NEMA_HOLDCMD >> 24u) {
                    break;
                }
                continue;
            }
            st = read_word(bus, reg_addr + 4u, &val_word, error);
            if (st != SEMU_OK) return st;

            if (rc >= NEMA_MAX_RECORDS) {
                semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                               "nema: record overflow");
                return SEMU_ERR_UNSUPPORTED;
            }
            records[rc].prefix = prefix;
            records[rc].reg_offset = reg_word & 0x00FFFFFFu;
            records[rc].value = val_word;
            records[rc].source_addr = reg_addr + 4u;
            ++rc;
        }
    }

    *record_count = rc;
    *child_count = cc;
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status nema_framing_parse(
    semu_bus *bus,
    uint32_t ring_base, uint32_t ring_words,
    uint32_t old_word, uint32_t new_word,
    nema_child_fn on_child, void *child_context,
    nema_record_fn on_record, void *record_context,
    semu_error *error)
{
    nema_record *records;
    uint32_t child_infos[64u];
    size_t record_count, child_count, i;
    semu_status st;

    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "nema: null bus");
        return SEMU_ERR_ARGUMENT;
    }
    if ((ring_base & 3u) != 0u || ring_words == 0u ||
        old_word >= ring_words || new_word >= ring_words) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "nema: invalid ring arguments");
        return SEMU_ERR_ARGUMENT;
    }

    records = (nema_record *)calloc(NEMA_MAX_RECORDS, sizeof(*records));
    if (records == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "nema: cannot allocate records");
        return SEMU_ERR_NOMEM;
    }

    st = scan_and_stage(bus, ring_base, ring_words, old_word, new_word,
                        records, &record_count, child_infos, &child_count,
                        error);
    if (st != SEMU_OK) {
        free(records);
        return st;
    }

    for (i = 0u; i < child_count; ++i) {
        if (on_child != NULL) {
            on_child(child_context, child_infos[i * 2u],
                     child_infos[i * 2u + 1u]);
        }
    }
    for (i = 0u; i < record_count; ++i) {
        if (on_record != NULL) {
            on_record(record_context, &records[i]);
        }
    }

    free(records);
    return SEMU_OK;
}
