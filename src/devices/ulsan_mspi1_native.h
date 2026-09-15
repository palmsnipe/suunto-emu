/*
 * Shared internals for the Ulsan MSPI1 module (ticket 730, E-ULS-0033).
 *
 * The native response machine mirrors the lane plugin class
 * `UlsanApollo4Mspi1` stage for stage; the register-plane file owns the
 * queue start dispatch and the interrupt line, this header owns the
 * shared state and constants so the files stay below the size
 * thresholds.
 */
#ifndef SEMU_DEVICES_ULSAN_MSPI1_NATIVE_H
#define SEMU_DEVICES_ULSAN_MSPI1_NATIVE_H

#include <stdint.h>

#include "semu/apollo4.h"
#include "semu/bus.h"

/* Lane NativeInitializationStage, same order (SapporoApollo4Mspi1.cs
 * line 1059). */
typedef enum {
    ULSAN_MSPI1_INITIAL = 0,
    ULSAN_MSPI1_AWAITING_ENTER_4BYTE,
    ULSAN_MSPI1_AWAITING_ENTER_QPI,
    ULSAN_MSPI1_AWAITING_QPI_IDENTIFICATION,
    ULSAN_MSPI1_AWAITING_STATUS_READ,
    ULSAN_MSPI1_COMPLETE,
    ULSAN_MSPI1_AWAITING_CONFIGURATION_WRITE,
    ULSAN_MSPI1_AWAITING_POST_CONFIGURATION_STATUS,
    ULSAN_MSPI1_INITIALIZATION_COMPLETE,
    ULSAN_MSPI1_PERSISTENCE_HEADER_READ,
    ULSAN_MSPI1_PERSISTENCE_SECTOR_SCAN,
    ULSAN_MSPI1_PERSISTENCE_SECTOR_SCAN_COMPLETE,
    ULSAN_MSPI1_MISSING_LOW_FLASH_SCAN_COMPLETE,
    ULSAN_MSPI1_POST_SCAN_IDENTIFICATION_COMPLETE,
    ULSAN_MSPI1_PERSISTENCE_CREATION_HEADER_READ,
    ULSAN_MSPI1_PERSISTENCE_WRITE_STATUS_READY,
    ULSAN_MSPI1_AWAITING_PERSISTENCE_PROGRAM,
    ULSAN_MSPI1_AWAITING_PERSISTENCE_PROGRAM_STATUS,
    ULSAN_MSPI1_AWAITING_PERSISTENCE_ERASE_STATUS,
    ULSAN_MSPI1_FIRMWARE_FOOTER_READ_COMPLETE
} ulsan_mspi1_stage;

/* Lane constants (SapporoApollo4Mspi1.cs lines 1093-1136). */
#define ULSAN_MSPI1_PERSIST_BACKING_START 0x00010000u
#define ULSAN_MSPI1_PERSIST_BACKING_END   0x00030000u
#define ULSAN_MSPI1_BACKING_LENGTH \
    (ULSAN_MSPI1_PERSIST_BACKING_END - ULSAN_MSPI1_PERSIST_BACKING_START)
#define ULSAN_MSPI1_SECTOR_LENGTH 0x1000u
#define ULSAN_MSPI1_RECORD_LENGTH 64u
#define ULSAN_MSPI1_HEADER_PROBE_LENGTH 4u
#define ULSAN_MSPI1_QUEUE_READ_CONTROL 0x13u
#define ULSAN_MSPI1_QUEUE_PROGRAM_CONTROL 0x17u
#define ULSAN_MSPI1_FOOTER_LENGTH 36u
#define ULSAN_MSPI1_FOOTER_OFFSET 0x01FF0000u
#define ULSAN_MSPI1_FOOTER_GAUGE_PROBE_OFFSET 0x00010200u
#define ULSAN_MSPI1_AUTHENTIC_OTA_START 0x00040000u
#define ULSAN_MSPI1_AUTHENTIC_OTA_END 0x02000000u
#define ULSAN_MSPI1_MISSING_LOW_FLASH_OFFSET 0x00001000u
#define ULSAN_MSPI1_MISSING_LOW_FLASH_LENGTH 8u
#define ULSAN_MSPI1_MANUFACTURING_PAGE_LENGTH 256u
#define ULSAN_MSPI1_MANUFACTURING_PAGE_OFFSET 0x0003F000u
#define ULSAN_MSPI1_SECONDARY_MANUFACTURING_PAGE_OFFSET 0x0003F600u
#define ULSAN_MSPI1_CONFIG_NUGGET_SECTOR 0x00002000u
#define ULSAN_MSPI1_CONFIG_NUGGET_SECTOR_LENGTH 0x1000u
#define ULSAN_MSPI1_CONFIG_NUGGET_LENGTH 32u
#define ULSAN_MSPI1_ERASED_BYTE 0xFFu
#define ULSAN_MSPI1_SENTINEL_BYTE 0xA5u
#define ULSAN_MSPI1_PIO_START_CONTROL 0xC1u
#define ULSAN_MSPI1_PIO_ADDRESSED_START_CONTROL 0xE1u
#define ULSAN_MSPI1_CMD_ENTER_4BYTE 0xB7u
#define ULSAN_MSPI1_CMD_ENTER_QPI 0x35u
#define ULSAN_MSPI1_CMD_WRITE_ENABLE 0x06u
#define ULSAN_MSPI1_CMD_ERASE_RESUME 0x7Au
#define ULSAN_MSPI1_CMD_BLOCK_ERASE_64K 0xDCu
#define ULSAN_MSPI1_CMD_SECTOR_ERASE_4K 0x21u
#define ULSAN_MSPI1_BLOCK_ERASE_LENGTH 0x10000u
#define ULSAN_MSPI1_PAGE_PROGRAM_LENGTH 0x100u
#define ULSAN_MSPI1_INT_QUEUE_COMPLETE 0x40u
#define ULSAN_MSPI1_INT_PIO_COMPLETE 0x01u

