#ifndef SEMU_NEMA_FRAMING_H
#define SEMU_NEMA_FRAMING_H

#include "semu/bus.h"
#include "semu/types.h"

/*
 * Nema ring and command-list framing parser (ticket 500).
 * Decodes verified ring/wrap/child-list framing into atomic
 * register-write records.  No render/state/texture semantics.
 *
 * Evidence: E-NEMA-RING-001 (bootstrap, wrap trailer, no IRQ),
 *           E-NEMA-LISTS-001 (complete suffix/list sizes).
 */

#define NEMA_MAX_LIST_WORDS 4096u
#define NEMA_MAX_RECORDS   4096u

#define NEMA_REG_CMDADDR   0xF0u
#define NEMA_REG_CMDSIZE   0xF4u
#define NEMA_REG_INTERRUPT 0xF8u
#define NEMA_REG_CLID      0x148u

#define NEMA_CL_PUSH       0x00020000u
#define NEMA_CL_NOP        0x00010000u
#define NEMA_HOLDCMD       0xFF000000u

typedef struct {
    uint8_t  prefix;
    uint32_t reg_offset;
    uint32_t value;
    uint32_t source_addr;
} nema_record;

typedef void (*nema_record_fn)(void *context, const nema_record *record);
typedef void (*nema_child_fn)(void *context, uint32_t child_address,
                               uint32_t child_entries);

/*
 * Parses the ring between old_word and new_word (wrapping), finds
 * child-list submissions, and decodes register/value pairs from each
 * child list.  CL_PUSH|CMDSIZE is a 32-bit entry count.  All records are
 * staged before any callback is invoked; on malformed input, returns
 * SEMU_ERR_UNSUPPORTED without callbacks.
 *
 * ring_base:   SRAM address of ring start (must be 4-byte aligned).
 * ring_words:  ring capacity in 32-bit words (must be > 0).
 * old_word:    previous write index (word offset from ring_base).
 * new_word:    current write index (word offset from ring_base).
 */
semu_status nema_framing_parse(
    semu_bus *bus,
    uint32_t ring_base, uint32_t ring_words,
    uint32_t old_word, uint32_t new_word,
    nema_child_fn on_child, void *child_context,
    nema_record_fn on_record, void *record_context,
    semu_error *error);

#endif
