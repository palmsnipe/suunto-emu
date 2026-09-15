/*
 * Ulsan MSPI1 native support: XIP access, queue-count contract, product
 * reset (ticket 730, E-ULS-0033; lane SapporoApollo4Mspi1.cs helpers).
 *
 * The lane backs the whole 0x18000000 XIP aperture with a flash image;
 * the tree maps only the OTA half (xip_tail starts at exactly
 * AuthenticOtaStart) while the persistence range 0x00010000..0x00030000
 * is what the lane itself calls missing low flash served from its blank
 * sector fixture. This file routes accordingly: the persistence range
 * lives in the device-local backing store, everything else goes through
 * the bus and REFUSES on a fault.
 */

#include "ulsan_mspi1_native.h"

#include <string.h>

#define XIP_WINDOW_BASE 0x18000000u /* lane Mspi1XipBase */

static int in_backing(uint32_t device_address)
{
    return device_address >= ULSAN_MSPI1_PERSIST_BACKING_START &&
           device_address < ULSAN_MSPI1_PERSIST_BACKING_END;
}

int ulsan_mspi1_xip_read(ulsan_mspi1_state *s, uint32_t device_address,
                         uint8_t *out)
{
    if (in_backing(device_address)) {
        if (s->synthetic_initialized == 0u) {
            return -1; /* the lane initialises the range before reading */
        }
        *out = s->backing[device_address -
                          ULSAN_MSPI1_PERSIST_BACKING_START];
        return 0;
    }
    {
        semu_error error;
        uint32_t value = 0u;
        semu_error_clear(&error);
        if (semu_bus_read(s->bus, XIP_WINDOW_BASE + device_address, 1u,
                          &value, &error) != SEMU_OK) {
            return -1;
        }
        *out = (uint8_t)value;
        return 0;
    }
}

int ulsan_mspi1_xip_write(ulsan_mspi1_state *s, uint32_t device_address,
                          uint8_t value)
{
    if (in_backing(device_address)) {
        if (s->synthetic_initialized == 0u) {
            return -1;
        }
        s->backing[device_address - ULSAN_MSPI1_PERSIST_BACKING_START] =
            value;
        return 0;
    }
    {
        semu_error error;
        semu_error_clear(&error);
        if (semu_bus_write(s->bus, XIP_WINDOW_BASE + device_address, 1u,
                           value, &error) != SEMU_OK) {
            return -1;
        }
        return 0;
    }
}

/* Lane HasSyntheticManufacturingFixture: the 14-byte "ProductionData"
 * signature at the manufacturing page. On this board the low XIP
 * aperture is unmapped, so a bus fault is the same answer the lane's
 * blank backing gave: fixture absent (observed in trace as "erased
 * placeholder for absent manufacturing page"). */
int ulsan_mspi1_has_manufacturing_fixture(ulsan_mspi1_state *s)
{
    static const uint8_t signature[14] = {
        'P', 'r', 'o', 'd', 'u', 'c', 't',
        'i', 'o', 'n', 'D', 'a', 't', 'a'
    };
    unsigned index;
    for (index = 0u; index < 14u; index++) {
        uint8_t byte = 0u;
        if (ulsan_mspi1_xip_read(s,
                ULSAN_MSPI1_MANUFACTURING_PAGE_OFFSET + index, &byte) != 0) {
            return 0;
        }
        if (byte != signature[index]) {
            return 0;
        }
    }
    return 1;
}

static int is_config_nugget_address(uint32_t address)
{
    return address >= ULSAN_MSPI1_CONFIG_NUGGET_SECTOR &&
           address + ULSAN_MSPI1_CONFIG_NUGGET_LENGTH <=
               ULSAN_MSPI1_CONFIG_NUGGET_SECTOR +
                   ULSAN_MSPI1_CONFIG_NUGGET_SECTOR_LENGTH &&
           (address - ULSAN_MSPI1_CONFIG_NUGGET_SECTOR) %
                   ULSAN_MSPI1_CONFIG_NUGGET_LENGTH == 0u;
}

int ulsan_mspi1_is_observed_manufacturing_page(ulsan_mspi1_state *s,
                                               uint32_t address)
{
    if (ulsan_mspi1_has_manufacturing_fixture(s)) {
        return address >= ULSAN_MSPI1_MANUFACTURING_PAGE_OFFSET &&
               address + ULSAN_MSPI1_MANUFACTURING_PAGE_LENGTH <=
                   ULSAN_MSPI1_MANUFACTURING_PAGE_OFFSET +
                       ULSAN_MSPI1_SECTOR_LENGTH &&
               (address - ULSAN_MSPI1_MANUFACTURING_PAGE_OFFSET) %
                       ULSAN_MSPI1_MANUFACTURING_PAGE_LENGTH == 0u;
    }
    return address == ULSAN_MSPI1_MANUFACTURING_PAGE_OFFSET ||
           (s->manufacturing_page_observed != 0u &&
            address == ULSAN_MSPI1_SECONDARY_MANUFACTURING_PAGE_OFFSET);
}

