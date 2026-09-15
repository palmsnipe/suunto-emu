/*
 * Ulsan 2.35.36 SystemTimer registers at 0x40008800 (ticket 730,
 * E-ULS-0014). See ulsan_stimer.h for the full evidence summary.
 */

#include "ulsan_stimer.h"

#include "semu/types.h"

#define STIMER_BASE        0x40008800u
#define STIMER_SIZE        0x200u
#define STIMER_LOAD        0x000u
#define STIMER_COUNT       0x004u
#define STIMER_CONTROL     0x100u
/* STIMER NVRAM words at +0x50..+0x5c (E-ULS-0020 pinned reads of 0;
 * E-ULS-0038 identified the plane): the lane wrapper
 * Apollo4RetainedSystemTimer intercepts every read and write of these
 * four words ahead of the upstream SystemTimer (Nvram0Offset 0x50 ..
 * Nvram3Offset 0x5c), backed by its own uint[4], and its Reset()
 * deliberately keeps them - "Apollo4's four STIMER NVRAM words survive
 * a software reset. The firmware uses them to carry its next startup
 * mode across AIRCR." */
#define STIMER_NVRAM0 0x050u
#define STIMER_NVRAM1 0x054u
#define STIMER_NVRAM2 0x058u
#define STIMER_NVRAM3 0x05cu
/* The lane read LOAD back as 0x303 after boot's 0x80000000 write, so
 * that write's bit 31 is not stored. Other LOAD bits keep observed
 * store behavior; no other LOAD bit has proven evidence to mask. */
#define STIMER_LOAD_STORE_MASK UINT32_C(0x7FFFFFFF)

typedef struct {
    uint32_t nvram[4];
    uint32_t load;
    uint32_t control;
    uint32_t count;
} stimer_state;

static stimer_state stimer_instance;

static semu_status stimer_read(void *context, uint32_t offset,
                               unsigned width, uint32_t *value,
                               semu_error *error)
{
    stimer_state *state = (stimer_state *)context;

    (void)state;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan SystemTimer read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan SystemTimer read at 0x%08x width %u is "
                       "unsupported", offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    switch (offset) {
    case STIMER_LOAD:
        *value = stimer_instance.load;
        return SEMU_OK;
    case STIMER_COUNT:
        /* Free-running lane counter (probe: reaches 0xA7ED with LOAD
         * 0x303); advance once per read: monotonic, stable per pass. */
        *value = stimer_instance.count;
        stimer_instance.count += 1u;
        return SEMU_OK;
    case STIMER_CONTROL:
        *value = stimer_instance.control;
        return SEMU_OK;
    case STIMER_NVRAM0:
    case STIMER_NVRAM1:
    case STIMER_NVRAM2:
    case STIMER_NVRAM3:
        *value = stimer_instance.nvram[(offset - STIMER_NVRAM0) / 4u];
        return SEMU_OK;
    default:
        break;
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan SystemTimer read at 0x%08x is unsupported",
                   offset);
    return SEMU_ERR_UNSUPPORTED;
}

static semu_status stimer_write(void *context, uint32_t offset,
                                unsigned width, uint32_t value,
                                semu_error *error)
{
    (void)context;
    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan SystemTimer write at 0x%08x width %u is "
                       "unsupported", offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    switch (offset) {
    case STIMER_LOAD:
        stimer_instance.load = value & STIMER_LOAD_STORE_MASK;
        return SEMU_OK;
    case STIMER_CONTROL:
        stimer_instance.control = value;
        return SEMU_OK;
    case STIMER_NVRAM0:
    case STIMER_NVRAM1:
    case STIMER_NVRAM2:
    case STIMER_NVRAM3:
        stimer_instance.nvram[(offset - STIMER_NVRAM0) / 4u] = value;
        return SEMU_OK;
    default:
        break;
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan SystemTimer write at 0x%08x is unsupported",
                   offset);
    return SEMU_ERR_UNSUPPORTED;
}

static void stimer_reset(void *context)
{
    (void)context;
    stimer_instance.load = 0u;
    stimer_instance.control = 0u;
    stimer_instance.count = 0u;
    /* NVRAM words are deliberately not cleared: the lane wrapper's
     * Reset() keeps them (SapporoApollo4Extensions.cs line 136) so the
     * firmware carries its next startup mode across AIRCR. The static
     * instance is already zero on a fresh process map. */
}

static const semu_bus_device_ops stimer_ops = {
    stimer_read,
    stimer_write,
    stimer_reset
};

semu_status semu_ulsan_stimer_map(semu_bus *bus, semu_error *error)
{
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan SystemTimer needs a bus");
        return SEMU_ERR_ARGUMENT;
    }
    /* Fresh machine: NVRAM starts at power-on zero (lane power-on has an
     * empty wrapper array); in-machine resets keep it via stimer_reset. */
    stimer_instance.nvram[0] = 0u;
    stimer_instance.nvram[1] = 0u;
    stimer_instance.nvram[2] = 0u;
    stimer_instance.nvram[3] = 0u;
    stimer_reset(NULL);
    return semu_bus_map_device(bus, "ulsan.stimer", STIMER_BASE, STIMER_SIZE,
                               &stimer_ops, &stimer_instance, error);
}
