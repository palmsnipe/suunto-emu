/*
 * Ulsan MSPI1 native tail branches: descriptor identities, the
 * configuration write, lifecycle status reads (ticket 730, E-ULS-0033).
 *
 * Transcription of SapporoApollo4Mspi1.cs lines 690-845. The 2.44.52
 * descriptor shapes at 0x10029AD0 stay ported because the lane class
 * keeps them for every profile; on the 2.35.36 boot they are dormant
 * (that trace identifies through the main fall-through instead).
 */

#include "ulsan_mspi1_native.h"

#define NATIVE_RETURN_OFFSET 0x2Cu
#define DESCRIPTOR_244_ADDRESS 0x10029AD0u
#define READ_ID_NATIVE_RETURN 0x000F4543u
#define READ_QPI_ID_NATIVE_RETURN 0x000F4595u
#define QPI_ID_NATIVE_WORD 0x003925C2u

/* Lane BeginPersistenceProgramming (line 1258). */
static void begin_persistence_programming(ulsan_mspi1_state *s)
{
    if (s->next_write_address < ULSAN_MSPI1_PERSIST_BACKING_START ||
        s->next_write_address >= ULSAN_MSPI1_PERSIST_BACKING_END) {
        s->next_write_address = ULSAN_MSPI1_PERSIST_BACKING_START;
    }
    s->stage = ULSAN_MSPI1_PERSISTENCE_WRITE_STATUS_READY;
}

static int descriptor_244_read(ulsan_mspi1_state *s, uint32_t control,
                               uint32_t address, uint32_t count)
{
    return s->stage != ULSAN_MSPI1_INITIAL &&
           control == ULSAN_MSPI1_QUEUE_READ_CONTROL && count == 3u &&
           address == DESCRIPTOR_244_ADDRESS &&
           ulsan_mspi1_guest_prefix_is(s, address, 0u, 1u, 0u, 1u) == 0;
}

int ulsan_mspi1_native_tail(ulsan_mspi1_state *s, uint32_t control,
                            uint32_t address, uint32_t count, int fixture)
{
    uint32_t native_return = 0u;

    (void)fixture;
    /* Line 690: 2.44.52 initial descriptor yields the identity. */
    if (descriptor_244_read(s, control, address, count) &&
        s->stage == ULSAN_MSPI1_INITIAL) {
        if (ulsan_mspi1_write_jedec(s, address) != 0) {
            return 0;
        }
        s->stage = ULSAN_MSPI1_AWAITING_ENTER_4BYTE;
        return 1;
    }
    /* Line 725: 2.44.52 QPI repeat at the same descriptor. */
    if (s->stage == ULSAN_MSPI1_AWAITING_QPI_IDENTIFICATION &&
        descriptor_244_read(s, control, address, count) &&
        ulsan_mspi1_guest_read_u32(s, address + NATIVE_RETURN_OFFSET,
                                   &native_return) == 0 &&
        native_return == QPI_ID_NATIVE_WORD) {
        if (ulsan_mspi1_write_jedec(s, address) != 0) {
            return 0;
        }
        s->stage = ULSAN_MSPI1_AWAITING_STATUS_READ;
        return 1;
    }
    /* Line 740: 2.44.52 final zero status at that descriptor. */
    if (s->stage == ULSAN_MSPI1_AWAITING_STATUS_READ &&
        descriptor_244_read(s, control, address, count) &&
        ulsan_mspi1_guest_read_u32(s, address + NATIVE_RETURN_OFFSET,
                                   &native_return) == 0 &&
        native_return == 0u) {
        s->stage = ULSAN_MSPI1_COMPLETE;
        return 1;
    }
    /* Line 755: 2.44.52 steady-state shape. */
    if (s->stage == ULSAN_MSPI1_COMPLETE &&
        descriptor_244_read(s, control, address, count) &&
        ulsan_mspi1_guest_read_u32(s, address + NATIVE_RETURN_OFFSET,
                                   &native_return) == 0 &&
        native_return == 0u) {
        return 1;
    }
    /* Line 771: one-byte configuration write retires the queue. */
    if (s->stage == ULSAN_MSPI1_AWAITING_CONFIGURATION_WRITE) {
        if (control != ULSAN_MSPI1_QUEUE_PROGRAM_CONTROL || count != 1u) {
            return 0;
        }
        s->stage = ULSAN_MSPI1_AWAITING_POST_CONFIGURATION_STATUS;
        return 1;
    }
    /* Line 791: zero status-register read. */
    if (ulsan_mspi1_native_is_lifecycle_status_read(s)) {
        if (control != ULSAN_MSPI1_QUEUE_READ_CONTROL || count != 1u) {
            return 0;
        }
        if (ulsan_mspi1_guest_write(s, address, 0u) != 0) {
            return 0;
        }
        switch (s->stage) {
        case ULSAN_MSPI1_AWAITING_POST_CONFIGURATION_STATUS:
            s->stage = ULSAN_MSPI1_INITIALIZATION_COMPLETE;
            break;
        case ULSAN_MSPI1_PERSISTENCE_CREATION_HEADER_READ:
            begin_persistence_programming(s);
            break;
        case ULSAN_MSPI1_AWAITING_PERSISTENCE_PROGRAM_STATUS:
        case ULSAN_MSPI1_AWAITING_PERSISTENCE_ERASE_STATUS:
        case ULSAN_MSPI1_PERSISTENCE_WRITE_STATUS_READY:
            s->stage = ULSAN_MSPI1_PERSISTENCE_WRITE_STATUS_READY;
            break;
        default:
            s->stage = ULSAN_MSPI1_COMPLETE;
            break;
        }
        return 1;
    }
    /* Line 818: main JEDEC fall-through. The native return word at
     * descriptor +0x2C selects the expected value and the three bytes
     * at the destination must still be zero. */
    if (count == 3u &&
        ulsan_mspi1_guest_read_u32(s, address + NATIVE_RETURN_OFFSET,
                                   &native_return) == 0 &&
        native_return == (s->stage == ULSAN_MSPI1_INITIAL
                              ? READ_ID_NATIVE_RETURN
                              : READ_QPI_ID_NATIVE_RETURN)) {
        unsigned byte_index;
        int buffer_empty = 1;
        for (byte_index = 0u; byte_index < 3u; byte_index++) {
            uint8_t byte = 0u;
            if (ulsan_mspi1_guest_read(s, address + byte_index, &byte) != 0) {
                return 0;
            }
            if (byte != 0u) {
                buffer_empty = 0; /* lane: refuse over non-empty buffer */
            }
        }
        if (buffer_empty == 0) {
            return 0;
        }
        if (ulsan_mspi1_write_jedec(s, address) != 0) {
            return 0;
        }
        s->stage = s->stage == ULSAN_MSPI1_INITIAL
            ? ULSAN_MSPI1_AWAITING_ENTER_4BYTE
            : ULSAN_MSPI1_AWAITING_STATUS_READ;
        return 1;
    }
    return 0;
}