/* Lane base-class store-through dictionary (SapporoApollo4Mspi1.cs
 * ReadDoubleWord/WriteDoubleWord default cases): every 32-bit aligned
 * word inside the 0x1000 window is stored on write and reads back,
 * 0 while never stored; Reset clears it (registers.Clear()). */
#define ULSAN_MSPI1_REG_WORDS 1024u

typedef struct {
    uint32_t registers[6];          /* 0 queue control, 1 address,
                                       2 device, 3 count,
                                       4 PIO control, 5 PIO address/
                                       command pair storage below     */
    uint32_t pio_command;
    uint32_t interrupt_enable;
    uint32_t interrupt_status;
    unsigned irq_level;
    semu_bus *bus;                  /* set at map; SRAM/XIP access      */
    semu_apollo4_irq_fn irq_sink;
    void *irq_context;
    /* Native machine (lane ResetProductState clears all of these
     * except the two backing-store fields: emulated flash is
     * non-volatile across the emulated reset). */
    ulsan_mspi1_stage stage;
    uint32_t next_read_address;
    uint32_t next_write_address;
    uint32_t active_erase_address;
    uint32_t active_erase_length;
    unsigned manufacturing_page_observed;
    int calibration_reads_remaining;
    unsigned synthetic_initialized;
    uint32_t reg_store[ULSAN_MSPI1_REG_WORDS]; /* lane store-through dict */
    uint8_t backing[ULSAN_MSPI1_BACKING_LENGTH];
} ulsan_mspi1_state;

/* XIP routing: persistence range in device-local backing, everything
 * else through the bus (0 = success, -1 = refuse). */
int ulsan_mspi1_xip_read(ulsan_mspi1_state *s, uint32_t device_address,
                         uint8_t *out);
int ulsan_mspi1_xip_write(ulsan_mspi1_state *s, uint32_t device_address,
                          uint8_t value);
int ulsan_mspi1_has_manufacturing_fixture(ulsan_mspi1_state *s);
int ulsan_mspi1_is_observed_manufacturing_page(ulsan_mspi1_state *s,
                                               uint32_t address);
int ulsan_mspi1_is_authentic_ota_read(uint32_t address, uint32_t count);
int ulsan_mspi1_native_is_lifecycle_status_read(ulsan_mspi1_state *s);
int ulsan_mspi1_native_count_allowed(ulsan_mspi1_state *s,
                                     uint32_t device_address,
                                     uint32_t count);
/* Guest queue-buffer access helpers (validate-before-commit). */
int ulsan_mspi1_write_jedec(ulsan_mspi1_state *s, uint32_t address);
int ulsan_mspi1_guest_prefix_is(ulsan_mspi1_state *s, uint32_t address,
                                unsigned b0, unsigned b1, unsigned b2,
                                unsigned b3);
int ulsan_mspi1_guest_read(ulsan_mspi1_state *s, uint32_t address,
                           uint8_t *out);
int ulsan_mspi1_guest_read_u32(ulsan_mspi1_state *s, uint32_t address,
                               uint32_t *out);
int ulsan_mspi1_guest_write(ulsan_mspi1_state *s, uint32_t address,
                            uint8_t value);
int ulsan_mspi1_copy_guest_to_xip(ulsan_mspi1_state *s, uint32_t guest,
                                  uint32_t device, uint32_t count);
int ulsan_mspi1_copy_xip_to_guest(ulsan_mspi1_state *s, uint32_t guest,
                                  uint32_t device, uint32_t count);
int ulsan_mspi1_fill_guest(ulsan_mspi1_state *s, uint32_t guest,
                           uint32_t count, uint8_t value);
int ulsan_mspi1_erase_xip(ulsan_mspi1_state *s, uint32_t device,
                          uint32_t length);
int ulsan_mspi1_proven_flash_program(ulsan_mspi1_state *s,
                                     uint32_t guest_address,
                                     uint32_t device_address,
                                     uint32_t count);
void ulsan_mspi1_initialize_synthetic_range(ulsan_mspi1_state *s);

/* Returns 1 when the response was populated (queue completion fires).
 * Validates every access before committing (AGENTS DMA rule). */
int ulsan_mspi1_native_try_populate(ulsan_mspi1_state *s, uint32_t control,
                                    uint32_t address, uint32_t count);
/* Returns interrupt bits to raise (bit 0 = PIO complete), else 0. */
uint32_t ulsan_mspi1_native_pio_write(ulsan_mspi1_state *s,
                                      uint32_t offset, uint32_t value);
/* Tail branches of the lane chain (class lines 690-845): the 2.44.52
 * descriptor shapes, the configuration write, lifecycle status reads,
 * and the main JEDEC fall-through. */
int ulsan_mspi1_native_tail(ulsan_mspi1_state *s, uint32_t control,
                            uint32_t address, uint32_t count, int fixture);

void ulsan_mspi1_native_reset(ulsan_mspi1_state *s);

#endif /* SEMU_DEVICES_ULSAN_MSPI1_NATIVE_H */
