/*
 * Ulsan 2.35.36 cpu-complex DAXI block at 0x48000000 (ticket 730,
 * E-ULS-0011).
 *
 * The reference lane script (ambiq-apollo4.repl lines 197-201) is the
 * complete model: request.Value = 0x4 if request.Offset == 0x54 else 0,
 * with no store for writes (size 0x1000; repl comment: "DAXI Control = 0x4
 * - DAXIREADY bit set"). The boot reads 0x50 (bit-2 continue test at
 * 0x0009750c, observed 0 in the lane probe), reads 0x54, and stores 0x54;
 * the scratch access trace shows no other cpu-complex access through
 * instruction 200,000,000. Reads serve the script value for every offset
 * of the block (that is the lane's byte truth, not a fallback), writes
 * are accepted and discarded, and narrower accesses refuse because the
 * lane peripheral sees 32-bit requests.
 */

#include "ulsan_daxi.h"

#include "semu/types.h"
#include <stddef.h>

#define DAXI_DAXI_CONTROL 0x54u
#define DAXI_DAXI_CONTROL_VALUE UINT32_C(0x4)

static semu_status daxi_read(void *context, uint32_t offset,
                             unsigned width, uint32_t *value,
                             semu_error *error)
{
    (void)context;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan DAXI read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan DAXI read at 0x%08x width %u is unsupported",
                       offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    *value = offset == DAXI_DAXI_CONTROL ? DAXI_DAXI_CONTROL_VALUE
                                         : UINT32_C(0);
    return SEMU_OK;
}

static semu_status daxi_write(void *context, uint32_t offset,
                              unsigned width, uint32_t value,
                              semu_error *error)
{
    (void)context;
    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan DAXI write at 0x%08x width %u is unsupported",
                       offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    (void)value;
    return SEMU_OK;
}

static void daxi_reset(void *context)
{
    (void)context;
}

static semu_status silence_read(void *context, uint32_t offset,
                                unsigned width, uint32_t *value,
                                semu_error *error)
{
    (void)context;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan silence range read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan silence range read at 0x%08x width %u is "
                       "unsupported", offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    *value = 0u;
    return SEMU_OK;
}

static semu_status silence_write(void *context, uint32_t offset,
                                 unsigned width, uint32_t value,
                                 semu_error *error)
{
    (void)context;
    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan silence range write at 0x%08x width %u is "
                       "unsupported", offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    (void)value;
    return SEMU_OK;
}

static const semu_bus_device_ops silence_ops = {
    silence_read,
    silence_write,
    daxi_reset
};

static const semu_bus_device_ops daxi_ops = {
    daxi_read,
    daxi_write,
    daxi_reset
};

static int daxi_context;

/* sysbus SilenceRange declarations in ambiq-apollo4.repl: MCUCTRL and
 * SYNC_READ read as the zero the probe confirmed and ignore writes. */
static const uint32_t silence_base[2] = { 0x40020000u, 0x47FF0000u };
static const uint32_t silence_size[2] = { 0x1000u, 8u };
static const char *const silence_names[2] = { "ulsan.silence_muctrl",
                                              "ulsan.silence_sync" };

semu_status semu_ulsan_daxi_map(semu_bus *bus, semu_error *error)
{
    size_t index;

    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Ulsan DAXI needs a bus");
        return SEMU_ERR_ARGUMENT;
    }
    if (semu_bus_map_device(bus, "ulsan.daxi", 0x48000000u, 0x1000u,
                            &daxi_ops, &daxi_context, error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    for (index = 0u; index < 2u; ++index) {
        if (semu_bus_map_device(bus, silence_names[index],
                                silence_base[index], silence_size[index],
                                &silence_ops, &daxi_context,
                                error) != SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_STATE;
        }
    }
    return SEMU_OK;
}