int ulsan_mspi1_is_authentic_ota_read(uint32_t address, uint32_t count)
{
    uint64_t end = (uint64_t)address + count;
    return count > 0u && count <= 0x1000u &&
           address >= ULSAN_MSPI1_AUTHENTIC_OTA_START &&
           end <= (uint64_t)ULSAN_MSPI1_AUTHENTIC_OTA_END;
}

/* Lane IsNativePageProgramRange. */
static int is_native_page_program_range(ulsan_mspi1_state *s,
                                        uint32_t device_address,
                                        uint32_t count)
{
    uint64_t end = (uint64_t)device_address + count;
    int in_active_erase;
    int in_authentic_ota;
    if (count == 0u || count > ULSAN_MSPI1_PAGE_PROGRAM_LENGTH ||
        (device_address & (ULSAN_MSPI1_PAGE_PROGRAM_LENGTH - 1u)) +
            count > ULSAN_MSPI1_PAGE_PROGRAM_LENGTH) {
        return 0;
    }
    in_active_erase = s->active_erase_address != 0u &&
        device_address >= s->active_erase_address &&
        end <= (uint64_t)s->active_erase_address +
                   s->active_erase_length;
    in_authentic_ota = device_address >= ULSAN_MSPI1_AUTHENTIC_OTA_START &&
        end <= (uint64_t)ULSAN_MSPI1_AUTHENTIC_OTA_END;
    return in_active_erase || in_authentic_ota;
}

/* Lane IsProvenNativeFlashProgram: NOR programming may only clear
 * bits, so the target bytes must already contain the payload bits.
 * Validates the full payload before answering. */
int ulsan_mspi1_proven_flash_program(ulsan_mspi1_state *s,
                                     uint32_t guest_address,
                                     uint32_t device_address,
                                     uint32_t count)
{
    uint32_t index;
    if (!is_native_page_program_range(s, device_address, count)) {
        return 0;
    }
    for (index = 0u; index < count; index++) {
        uint32_t requested = 0u;
        uint8_t current = 0u;
        if (ulsan_mspi1_guest_read(s, guest_address + index,
                                   (uint8_t *)&requested) != 0) {
            return 0;
        }
        if (ulsan_mspi1_xip_read(s, device_address + index, &current) != 0) {
            return 0;
        }
        if (((uint32_t)current & requested) != requested) {
            return 0;
        }
    }
    return 1;
}

/* Guest queue-buffer access. All bulk operations validate every access
 * before committing any write (AGENTS validate-before-mutate). */
int ulsan_mspi1_guest_read(ulsan_mspi1_state *s, uint32_t address,
                           uint8_t *out)
{
    semu_error error;
    uint32_t value = 0u;
    semu_error_clear(&error);
    if (semu_bus_read(s->bus, address, 1u, &value, &error) != SEMU_OK) {
        return -1;
    }
    *out = (uint8_t)value;
    return 0;
}

int ulsan_mspi1_guest_read_u32(ulsan_mspi1_state *s, uint32_t address,
                               uint32_t *out)
{
    semu_error error;
    semu_error_clear(&error);
    return semu_bus_read(s->bus, address, 4u, out, &error) == SEMU_OK ? 0
                                                                      : -1;
}

int ulsan_mspi1_guest_write(ulsan_mspi1_state *s, uint32_t address,
                            uint8_t value)
{
    semu_error error;
    semu_error_clear(&error);
    return semu_bus_write(s->bus, address, 1u, value, &error) == SEMU_OK ? 0
                                                                        : -1;
}

int ulsan_mspi1_copy_guest_to_xip(ulsan_mspi1_state *s, uint32_t guest,
                                  uint32_t device, uint32_t count)
{
    uint8_t buffer[256];
    uint32_t index;
    if (count == 0u || count > sizeof(buffer)) {
        return -1;
    }
    for (index = 0u; index < count; index++) {
        if (ulsan_mspi1_guest_read(s, guest + index, &buffer[index]) != 0) {
            return -1;
        }
    }
    for (index = 0u; index < count; index++) {
        if (ulsan_mspi1_xip_write(s, device + index, buffer[index]) != 0) {
            return -1;
        }
    }
    return 0;
}

