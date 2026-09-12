/*
 * Ulsan 2.35.36 watchdog control write at 0x40024000 (ticket 730,
 * E-ULS-0015). See ulsan_wdt.h: exactly one lane-logged write pair is
 * accepted; everything else refuses.
 */

#include "ulsan_wdt.h"

#include "semu/types.h"

#define WDT_BASE 0x40024000u
#define WDT_SIZE 0x1000u
#define WDT_CONTROL 0x000u
#define WDT_CONTROL_VALUE UINT32_C(0x033C3D06)
#define WDT_INTERRUPT_ENABLE 0x200u
#define WDT_RELOAD 0x4u
#define WDT_CONTROL_UNTAGGED_BIT UINT32_C(0x2)

/*
 * Control register: the lane probe at guest PC 0x000da892 reads
 * 0x33C3D04 after boot stored 0x33C3D06 - the register answers with the
 * stored value minus bit 1, matching "Unhandled bits: [1]" in the lane
 * write log. Boot's second control write is (read | 1) = 0x33C3D05,
 * which needs no log line (bit 1 clear) and is stored back unchanged.
 * Writes therefore store (value & ~bit1); reads return the store.
 *
 * The upstream register framework stores register writes and answers
 * reads with the stored value; the lane records the read before the
 * write (so the reset state 0 is the byte truth at read time) and the
 * write value 0x1. Boot repeats exactly this pair per delta pass
 * (scratch trace through instruction 200,000,000: the only accesses in
 * the upper window). */
static uint32_t wdt_interrupt_enable;
static uint32_t wdt_reload;
static uint32_t wdt_control;

static semu_status wdt_read(void *context, uint32_t offset, unsigned width,
                            uint32_t *value, semu_error *error)
{
    (void)context;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan watchdog read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width == 4u && offset == WDT_INTERRUPT_ENABLE) {
        *value = wdt_interrupt_enable;
        return SEMU_OK;
    }
    if (width == 4u && offset == WDT_CONTROL) {
        *value = wdt_control;
        return SEMU_OK;
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan watchdog read at 0x%08x width %u is unsupported "
                   "(lane logged none besides InterruptEnable)", offset,
                   width);
    return SEMU_ERR_UNSUPPORTED;
}

static semu_status wdt_write(void *context, uint32_t offset, unsigned width,
                             uint32_t value, semu_error *error)
{
    (void)context;
    if (width == 4u && offset == WDT_CONTROL) {
        /* Framework store dropping the untagged bit; the lane logged the
         * first write's value 0x33C3D06 verbatim. */
        wdt_control = value & ~WDT_CONTROL_UNTAGGED_BIT;
        return SEMU_OK;
    }
    if (width == 4u && offset == WDT_INTERRUPT_ENABLE) {
        wdt_interrupt_enable = value; /* framework register store */
        return SEMU_OK;
    }
    if (width == 4u && offset == WDT_RELOAD) {
        wdt_reload = value; /* lane-silent reload store, no read logged */
        return SEMU_OK;
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan watchdog write at 0x%08x width %u value 0x%08x "
                   "is unsupported", offset, width, value);
    return SEMU_ERR_UNSUPPORTED;
}

static void wdt_reset(void *context)
{
    (void)context;
    wdt_interrupt_enable = 0u;
    wdt_reload = 0u;
    wdt_control = 0u;
}

static const semu_bus_device_ops wdt_ops = {
    wdt_read,
    wdt_write,
    wdt_reset
};

static int wdt_context = 0;

semu_status semu_ulsan_wdt_map(semu_bus *bus, semu_error *error)
{
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Ulsan watchdog needs a bus");
        return SEMU_ERR_ARGUMENT;
    }
    wdt_reset(NULL); /* single shared static: every map starts a new machine */

    return semu_bus_map_device(bus, "ulsan.wdt", WDT_BASE, WDT_SIZE, &wdt_ops,
                               &wdt_context, error);
}
