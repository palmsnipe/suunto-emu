/*
 * Ulsan 2.35.36 clock generator at 0x40004000 (ticket 730, E-ULS-0010).
 *
 * The reference lane models this block with clkgen_ulsan, a
 * Python.PythonPeripheral declared in the Ulsan platform description
 * (ulsan-platform.repl lines 13-24, size 0x800) whose script is the
 * complete model: a sparse dictionary where a read returns the stored
 * value for the offset or 0 when none was stored, and a write stores the
 * written 32-bit value under its offset. The in-tree device reproduces
 * that script exactly over a bounded slot table (the boot phase stores at
 * most two offsets; a table overflow refuses instead of guessing lane
 * capacity).
 *
 * Observed boot usage (lane probes 8/9 at the guest access points plus the
 * in-tree scratch access trace through instruction 200,000,000): boot
 * reads offset 0x44 -> 0, stores 0x00FC0000 (0x00096b8a), reads back
 * 0x00FC0000, stores 0x00FC0040 (0x00096bd4), and the value survives to
 * the idle dump. The platform comment also records repeated +0x84
 * read/writes during display-clock bring-up; the dictionary serves those
 * with the same rule.
 */

#include "ulsan_clkgen.h"

#include "semu/types.h"

#define CLKGEN_SIZE 0x800u
#define CLKGEN_SLOTS 64u

typedef struct {
    uint32_t stored[CLKGEN_SLOTS];
    uint32_t value[CLKGEN_SLOTS];
    unsigned used;
} clkgen_state;

/* Single boot-phase machine context, like the other Ulsan devices;
 * reset clears it. */
static clkgen_state clkgen_instance;

static int clkgen_find(const clkgen_state *state, uint32_t offset)
{
    unsigned index;

    for (index = 0u; index < state->used; ++index) {
        if (state->stored[index] == offset) {
            return (int)index;
        }
    }
    return -1;
}

static semu_status clkgen_read(void *context, uint32_t offset,
                               unsigned width, uint32_t *value,
                               semu_error *error)
{
    clkgen_state *state = (clkgen_state *)context;
    int slot;

    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan clock generator read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan clock generator read at 0x%08x width %u is "
                       "unsupported", offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    slot = clkgen_find(state, offset);
    *value = slot >= 0 ? state->value[slot] : 0u;
    return SEMU_OK;
}

static semu_status clkgen_write(void *context, uint32_t offset,
                                unsigned width, uint32_t value,
                                semu_error *error)
{
    clkgen_state *state = (clkgen_state *)context;
    int slot;

    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan clock generator write at 0x%08x width %u is "
                       "unsupported", offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    slot = clkgen_find(state, offset);
    if (slot < 0) {
        if (state->used >= CLKGEN_SLOTS) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "Ulsan clock generator storage exhausted at "
                           "0x%08x", offset);
            return SEMU_ERR_UNSUPPORTED;
        }
        slot = (int)state->used;
        state->stored[state->used] = offset;
        state->used++;
    }
    state->value[slot] = value;
    return SEMU_OK;
}

static void clkgen_reset(void *context)
{
    clkgen_state *state = (clkgen_state *)context;

    state->used = 0u;
}

static const semu_bus_device_ops clkgen_ops = {
    clkgen_read,
    clkgen_write,
    clkgen_reset
};

semu_status semu_ulsan_clkgen_map(semu_bus *bus, semu_error *error)
{
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan clock generator needs a bus");
        return SEMU_ERR_ARGUMENT;
    }
    clkgen_instance.used = 0u;
    return semu_bus_map_device(bus, "ulsan.clkgen", 0x40004000u,
                               CLKGEN_SIZE, &clkgen_ops, &clkgen_instance,
                               error);
}
