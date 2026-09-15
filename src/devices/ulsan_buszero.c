/*
 * Ulsan 2.35.36 lane-recorded bus-zero accesses (ticket 730,
 * E-ULS-0013/0016/0038).
 *
 * See ulsan_buszero.h. The reference-lane sysbus answers every access
 * below the peripheral registry with its non-existing-peripheral
 * fallback: the read logs "Read<T> from non existing peripheral at
 * 0x..." and returns the untagged default 0 (SystemBus.cs
 * ReportNonExistingRead `tag?.DefaultValue ?? default(ulong)`, tag
 * null - the log lines carry no tag), and the write logs the value and
 * discards it. Each tabled block window lists exactly the
 * (offset, width[, write value]) tuples with reproduced lane log lines;
 * everything else refuses.
 *
 * Blocks: CRYPTO <0x400C0000,0x400C3FFF> read at +0xFE0 (E-ULS-0013);
 * the logger port at 0x40000000 (E-ULS-0016); the unregistered 0x400B0
 * I2C0 block and the 0x400B2xxx pair (E-ULS-0038, twelve read tuples
 * and nine write tuples, identical in both reference runs).
 */

#include "ulsan_buszero.h"

#include "semu/types.h"
#include <stddef.h>

typedef struct {
    uint32_t offset; /* window-relative address                      */
    unsigned width;  /* recorded access width (1/2/4)                */
    int is_write;    /* 0: read returned value; 1: write discarded   */
    uint32_t value;  /* read: returned value; write: logged value    */
} zero_access;

typedef struct {
    uint32_t base;   /* window start                                        */
    uint32_t size;   /* window size (routing region; answers stay per-line) */
    const zero_access *accesses;
    unsigned count;
    const char *name;
} zero_window;

static const zero_access crypto_accesses[] = {
    { 0xFE0u, 4u, 0, UINT32_C(0) } /* logged read, returning 0x00000000 */
};

static const zero_access logger_accesses[] = {
    { 0x0u, 4u, 0, UINT32_C(0) }, /* probe-pinned lane read value       */
    { 0x0u, 4u, 1, UINT32_C(2) }  /* the lane-discarded logged write    */
};

/*
 * 0x400B0000 I2C0 block (E-ULS-0038): the ten 0x0C writes are five
 * rising 0x10000-step command values bracketed by zero writes, the
 * +0x01/+0x0B byte writes are the guest's read-modify-write results
 * over the zero reads (guest disassembly at 0x000f383c/0x000f3e7a
 * pins each written value against the lane line).
 */
static const zero_access i2c0_accesses[] = {
    { 0x00u, 4u, 0, UINT32_C(0) },
    { 0x01u, 1u, 0, UINT32_C(0) },
    { 0x02u, 2u, 0, UINT32_C(0) },
    { 0x04u, 1u, 0, UINT32_C(0) },
    { 0x04u, 4u, 0, UINT32_C(0) },
    { 0x08u, 4u, 0, UINT32_C(0) },
    { 0x0Au, 1u, 0, UINT32_C(0) },
    { 0x0Bu, 1u, 0, UINT32_C(0) },
    { 0x0Cu, 4u, 0, UINT32_C(0) },
    { 0x10u, 4u, 0, UINT32_C(0) },
    { 0x14u, 4u, 0, UINT32_C(0) },
    { 0x18u, 4u, 0, UINT32_C(0) },
    { 0x01u, 1u, 1, UINT32_C(0x00) },
    { 0x01u, 1u, 1, UINT32_C(0x40) },
    { 0x0Bu, 1u, 1, UINT32_C(0x04) },
    { 0x0Cu, 4u, 1, UINT32_C(0x00000) },
    { 0x0Cu, 4u, 1, UINT32_C(0x10000) },
    { 0x0Cu, 4u, 1, UINT32_C(0x20000) },
    { 0x0Cu, 4u, 1, UINT32_C(0x30000) },
    { 0x0Cu, 4u, 1, UINT32_C(0x40000) },
    { 0x0Cu, 4u, 1, UINT32_C(0x50000) }
};

/* 0x400B2xxx pair (E-ULS-0038): three lane lines only - the +0x2000
 * read/write pair and a write-only word at +0x2024 (no logged read of
 * it, so it stays read-refused). */
static const zero_access periph_b2000_accesses[] = {
    { 0x0u, 4u, 0, UINT32_C(0) },
    { 0x0u, 4u, 1, UINT32_C(0x2000000) }
};

static const zero_access periph_b2024_accesses[] = {
    { 0x0u, 4u, 1, UINT32_C(0x80000000) }
};

static const zero_window zero_windows[] = {
    { 0x400C0000u, 0x4000u, crypto_accesses,
      sizeof(crypto_accesses) / sizeof(crypto_accesses[0]),
      "ulsan.buszero.crypto" },
    { 0x40000000u, 0x4u, logger_accesses,
      sizeof(logger_accesses) / sizeof(logger_accesses[0]),
      "ulsan.buszero.logger" },
    { 0x400B0000u, 0x20u, i2c0_accesses,
      sizeof(i2c0_accesses) / sizeof(i2c0_accesses[0]),
      "ulsan.buszero.i2c0" },
    { 0x400B2000u, 0x4u, periph_b2000_accesses,
      sizeof(periph_b2000_accesses) / sizeof(periph_b2000_accesses[0]),
      "ulsan.buszero.periph_400b2000" },
    { 0x400B2024u, 0x4u, periph_b2024_accesses,
      sizeof(periph_b2024_accesses) / sizeof(periph_b2024_accesses[0]),
      "ulsan.buszero.periph_400b2024" }
};

typedef struct {
    const zero_window *window;
} buszero_context;

static const zero_access *buszero_find(const buszero_context *ctx,
                                       uint32_t offset, unsigned width,
                                       int is_write, uint32_t value,
                                       unsigned start)
{
    unsigned index;
    for (index = start; index < ctx->window->count; ++index) {
        const zero_access *acc = &ctx->window->accesses[index];
        if (acc->offset == offset && acc->width == width &&
            acc->is_write == is_write &&
            (!is_write || acc->value == value)) {
            return acc;
        }
    }
    return NULL;
}

static semu_status buszero_read(void *context, uint32_t offset,
                                unsigned width, uint32_t *value,
                                semu_error *error)
{
    const buszero_context *ctx = (const buszero_context *)context;
    const zero_access *acc;

    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan bus-zero read value required");
        return SEMU_ERR_ARGUMENT;
    }
    acc = buszero_find(ctx, offset, width, 0, 0u, 0u);
    if (acc != NULL) {
        *value = acc->value; /* the lane fallback returned 0 */
        return SEMU_OK;
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan bus-zero read at 0x%08x+%x width %u is "
                   "unsupported", ctx->window->base, offset, width);
    return SEMU_ERR_UNSUPPORTED;
}

static semu_status buszero_write(void *context, uint32_t offset,
                                 unsigned width, uint32_t value,
                                 semu_error *error)
{
    const buszero_context *ctx = (const buszero_context *)context;

    if (buszero_find(ctx, offset, width, 1, value, 0u) != NULL) {
        return SEMU_OK; /* the lane sysbus logged and discarded this one */
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan bus-zero write at 0x%08x+%x width %u value "
                   "0x%08x is unsupported",
                   ctx->window->base, offset, width, value);
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

/* Stable storage for one context pointer per window (one per lane block). */
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