int ulsan_mspi1_copy_xip_to_guest(ulsan_mspi1_state *s, uint32_t guest,
                                  uint32_t device, uint32_t count)
{
    uint8_t buffer[4096];
    uint32_t index;
    if (count == 0u || count > sizeof(buffer)) {
        return -1;
    }
    for (index = 0u; index < count; index++) {
        if (ulsan_mspi1_xip_read(s, device + index, &buffer[index]) != 0) {
            return -1;
        }
    }
    for (index = 0u; index < count; index++) {
        if (ulsan_mspi1_guest_write(s, guest + index, buffer[index]) != 0) {
            return -1;
        }
    }
    return 0;
}

int ulsan_mspi1_fill_guest(ulsan_mspi1_state *s, uint32_t guest,
                           uint32_t count, uint8_t value)
{
    uint32_t index;
    uint8_t probe;
    for (index = 0u; index < count; index++) {
        if (ulsan_mspi1_guest_read(s, guest + index, &probe) != 0) {
            return -1;
        }
    }
    for (index = 0u; index < count; index++) {
        if (ulsan_mspi1_guest_write(s, guest + index, value) != 0) {
            return -1;
        }
    }
    return 0;
}

int ulsan_mspi1_erase_xip(ulsan_mspi1_state *s, uint32_t device,
                          uint32_t length)
{
    uint32_t index;
    if (device < ULSAN_MSPI1_AUTHENTIC_OTA_START && !in_backing(device)) {
        return -1; /* outside every proven backing region */
    }
    for (index = 0u; index < length; index++) {
        uint8_t probe;
        if (ulsan_mspi1_xip_read(s, device + index, &probe) != 0) {
            return -1;
        }
    }
    for (index = 0u; index < length; index++) {
        if (ulsan_mspi1_xip_write(s, device + index,
                                  ULSAN_MSPI1_ERASED_BYTE) != 0) {
            return -1;
        }
    }
    return 0;
}

int ulsan_mspi1_write_jedec(ulsan_mspi1_state *s, uint32_t address)
{
    if (ulsan_mspi1_guest_write(s, address, 0xC2u) != 0 ||
        ulsan_mspi1_guest_write(s, address + 1u, 0x25u) != 0 ||
        ulsan_mspi1_guest_write(s, address + 2u, 0x39u) != 0) {
        return -1;
    }
    return 0;
}

/* 0 when the four bytes match, 1 when they differ, -1 on a bus fault. */
int ulsan_mspi1_guest_prefix_is(ulsan_mspi1_state *s, uint32_t address,
                                unsigned b0, unsigned b1, unsigned b2,
                                unsigned b3)
{
    unsigned expected[4] = { b0, b1, b2, b3 };
    unsigned index;
    for (index = 0u; index < 4u; index++) {
        uint8_t byte = 0u;
        if (ulsan_mspi1_guest_read(s, address + index, &byte) != 0) {
            return -1;
        }
        if (byte != (uint8_t)expected[index]) {
            return 1;
        }
    }
    return 0;
}

void ulsan_mspi1_initialize_synthetic_range(ulsan_mspi1_state *s)
{
    if (s->synthetic_initialized != 0u) {
        return;
    }
    memset(s->backing, ULSAN_MSPI1_ERASED_BYTE, sizeof(s->backing));
    s->synthetic_initialized = 1u;
}

int ulsan_mspi1_native_is_lifecycle_status_read(ulsan_mspi1_state *s)
{
    return s->stage == ULSAN_MSPI1_AWAITING_STATUS_READ ||
           s->stage == ULSAN_MSPI1_COMPLETE ||
           s->stage == ULSAN_MSPI1_AWAITING_POST_CONFIGURATION_STATUS ||
           s->stage == ULSAN_MSPI1_PERSISTENCE_CREATION_HEADER_READ ||
           s->stage == ULSAN_MSPI1_AWAITING_PERSISTENCE_PROGRAM_STATUS ||
           s->stage == ULSAN_MSPI1_AWAITING_PERSISTENCE_ERASE_STATUS ||
           s->stage == ULSAN_MSPI1_PERSISTENCE_WRITE_STATUS_READY;
}

