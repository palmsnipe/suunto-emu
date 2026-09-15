/*
 * Ulsan MSPI1 native response machine, main branch chain (ticket 730,
 * E-ULS-0033).
 *
 * Transcription of the lane plugin TryPopulateObservedCommandQueueResponse
 * (SapporoApollo4Mspi1.cs lines 234-668) in exact class order so the
 * first matching branch always agrees with the lane; the tail branches
 * (lines 690-845) live in ulsan_mspi1_identity.c. Every byte written
 * into the guest buffer, every backing-store program, and every stage
 * transition mirrors the cited lane code; unmatched requests refuse
 * with no state mutation.
 */

#include "ulsan_mspi1_native.h"

#define NATIVE_RETURN_OFFSET 0x2Cu /* lane descriptor +0x2C            */

static int header_is_erased(ulsan_mspi1_state *s, uint32_t device,
                            uint32_t count, int *erased)
{
    uint32_t index;
    *erased = 1;
    for (index = 0u; index < count; index++) {
        uint8_t byte;
        if (ulsan_mspi1_xip_read(s, device + index, &byte) != 0) {
            return -1;
        }
        if (byte != ULSAN_MSPI1_ERASED_BYTE) {
            *erased = 0;
        }
    }
    return 0;
}

int ulsan_mspi1_native_try_populate(ulsan_mspi1_state *s, uint32_t control,
                                    uint32_t address, uint32_t count)
{
    uint32_t device = s->registers[2]; /* queue device/flash address    */
    int fixture;

    if (count > 0x1000u) {
        return 0;
    }
    fixture = ulsan_mspi1_has_manufacturing_fixture(s);

    /* Line 234: authentic OTA read (stage independent). */
    if (control == ULSAN_MSPI1_QUEUE_READ_CONTROL &&
        ulsan_mspi1_is_authentic_ota_read(device, count)) {
        return ulsan_mspi1_copy_xip_to_guest(s, address, device, count) == 0
            ? 1 : 0;
    }
    /* Line 251: missing low-flash 8-byte record (fixture only). */
    if (control == ULSAN_MSPI1_QUEUE_READ_CONTROL &&
        count == ULSAN_MSPI1_MISSING_LOW_FLASH_LENGTH &&
        device == ULSAN_MSPI1_MISSING_LOW_FLASH_OFFSET && fixture) {
        return ulsan_mspi1_fill_guest(s, address, count,
                                      ULSAN_MSPI1_ERASED_BYTE) == 0 ? 1 : 0;
    }
    /* Line 273: post-footer creation header at the fuel-gauge probe. */
    if (s->stage == ULSAN_MSPI1_FIRMWARE_FOOTER_READ_COMPLETE &&
        control == ULSAN_MSPI1_QUEUE_READ_CONTROL &&
        count == ULSAN_MSPI1_HEADER_PROBE_LENGTH &&
        device == ULSAN_MSPI1_FOOTER_GAUGE_PROBE_OFFSET) {
        if (ulsan_mspi1_copy_xip_to_guest(s, address, device, count) != 0) {
            return 0;
        }
        s->next_write_address = ULSAN_MSPI1_FOOTER_GAUGE_PROBE_OFFSET;
        s->stage = ULSAN_MSPI1_PERSISTENCE_CREATION_HEADER_READ;
        return 1;
    }
    /* Line 287: erased config_nugget slot after a manufacturing page. */
    if (s->manufacturing_page_observed != 0u &&
        control == ULSAN_MSPI1_QUEUE_READ_CONTROL &&
        count == ULSAN_MSPI1_CONFIG_NUGGET_LENGTH &&
        device >= ULSAN_MSPI1_CONFIG_NUGGET_SECTOR &&
        device + count <= ULSAN_MSPI1_CONFIG_NUGGET_SECTOR +
            ULSAN_MSPI1_CONFIG_NUGGET_SECTOR_LENGTH &&
        (device - ULSAN_MSPI1_CONFIG_NUGGET_SECTOR) %
            ULSAN_MSPI1_CONFIG_NUGGET_LENGTH == 0u) {
        return ulsan_mspi1_fill_guest(s, address, count,
                                      ULSAN_MSPI1_ERASED_BYTE) == 0 ? 1 : 0;
    }
    /* Line 302: bounded calibration MXIC ready reads (fixture only). */
    if (s->calibration_reads_remaining > 0 &&
        !ulsan_mspi1_native_is_lifecycle_status_read(s) &&
        control == ULSAN_MSPI1_QUEUE_READ_CONTROL && count == 1u &&
        device == 0u && fixture) {
        if (ulsan_mspi1_guest_write(s, address, 0u) != 0) {
            return 0;
        }
        s->calibration_reads_remaining--;
        return 1;
    }
    /* Line 327: early synthetic manufacturing page (fixture only). */
    if (control == ULSAN_MSPI1_QUEUE_READ_CONTROL &&
        count == ULSAN_MSPI1_MANUFACTURING_PAGE_LENGTH && fixture &&
        ulsan_mspi1_is_observed_manufacturing_page(s, device)) {
        if (ulsan_mspi1_copy_xip_to_guest(s, address, device, count) != 0) {
            return 0;
        }
        s->manufacturing_page_observed = 1u;
        s->calibration_reads_remaining = 2;
        return 1;
    }
    /* Line 348: manufacturing page or its erased placeholder. */
    if ((s->stage == ULSAN_MSPI1_FIRMWARE_FOOTER_READ_COMPLETE ||
         s->stage == ULSAN_MSPI1_PERSISTENCE_WRITE_STATUS_READY ||
         s->manufacturing_page_observed != 0u) &&
        control == ULSAN_MSPI1_QUEUE_READ_CONTROL &&
        count == ULSAN_MSPI1_MANUFACTURING_PAGE_LENGTH &&
        ulsan_mspi1_is_observed_manufacturing_page(s, device)) {
        if (fixture) {
            if (ulsan_mspi1_copy_xip_to_guest(s, address, device, count) !=
                0) {
                return 0;
            }
        } else if (ulsan_mspi1_fill_guest(s, address, count,
                     ULSAN_MSPI1_ERASED_BYTE) != 0) {
            return 0;
        }
        s->manufacturing_page_observed = 1u;
        s->calibration_reads_remaining = 2;
        return 1;
    }
    /* Line 379: authentic 36-byte firmware footer. */
    if (s->stage == ULSAN_MSPI1_PERSISTENCE_WRITE_STATUS_READY &&
        control == ULSAN_MSPI1_QUEUE_READ_CONTROL &&
        count == ULSAN_MSPI1_FOOTER_LENGTH &&
        device == ULSAN_MSPI1_FOOTER_OFFSET) {
        if (ulsan_mspi1_copy_xip_to_guest(s, address,
                ULSAN_MSPI1_FOOTER_OFFSET, count) != 0) {
            return 0;
        }
        s->stage = ULSAN_MSPI1_FIRMWARE_FOOTER_READ_COMPLETE;
        return 1;
    }
    /* Line 396: the 64-byte persistence record program. */
    if ((s->stage == ULSAN_MSPI1_AWAITING_PERSISTENCE_PROGRAM ||
         s->stage == ULSAN_MSPI1_PERSISTENCE_WRITE_STATUS_READY) &&
        control == ULSAN_MSPI1_QUEUE_PROGRAM_CONTROL &&
        count == ULSAN_MSPI1_RECORD_LENGTH) {
        if (device != s->next_write_address ||
            s->next_write_address + count >
                ULSAN_MSPI1_PERSIST_BACKING_END) {
            return 0; /* lane logs a program mismatch and refuses */
        }
        if (ulsan_mspi1_copy_guest_to_xip(s, address, s->next_write_address,
                                          count) != 0) {
            return 0;
        }
        s->next_write_address += count;
        s->stage = ULSAN_MSPI1_AWAITING_PERSISTENCE_PROGRAM_STATUS;
        return 1;
    }
    /* Line 428: page-program fragment inside a proven erase. */
    if (s->stage == ULSAN_MSPI1_AWAITING_PERSISTENCE_PROGRAM &&
        control == ULSAN_MSPI1_QUEUE_PROGRAM_CONTROL &&
        ulsan_mspi1_proven_flash_program(s, address, device, count) == 1) {
        if (ulsan_mspi1_copy_guest_to_xip(s, address, device, count) != 0) {
            return 0;
        }
        s->stage = ULSAN_MSPI1_AWAITING_PERSISTENCE_PROGRAM_STATUS;
        return 1;
    }
    /* Line 448: post-scan A5 sentinel replaced by the QPI identity. */
    if (s->stage == ULSAN_MSPI1_MISSING_LOW_FLASH_SCAN_COMPLETE) {
        unsigned index;
        if (control != ULSAN_MSPI1_QUEUE_READ_CONTROL || count != 3u) {
            return 0;
        }
        for (index = 0u; index < 3u; index++) {
            uint8_t byte;
            if (ulsan_mspi1_guest_read(s, address + index, &byte) != 0) {
                return 0;
            }
            if (byte != ULSAN_MSPI1_SENTINEL_BYTE) {
                return 0;
            }
        }
        if (ulsan_mspi1_write_jedec(s, address) != 0) {
            return 0;
        }
        s->stage = ULSAN_MSPI1_POST_SCAN_IDENTIFICATION_COMPLETE;
        return 1;
    }
    /* Line 475: post-initialization scan entry from the blank sector. */
    if (s->stage == ULSAN_MSPI1_INITIALIZATION_COMPLETE ||
        s->stage == ULSAN_MSPI1_POST_SCAN_IDENTIFICATION_COMPLETE) {
        int post_scan =
            s->stage == ULSAN_MSPI1_POST_SCAN_IDENTIFICATION_COMPLETE;
        if (post_scan &&
            control == ULSAN_MSPI1_QUEUE_READ_CONTROL &&
            count == ULSAN_MSPI1_HEADER_PROBE_LENGTH &&
            s->next_write_address >= ULSAN_MSPI1_PERSIST_BACKING_START &&
            s->next_write_address < ULSAN_MSPI1_PERSIST_BACKING_END &&
            device == s->next_write_address) {
            if (ulsan_mspi1_copy_xip_to_guest(s, address,
                    s->next_write_address, count) != 0) {
                return 0;
            }
            s->stage = ULSAN_MSPI1_PERSISTENCE_CREATION_HEADER_READ;
            return 1;
        }
        if (control == ULSAN_MSPI1_QUEUE_READ_CONTROL &&
            count == ULSAN_MSPI1_HEADER_PROBE_LENGTH &&
            device == ULSAN_MSPI1_PERSIST_BACKING_START) {
            ulsan_mspi1_initialize_synthetic_range(s);
            s->next_read_address = ULSAN_MSPI1_PERSIST_BACKING_START;
            if (ulsan_mspi1_copy_xip_to_guest(s, address,
                    s->next_read_address, count) != 0) {
                return 0;
            }
            s->stage = post_scan
                ? ULSAN_MSPI1_PERSISTENCE_CREATION_HEADER_READ
                : ULSAN_MSPI1_PERSISTENCE_HEADER_READ;
            return 1;
        }
        return 0;
    }
    /* Line 524: the full 64-byte record read after a sector header. */
    if (s->stage == ULSAN_MSPI1_PERSISTENCE_HEADER_READ) {
        if (control != ULSAN_MSPI1_QUEUE_READ_CONTROL ||
            count != ULSAN_MSPI1_RECORD_LENGTH ||
            device != s->next_read_address) {
            return 0;
        }
        if (ulsan_mspi1_copy_xip_to_guest(s, address, s->next_read_address,
                                          count) != 0) {
            return 0;
        }
        s->next_read_address += count;
        s->stage = ULSAN_MSPI1_PERSISTENCE_SECTOR_SCAN;
        return 1;
    }
    /* Line 537: sector scan: validation reread, header probe, next
     * full record. */
    if (s->stage == ULSAN_MSPI1_PERSISTENCE_SECTOR_SCAN) {
        uint32_t header_address;
        int erased = 0;
        if (control == ULSAN_MSPI1_QUEUE_READ_CONTROL &&
            count == ULSAN_MSPI1_RECORD_LENGTH &&
            s->next_read_address >=
                ULSAN_MSPI1_PERSIST_BACKING_START + count &&
            device == s->next_read_address - count) {
            return ulsan_mspi1_copy_xip_to_guest(s, address,
                       s->next_read_address - count, count) == 0 ? 1 : 0;
        }
        if (control == ULSAN_MSPI1_QUEUE_READ_CONTROL &&
            count == ULSAN_MSPI1_HEADER_PROBE_LENGTH &&
            device == s->next_read_address) {
            if (ulsan_mspi1_copy_xip_to_guest(s, address,
                    s->next_read_address, count) != 0) {
                return 0;
            }
            header_address = s->next_read_address;
            s->next_read_address += ULSAN_MSPI1_RECORD_LENGTH;
            if (header_is_erased(s, header_address, count, &erased) != 0) {
                return 0;
            }
            if (erased != 0) {
                s->next_write_address = header_address;
                if (header_address % ULSAN_MSPI1_SECTOR_LENGTH == 0u) {
                    s->next_read_address = header_address;
                    s->stage = ULSAN_MSPI1_PERSISTENCE_HEADER_READ;
                } else {
                    s->stage =
                        ULSAN_MSPI1_MISSING_LOW_FLASH_SCAN_COMPLETE;
                }
            }
            return 1;
        }
        if (control == ULSAN_MSPI1_QUEUE_READ_CONTROL &&
            count == ULSAN_MSPI1_RECORD_LENGTH &&
            device == s->next_read_address &&
            s->next_read_address + count <=
                ULSAN_MSPI1_PERSIST_BACKING_END) {
            if (ulsan_mspi1_copy_xip_to_guest(s, address,
                    s->next_read_address, count) != 0) {
                return 0;
            }
            s->next_read_address += count;
            if (s->next_read_address % ULSAN_MSPI1_SECTOR_LENGTH == 0u) {
                s->stage = s->next_read_address ==
                            ULSAN_MSPI1_PERSIST_BACKING_END
                    ? ULSAN_MSPI1_MISSING_LOW_FLASH_SCAN_COMPLETE
                    : ULSAN_MSPI1_PERSISTENCE_SECTOR_SCAN_COMPLETE;
            }
            return 1;
        }
        return 0;
    }
    /* Line 622: next-sector header after a completed sector scan. */
    if (s->stage == ULSAN_MSPI1_PERSISTENCE_SECTOR_SCAN_COMPLETE) {
        if (control != ULSAN_MSPI1_QUEUE_READ_CONTROL ||
            count != ULSAN_MSPI1_HEADER_PROBE_LENGTH ||
            device != s->next_read_address ||
            s->next_read_address >= ULSAN_MSPI1_PERSIST_BACKING_END) {
            return 0;
        }
        if (ulsan_mspi1_copy_xip_to_guest(s, address, s->next_read_address,
                                          count) != 0) {
            return 0;
        }
        s->stage = ULSAN_MSPI1_PERSISTENCE_HEADER_READ;
        return 1;
    }
    /* Line 637: creation-header probe for the append cursor. */
    if ((s->manufacturing_page_observed != 0u ||
         s->stage == ULSAN_MSPI1_PERSISTENCE_WRITE_STATUS_READY) &&
        control == ULSAN_MSPI1_QUEUE_READ_CONTROL &&
        count == ULSAN_MSPI1_HEADER_PROBE_LENGTH &&
        device == s->next_write_address) {
        if (ulsan_mspi1_copy_xip_to_guest(s, address, s->next_write_address,
                                          count) != 0) {
            return 0;
        }
        s->stage = ULSAN_MSPI1_PERSISTENCE_CREATION_HEADER_READ;
        return 1;
    }
    /* Line 655: retained synthetic persistence random reads. */
    if (s->synthetic_initialized != 0u &&
        control == ULSAN_MSPI1_QUEUE_READ_CONTROL &&
        (count == ULSAN_MSPI1_HEADER_PROBE_LENGTH ||
         count == ULSAN_MSPI1_RECORD_LENGTH) &&
        device >= ULSAN_MSPI1_PERSIST_BACKING_START &&
        device + count <= ULSAN_MSPI1_PERSIST_BACKING_END) {
        if (ulsan_mspi1_copy_xip_to_guest(s, address, device, count) != 0) {
            return 0;
        }
        if (count == ULSAN_MSPI1_HEADER_PROBE_LENGTH &&
            device == s->next_write_address && fixture) {
            s->calibration_reads_remaining = 2;
        }
        return 1;
    }
    return ulsan_mspi1_native_tail(s, control, address, count, fixture);
}
