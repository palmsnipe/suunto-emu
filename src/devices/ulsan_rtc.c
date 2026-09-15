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
 * E-ULS-0036 adds the sleep-path counter window (tree refusal site pc
 * 0x0007aa66, guest seqlock 0x0007aa5c): +0x20 answers the lane's live
 * counter - BCD hundredths-of-a-second since the epoch, lane calibration
 * run lp40 (0.1 s -> 0x00000010, 0.9 s -> 0x90, 1.001 s -> 0x100, 2.0 s
 * -> 0x200, 10.0 s -> 0x1000, 11.0 s -> 0x1100; hundredths resolution
 * confirmed at 3 ms sampling), modelled as the scheduler's virtual time
 * divided by 10 ms and BCD-packed; +0x24 answers its store with reset
 * value 0x14700101, the constant the lane logged at every sample time.
 * Neither counter word was ever written in the lane, so writes there
 * refuse while reads need the attached machine scheduler (no scheduler:
 * the reads refuse, fail-closed). The guest reads +0x20 twice and
 * retries until stable; deterministic virtual time keeps that loop
 * bounded. Every other RTC address and width refuses. Reset clears the
 * stores and restarts the counter epoch with the scheduler; attach
 * survives device reset (maps detach, per the E-ULS-0035 seam).
 */

#include "ulsan_rtc.h"

#include "semu/scheduler.h"
#include "semu/types.h"

#define RTC_BASE 0x40004800u
#define RTC_SIZE 0x800u

#define RTC_UPPER_RESET UINT32_C(0x14700101)

typedef struct {
    uint32_t regs[4]; /* +0x00, +0x30, +0x200, +0x208 */
    uint32_t upper;   /* +0x24, reset 0x14700101 */
    semu_scheduler *scheduler; /* attached machine scheduler */
} rtc_state;

static rtc_state rtc_instance;

static int rtc_slot(uint32_t offset, unsigned *slot)
{
    switch (offset) {
    case 0x000u: *slot = 0u; return 1;
    case 0x030u: *slot = 1u; return 1;
    case 0x200u: *slot = 2u; return 1;
    case 0x208u: *slot = 3u; return 1;
    case 0x020u: *slot = 4u; return 1; /* counter read, write refuses */
    case 0x024u: *slot = 5u; return 1; /* counter read, write refuses */
    default: return 0;
    }
}

/* Elapsed scheduler time in hundredths of a second, BCD-packed (max 8
 * digits; observed runs never reach the cap). */
static uint32_t rtc_bcd_hundredths(const semu_scheduler *scheduler)
{
    uint64_t hundredths = semu_scheduler_now(scheduler) / UINT64_C(10000000);
    uint32_t out = 0u;
    unsigned shift = 0u;
    unsigned digit;

    for (digit = 0u; digit < 8u && hundredths != 0u; ++digit) {
        out |= (uint32_t)((hundredths % 10u) << shift);
        hundredths /= 10u;
        shift += 4u;
    }
    return out;
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
    if (slot == 4u) {
        if (rtc_instance.scheduler == NULL) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "Ulsan RTC counter needs the machine scheduler");
            return SEMU_ERR_UNSUPPORTED;
        }
        *value = rtc_bcd_hundredths(rtc_instance.scheduler);
        return SEMU_OK;
    }
    *value = (slot == 5u) ? rtc_instance.upper : rtc_instance.regs[slot];
    return SEMU_OK;
}

static semu_status rtc_write(void *context, uint32_t offset, unsigned width,
                             uint32_t value, semu_error *error)
{
    unsigned slot = 0u;

    (void)context;
    if (width != 4u || (offset & 3u) != 0u || !rtc_slot(offset, &slot) ||
        slot > 3u) {
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
    rtc_instance.upper = RTC_UPPER_RESET;
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
    rtc_instance.scheduler = NULL;
    rtc_reset(NULL);
    return semu_bus_map_device(bus, "ulsan.rtc", RTC_BASE, RTC_SIZE,
                               &rtc_ops, &rtc_instance, error);
}

void semu_ulsan_rtc_attach(semu_scheduler *scheduler)
{
    /* The machine attaches after the board map: device maps reset
     * detach. Device resets keep the scheduler (epoch restarts with
     * the scheduler's own reset). */
    rtc_instance.scheduler = scheduler;
}