/* Lane IsObservedCommandQueueCount, clause by clause. */
int ulsan_mspi1_native_count_allowed(ulsan_mspi1_state *s,
                                     uint32_t device_address,
                                     uint32_t count)
{
    int fixture;
    if (ulsan_mspi1_is_authentic_ota_read(device_address, count)) {
        return 1;
    }
    fixture = ulsan_mspi1_has_manufacturing_fixture(s);
    if (count == ULSAN_MSPI1_MISSING_LOW_FLASH_LENGTH &&
        device_address == ULSAN_MSPI1_MISSING_LOW_FLASH_OFFSET && fixture) {
        return 1;
    }
    if (count == 3u &&
        (s->stage == ULSAN_MSPI1_INITIAL ||
         s->stage == ULSAN_MSPI1_AWAITING_QPI_IDENTIFICATION ||
         s->stage == ULSAN_MSPI1_MISSING_LOW_FLASH_SCAN_COMPLETE ||
         s->stage == ULSAN_MSPI1_AWAITING_STATUS_READ ||
         s->stage == ULSAN_MSPI1_COMPLETE)) {
        return 1;
    }
    if (count == 1u &&
        (ulsan_mspi1_native_is_lifecycle_status_read(s) ||
         s->stage == ULSAN_MSPI1_AWAITING_CONFIGURATION_WRITE)) {
        return 1;
    }
    if (count == ULSAN_MSPI1_HEADER_PROBE_LENGTH &&
        (s->stage == ULSAN_MSPI1_INITIALIZATION_COMPLETE ||
         s->stage == ULSAN_MSPI1_POST_SCAN_IDENTIFICATION_COMPLETE ||
         s->stage == ULSAN_MSPI1_PERSISTENCE_SECTOR_SCAN ||
         s->stage == ULSAN_MSPI1_PERSISTENCE_SECTOR_SCAN_COMPLETE ||
         s->manufacturing_page_observed != 0u ||
         s->stage == ULSAN_MSPI1_PERSISTENCE_WRITE_STATUS_READY)) {
        return 1;
    }
    if (count == ULSAN_MSPI1_RECORD_LENGTH &&
        (s->stage == ULSAN_MSPI1_PERSISTENCE_HEADER_READ ||
         s->stage == ULSAN_MSPI1_PERSISTENCE_SECTOR_SCAN ||
         s->stage == ULSAN_MSPI1_AWAITING_PERSISTENCE_PROGRAM ||
         s->stage == ULSAN_MSPI1_PERSISTENCE_WRITE_STATUS_READY ||
         s->synthetic_initialized != 0u)) {
        return 1;
    }
    if (s->stage == ULSAN_MSPI1_AWAITING_PERSISTENCE_PROGRAM &&
        is_native_page_program_range(s, device_address, count)) {
        return 1;
    }
    if (count == ULSAN_MSPI1_HEADER_PROBE_LENGTH &&
        s->synthetic_initialized != 0u &&
        device_address >= ULSAN_MSPI1_PERSIST_BACKING_START &&
        device_address + count <= ULSAN_MSPI1_PERSIST_BACKING_END) {
        return 1;
    }
    if (count == ULSAN_MSPI1_FOOTER_LENGTH &&
        s->stage == ULSAN_MSPI1_PERSISTENCE_WRITE_STATUS_READY) {
        return 1;
    }
    if (count == ULSAN_MSPI1_MANUFACTURING_PAGE_LENGTH && fixture &&
        ulsan_mspi1_is_observed_manufacturing_page(s, device_address)) {
        return 1;
    }
    if (count == ULSAN_MSPI1_HEADER_PROBE_LENGTH &&
        s->stage == ULSAN_MSPI1_FIRMWARE_FOOTER_READ_COMPLETE &&
        device_address == ULSAN_MSPI1_FOOTER_GAUGE_PROBE_OFFSET) {
        return 1;
    }
    if (count == ULSAN_MSPI1_MANUFACTURING_PAGE_LENGTH &&
        (s->stage == ULSAN_MSPI1_FIRMWARE_FOOTER_READ_COMPLETE ||
         s->stage == ULSAN_MSPI1_PERSISTENCE_WRITE_STATUS_READY ||
         s->manufacturing_page_observed != 0u)) {
        return 1;
    }
    if (count == ULSAN_MSPI1_CONFIG_NUGGET_LENGTH &&
        s->manufacturing_page_observed != 0u &&
        is_config_nugget_address(device_address)) {
        return 1;
    }
    if (count == 1u && s->calibration_reads_remaining > 0 &&
        device_address == 0u && fixture) {
        return 1;
    }
    return 0;
}

void ulsan_mspi1_native_reset(ulsan_mspi1_state *s)
{
    s->stage = ULSAN_MSPI1_INITIAL;
    s->next_read_address = 0u;
    s->next_write_address = 0u;
    s->active_erase_address = 0u;
    s->active_erase_length = 0u;
    s->manufacturing_page_observed = 0u;
    s->calibration_reads_remaining = 0;
}
