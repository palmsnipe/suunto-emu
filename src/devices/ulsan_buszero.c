/*
 * Ulsan 2.35.36 lane-recorded bus-zero reads (ticket 730, E-ULS-0013).
 *
 * See ulsan_buszero.h. Current table entries, one per reproduced lane
 * sysbus warning line: the CRYPTO tag range (upstream repl tag
 * <0x400C0000,0x400C3FFF>) read at 0x400C0FE0 from guest PC 0x00096A60,
 * logged "returning 0x00000000"; the in-tree scratch access trace shows
 * this is the block's only access through instruction 200,000,000 (one
 * read per boot pass) and no write ever targets it.
 */

#include "ulsan_buszero.h"

#include "semu/types.h"
#include <stddef.h>

typedef struct {
    uint32_t base;   /* device window start (the declared tag range)  */
    uint32_t size;   /* window size                                   */
    uint32_t offset; /* offset inside the window with lane-recorded value */
    uint32_t value;  /* the value the lane sysbus log recorded        */
    int has_write;   /* the lane log also recorded a write here       */
    uint32_t write_value; /* that logged write value (discarded)       */
    const char *name;
} zero_window;

static const zero_window zero_windows[] = {
    { 0x400C0000u, 0x4000u, 0xFE0u, UINT32_C(0), 0, 0u,
      "ulsan.buszero.crypto" },
    /* Guest logger port at 0x40000000 (ticket 730, E-ULS-0016): lane
     * lines "ReadDoubleWord from non existing peripheral at 0x40000000"
     * (lane probe pins the returned value to 0) and "WriteDoubleWord to
     * non existing peripheral at 0x40000000, value 0x2"; the scratch
     * trace shows reads (returning 0) and 0x2 stores are the only
     * accesses through instruction 200,000,000. */
    { 0x40000000u, 0x4u, 0x0u, UINT32_C(0), 1, UINT32_C(2),
      "ulsan.buszero.logger" }
};

typedef struct {
    const zero_window *window;
} buszero_context;

static semu_status buszero_read(void *context, uint32_t offset,
                                unsigned width, uint32_t *value,
                                semu_error *error)
{
    const buszero_context *ctx = (const buszero_context *)context;

    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan bus-zero read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan bus-zero read at offset 0x%08x width %u is "
                       "unsupported", offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    if (offset == ctx->window->offset) {
        *value = ctx->window->value;
        return SEMU_OK;
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan bus-zero read at 0x%08x is unsupported",
                   ctx->window->base + offset);
    return SEMU_ERR_UNSUPPORTED;
}

static semu_status buszero_write(void *context, uint32_t offset,
                                 unsigned width, uint32_t value,
                                 semu_error *error)
{
    const buszero_context *ctx = (const buszero_context *)context;

    if (ctx->window->has_write && width == 4u &&
        offset == ctx->window->offset &&
        value == ctx->window->write_value) {
        return SEMU_OK; /* the lane sysbus discarded this logged write */
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan bus-zero write at 0x%08x width %u is unsupported",
                   ctx->window->base + offset, width);
    return SEMU_ERR_UNSUPPORTED;
}

static void buszero_reset(void *context)
{
    (void)context;
}

static const semu_bus_device_ops buszero_ops = {
    buszero_read,
    buszero_write,
    buszero_reset
};

/* Stable storage for one context pointer per window (one per lane line). */
static buszero_context buszero_contexts[sizeof(zero_windows) /
                                        sizeof(zero_windows[0])];

semu_status semu_ulsan_buszero_map(semu_bus *bus, semu_error *error)
{
    size_t index;

    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan bus-zero needs a bus");
        return SEMU_ERR_ARGUMENT;
    }
    for (index = 0u; index < sizeof(zero_windows) / sizeof(zero_windows[0]);
         ++index) {
        buszero_contexts[index].window = &zero_windows[index];
        if (semu_bus_map_device(bus, zero_windows[index].name,
                                zero_windows[index].base,
                                zero_windows[index].size, &buszero_ops,
                                &buszero_contexts[index], error) != SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_STATE;
        }
    }
    return SEMU_OK;
}
