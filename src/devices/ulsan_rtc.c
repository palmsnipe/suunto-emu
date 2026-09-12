/*
 * Ulsan 2.35.36 RTC block registers at 0x40004800 (ticket 730,
 * E-ULS-0019).
 *
 * Boot touches four RTC registers per delta pass (in-tree scratch access
 * trace through instruction 200,000,000): two reads and two stores
 * (0x0E then 0) on +0x0, two reads and a store of 0 on +0x30, a read
 * and a store of 1 on +0x200, and a store of 1 on +0x208. The lane
 * logged no RTC lines at all, matching upstream-handled registers whose
 * framework stores writes and answers reads from the store. Reads
 * therefore answer the store; with every trace value 0 at the reads,
 * the first-read-then-store sequence is reproduced byte-exactly and the
 * steady-state read-back follows the framework store semantics.
 *
 * This device stores the four observed registers (reset 0) and answers
 * their aligned 32-bit accesses; every other RTC address and width
 * refuses. Reset clears the stores.
 */

#include "ulsan_rtc.h"

#include "semu/types.h"

#define RTC_BASE 0x40004800u
#define RTC_SIZE 0x800u

typedef struct {
    uint32_t regs[4]; /* +0x00, +0x30, +0x200, +0x208 */
} rtc_state;

static rtc_state rtc_instance;

static int rtc_slot(uint32_t offset, unsigned *slot)
{
    switch (offset) {
    case 0x000u: *slot = 0u; return 1;
    case 0x030u: *slot = 1u; return 1;
    case 0x200u: *slot = 2u; return 1;
    case 0x208u: *slot = 3u; return 1;
    default: return 0;
    }
}

static semu_status rtc_read(void *context, uint32_t offset, unsigned width,
                            uint32_t *value, semu_error *error)
{
    unsigned slot = 0u;

    (void)context;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Ulsan RTC read required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || (offset & 3u) != 0u || !rtc_slot(offset, &slot)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan RTC read at 0x%08x width %u is unsupported",
                       offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    *value = rtc_instance.regs[slot];
    return SEMU_OK;
}

static semu_status rtc_write(void *context, uint32_t offset, unsigned width,
                             uint32_t value, semu_error *error)
{
    unsigned slot = 0u;

    (void)context;
    if (width != 4u || (offset & 3u) != 0u || !rtc_slot(offset, &slot)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan RTC write at 0x%08x width %u is unsupported",
                       offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    rtc_instance.regs[slot] = value;
    return SEMU_OK;
}

static void rtc_reset(void *context)
{
    (void)context;
    rtc_instance.regs[0] = 0u;
    rtc_instance.regs[1] = 0u;
    rtc_instance.regs[2] = 0u;
    rtc_instance.regs[3] = 0u;
}

static const semu_bus_device_ops rtc_ops = {
    rtc_read,
    rtc_write,
    rtc_reset
};

semu_status semu_ulsan_rtc_map(semu_bus *bus, semu_error *error)
{
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Ulsan RTC needs a bus");
        return SEMU_ERR_ARGUMENT;
    }
    rtc_reset(NULL);
    return semu_bus_map_device(bus, "ulsan.rtc", RTC_BASE, RTC_SIZE,
                               &rtc_ops, &rtc_instance, error);
}
