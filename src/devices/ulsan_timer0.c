/*
 * Ulsan 2.35.36 TIMER block registers at 0x40008000 (ticket 730,
 * E-ULS-0021).
 *
 * Boot touches seven TIMER registers per delta pass (in-tree scratch
 * access trace through instruction 200,000,000): a read and a read-
 * modify-write of TIMER0 offset 0x10 (store 0x2), a read and a store of
 * 0x4 at 0x60, a store of 0x4 at 0x68, and on the TIMER1 page five reads
 * and stores 0x1, 0xA20, 0x2, 0 at 0x220 (control), 0x20 then 0 at 0x228,
 * 0 at 0x22c, and 0 at 0x230. The lane logged no timer lines at all,
 * matching upstream-handled registers whose framework stores writes and
 * answers reads from the store; all observed reads return 0, consistent
 * with first reads from the reset state.
 *
 * This device stores exactly those seven registers (reset 0, cleared on
 * machine reset) and answers their aligned 32-bit accesses; every other
 * address, width, and read of a write-only-observed register refuses.
 */

#include "ulsan_timer0.h"

#include "semu/types.h"

#define TIMER0_BASE 0x40008000u
#define TIMER0_SIZE 0x800u

typedef struct {
    uint32_t regs[7];
} timer0_state;

static timer0_state timer0_instance;

static int timer0_slot(uint32_t offset, unsigned *slot, int write)
{
    switch (offset) {
    case 0x010u: *slot = 0u; break;
    case 0x060u: *slot = 1u; break;
    case 0x068u: *slot = 2u; break;
    case 0x220u: *slot = 3u; break;
    case 0x228u: *slot = 4u; break;
    case 0x22cu: *slot = 5u; break;
    case 0x230u: *slot = 6u; break;
    default: return 0;
    }
    if (!write) {
        switch (offset) {
        case 0x010u: /* fallthrough */
        case 0x060u: /* fallthrough */
        case 0x220u:
            return 1; /* reads observed only for these three */
        default: return 0;
        }
    }
    return 1;
}

static semu_status timer0_read(void *context, uint32_t offset,
                               unsigned width, uint32_t *value,
                               semu_error *error)
{
    unsigned slot = 0u;

    (void)context;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan TIMER read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || (offset & 3u) != 0u ||
        !timer0_slot(offset, &slot, 0)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan TIMER read at 0x%08x width %u is "
                       "unsupported", offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    *value = timer0_instance.regs[slot];
    return SEMU_OK;
}

static semu_status timer0_write(void *context, uint32_t offset,
                                unsigned width, uint32_t value,
                                semu_error *error)
{
    unsigned slot = 0u;

    (void)context;
    if (width != 4u || (offset & 3u) != 0u ||
        !timer0_slot(offset, &slot, 1)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan TIMER write at 0x%08x width %u is "
                       "unsupported", offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    timer0_instance.regs[slot] = value;
    return SEMU_OK;
}

static void timer0_reset(void *context)
{
    unsigned index = 0u;

    (void)context;
    for (index = 0u; index < 7u; ++index) {
        timer0_instance.regs[index] = 0u;
    }
}

static const semu_bus_device_ops timer0_ops = {
    timer0_read,
    timer0_write,
    timer0_reset
};

semu_status semu_ulsan_timer0_map(semu_bus *bus, semu_error *error)
{
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan TIMER needs a bus");
        return SEMU_ERR_ARGUMENT;
    }
    timer0_reset(NULL);
    return semu_bus_map_device(bus, "ulsan.timer0", TIMER0_BASE, TIMER0_SIZE,
                               &timer0_ops, &timer0_instance, error);
}
