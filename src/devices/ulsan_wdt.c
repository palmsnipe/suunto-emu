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

static semu_status wdt_read(void *context, uint32_t offset, unsigned width,
                            uint32_t *value, semu_error *error)
{
    (void)context;
    (void)value;
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan watchdog read at 0x%08x width %u is unsupported "
                   "(lane never logged one)", offset, width);
    return SEMU_ERR_UNSUPPORTED;
}

static semu_status wdt_write(void *context, uint32_t offset, unsigned width,
                             uint32_t value, semu_error *error)
{
    (void)context;
    if (width == 4u && offset == WDT_CONTROL && value == WDT_CONTROL_VALUE) {
        /* Lane-log line: "value 0x33C3D06", no expiry effects after it. */
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
}

static const semu_bus_device_ops wdt_ops = {
    wdt_read,
    wdt_write,
    wdt_reset
};

static int wdt_context;

semu_status semu_ulsan_wdt_map(semu_bus *bus, semu_error *error)
{
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Ulsan watchdog needs a bus");
        return SEMU_ERR_ARGUMENT;
    }
    return semu_bus_map_device(bus, "ulsan.wdt", WDT_BASE, WDT_SIZE, &wdt_ops,
                               &wdt_context, error);
}
