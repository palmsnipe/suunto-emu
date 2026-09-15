/*
 * Ulsan MSPI1 product PIO handler (ticket 730, E-ULS-0033).
 *
 * Transcription of HandleProductRegisterWrite (SapporoApollo4Mspi1.cs
 * lines 928-1075). Called after every plain register store, exactly as
 * the lane base class does; only PIO-control start values react. The
 * lane's refusal WARNINGs carry no guest-visible mutation, so the
 * refusal shapes here are simply "no state change, no interrupt".
 */

#include "ulsan_mspi1_native.h"

#define PIO_CONTROL_SLOT 4u /* registers[4] holds +0x0, s->pio_command
                                holds +0xC, registers[5] holds +0x8    */

static int payload_free_accepted(ulsan_mspi1_state *s, uint32_t command)
{
    return (s->stage == ULSAN_MSPI1_AWAITING_ENTER_4BYTE &&
            command == ULSAN_MSPI1_CMD_ENTER_4BYTE) ||
           (s->stage == ULSAN_MSPI1_AWAITING_ENTER_QPI &&
            command == ULSAN_MSPI1_CMD_ENTER_QPI) ||
           (s->stage == ULSAN_MSPI1_COMPLETE &&
            command == ULSAN_MSPI1_CMD_WRITE_ENABLE) ||
           (s->stage == ULSAN_MSPI1_PERSISTENCE_WRITE_STATUS_READY &&
            command == ULSAN_MSPI1_CMD_WRITE_ENABLE) ||
           (s->stage == ULSAN_MSPI1_AWAITING_PERSISTENCE_PROGRAM &&
            command == ULSAN_MSPI1_CMD_WRITE_ENABLE) ||
           (s->manufacturing_page_observed != 0u &&
            command == ULSAN_MSPI1_CMD_ERASE_RESUME);
}

/* Lane IsProvenNativeEraseAddress. */
static int is_proven_erase_address(uint32_t address)
{
    return (address == 0x01590000u || address == 0x01600000u ||
            address >= ULSAN_MSPI1_AUTHENTIC_OTA_START) &&
           (address & (ULSAN_MSPI1_BLOCK_ERASE_LENGTH - 1u)) == 0u &&
           (uint64_t)address + ULSAN_MSPI1_BLOCK_ERASE_LENGTH <=
               (uint64_t)ULSAN_MSPI1_AUTHENTIC_OTA_END;
}

/* Lane IsProvenNativeSectorEraseAddress. */
static int is_proven_sector_erase_address(uint32_t address)
{
    int in_synthetic_persistence =
        address >= ULSAN_MSPI1_PERSIST_BACKING_START &&
        (uint64_t)address + ULSAN_MSPI1_SECTOR_LENGTH <=
            (uint64_t)ULSAN_MSPI1_PERSIST_BACKING_END;
    return (address == 0x01FB0000u || in_synthetic_persistence) &&
           (address & (ULSAN_MSPI1_SECTOR_LENGTH - 1u)) == 0u;
}

uint32_t ulsan_mspi1_native_pio_write(ulsan_mspi1_state *s,
                                      uint32_t offset, uint32_t value)
{
    uint32_t command = s->pio_command & 0xFFu;
    uint32_t erase_address;
    int pio_started;
    int addressed_started;

    if (offset != 0u) {
        return 0u; /* lane reacts to PIO-control writes only */
    }
    pio_started = value == ULSAN_MSPI1_PIO_START_CONTROL;
    addressed_started = value == ULSAN_MSPI1_PIO_ADDRESSED_START_CONTROL;
    erase_address = s->registers[5];

    if (pio_started && payload_free_accepted(s, command)) {
        if (command == ULSAN_MSPI1_CMD_WRITE_ENABLE) {
            s->calibration_reads_remaining = 0;
        }
        if (s->stage == ULSAN_MSPI1_AWAITING_ENTER_4BYTE) {
            s->stage = ULSAN_MSPI1_AWAITING_ENTER_QPI;
        } else if (s->stage == ULSAN_MSPI1_AWAITING_ENTER_QPI) {
            s->stage = ULSAN_MSPI1_AWAITING_QPI_IDENTIFICATION;
        } else if (s->stage == ULSAN_MSPI1_COMPLETE) {
            s->stage = ULSAN_MSPI1_AWAITING_CONFIGURATION_WRITE;
        } else if (s->stage == ULSAN_MSPI1_PERSISTENCE_WRITE_STATUS_READY) {
            s->stage = ULSAN_MSPI1_AWAITING_PERSISTENCE_PROGRAM;
        } else if (s->stage == ULSAN_MSPI1_AWAITING_PERSISTENCE_PROGRAM ||
                   s->manufacturing_page_observed != 0u) {
            /* lane keeps the stage for these two shapes */
        } else {
            s->stage = ULSAN_MSPI1_COMPLETE;
        }
        return ULSAN_MSPI1_INT_PIO_COMPLETE;
    }
    if (addressed_started &&
        s->stage == ULSAN_MSPI1_AWAITING_PERSISTENCE_PROGRAM &&
        command == ULSAN_MSPI1_CMD_BLOCK_ERASE_64K &&
        is_proven_erase_address(erase_address)) {
        if (ulsan_mspi1_erase_xip(s, erase_address,
                                  ULSAN_MSPI1_BLOCK_ERASE_LENGTH) != 0) {
            return 0u; /* refuse without mutation on a bus fault */
        }
        s->active_erase_address = erase_address;
        s->active_erase_length = ULSAN_MSPI1_BLOCK_ERASE_LENGTH;
        s->stage = ULSAN_MSPI1_AWAITING_PERSISTENCE_ERASE_STATUS;
        return ULSAN_MSPI1_INT_PIO_COMPLETE;
    }
    if (addressed_started &&
        s->stage == ULSAN_MSPI1_AWAITING_PERSISTENCE_PROGRAM &&
        command == ULSAN_MSPI1_CMD_SECTOR_ERASE_4K &&
        is_proven_sector_erase_address(erase_address)) {
        if (ulsan_mspi1_erase_xip(s, erase_address,
                                  ULSAN_MSPI1_SECTOR_LENGTH) != 0) {
            return 0u;
        }
        s->active_erase_address = erase_address;
        s->active_erase_length = ULSAN_MSPI1_SECTOR_LENGTH;
        if (erase_address >= ULSAN_MSPI1_PERSIST_BACKING_START &&
            erase_address < ULSAN_MSPI1_PERSIST_BACKING_END) {
            s->next_write_address = erase_address;
        }
        s->stage = ULSAN_MSPI1_AWAITING_PERSISTENCE_ERASE_STATUS;
        return ULSAN_MSPI1_INT_PIO_COMPLETE;
    }
    return 0u; /* lane logs refusal warnings; nothing else changes */
}
