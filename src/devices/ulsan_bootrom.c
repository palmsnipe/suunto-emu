/*
 * Ulsan 2.35.36 bootrom stub block at 0x08000000 and BootromLogger at
 * 0x07FFFFFC (ticket 730, E-ULS-0009).
 *
 * The reference lane's bootrom is Memory.MappedMemory (0x08000000, size
 * 0x1000) whose bytes come exclusively from the init list declared in the
 * platform description; everything else in the block is zero there. The
 * table below is that declaration in halfword form (the declared values
 * are listed verbatim in the evidence entry): handler at 0x30 (writes the
 * caller's LR to the logger address at 0x48 = 0x07FFFFFC and branches to
 * it), unimplemented-function thunks bl'ing the handler at 0x4C/0x50/
 * 0x54/0x58/0x78/0x80/0x98, program_main2 dispatch at 0x6C..0x72 with its
 * two bodies at 0x200/0x220, read_word at 0x74, and delay at 0x9C..0xA4
 * (the function the 2.35.36 boot calls at instruction 12,584,023 through
 * the fetch fault at 0x0800009c).
 *
 * Reads serve the declared halfwords (or zero, matching the lane's
 * zero-initialized MappedMemory bytes) for widths 1/2/4 at offsets aligned
 * to the access width. Writes refuse: the lane block is writable memory,
 * but no boot-phase write to it is observed, so an unobserved write fails
 * closed. Any BootromLogger access refuses with the attempted value in the
 * diagnostic (the handler stores the caller address there); in the lane an
 * access to that device aborts the simulation, and the in-tree refusal is
 * the deterministic abort-equivalent.
 */

#include "ulsan_bootrom.h"

#include "semu/types.h"

#define BOOTROM_BASE 0x08000000u
#define BOOTROM_SIZE 0x1000u
#define LOGGER_BASE 0x07FFFFFCu

typedef struct {
    uint32_t offset;
    uint16_t half;
} bootrom_half;

/* All WriteWord/WriteDoubleWord values from the lane declaration, split
 * into little-endian halfwords. */
static const bootrom_half bootrom_halves[] = {
    { 0x030u, UINT16_C(0xF8DF) }, { 0x032u, UINT16_C(0x3014) },
    { 0x034u, UINT16_C(0xF8C3) }, { 0x036u, UINT16_C(0xE000) },
    { 0x038u, UINT16_C(0x4718) },
    { 0x048u, UINT16_C(0xFFFC) }, { 0x04Au, UINT16_C(0x07FF) },
    { 0x04Cu, UINT16_C(0xF7FF) }, { 0x04Eu, UINT16_C(0xFFF0) },
    { 0x050u, UINT16_C(0xF7FF) }, { 0x052u, UINT16_C(0xFFEE) },
    { 0x054u, UINT16_C(0xF7FF) }, { 0x056u, UINT16_C(0xFFEC) },
    { 0x058u, UINT16_C(0xF7FF) }, { 0x05Au, UINT16_C(0xFFEA) },
    { 0x06Cu, UINT16_C(0x2900) }, { 0x06Eu, UINT16_C(0xD000) },
    { 0x070u, UINT16_C(0xE0D6) }, { 0x072u, UINT16_C(0xE0C5) },
    { 0x074u, UINT16_C(0x6800) }, { 0x076u, UINT16_C(0x4770) },
    { 0x078u, UINT16_C(0xF7FF) }, { 0x07Au, UINT16_C(0xFFDA) },
    { 0x080u, UINT16_C(0xF7FF) }, { 0x082u, UINT16_C(0xFFD6) },
    { 0x098u, UINT16_C(0xF7FF) }, { 0x09Au, UINT16_C(0xFFCA) },
    { 0x09Cu, UINT16_C(0x300F) }, { 0x09Eu, UINT16_C(0x3801) },
    { 0x0A0u, UINT16_C(0x2800) }, { 0x0A2u, UINT16_C(0xD1FC) },
    { 0x0A4u, UINT16_C(0x4770) },
    { 0x200u, UINT16_C(0x9800) }, { 0x202u, UINT16_C(0x0080) },
    { 0x204u, UINT16_C(0x009B) }, { 0x206u, UINT16_C(0x501A) },
    { 0x208u, UINT16_C(0x3804) }, { 0x20Au, UINT16_C(0x2800) },
    { 0x20Cu, UINT16_C(0xDAFB) }, { 0x20Eu, UINT16_C(0x2000) },
    { 0x210u, UINT16_C(0x4770) },
    { 0x220u, UINT16_C(0x9800) }, { 0x222u, UINT16_C(0x0080) },
    { 0x224u, UINT16_C(0x009B) }, { 0x226u, UINT16_C(0x5811) },
    { 0x228u, UINT16_C(0x5019) }, { 0x22Au, UINT16_C(0x3804) },
    { 0x22Cu, UINT16_C(0x2800) }, { 0x22Eu, UINT16_C(0xDAFA) },
    { 0x230u, UINT16_C(0x2000) }, { 0x232u, UINT16_C(0x4770) }
};

static uint16_t bootrom_halfword(uint32_t offset)
{
    size_t index;

    for (index = 0u; index < sizeof(bootrom_halves) / sizeof(bootrom_halves[0]);
         ++index) {
        if (bootrom_halves[index].offset == offset) {
            return bootrom_halves[index].half;
        }
    }
    return UINT16_C(0);
}

static semu_status bootrom_read(void *context, uint32_t offset,
                                unsigned width, uint32_t *value,
                                semu_error *error)
{
    uint32_t result;

    (void)context;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan bootrom read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if ((width != 1u && width != 2u && width != 4u) ||
        (width == 4u && (offset & 3u) != 0u) ||
        (width == 2u && (offset & 1u) != 0u)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan bootrom read at 0x%08x width %u is unsupported",
                       offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    if (width == 1u) {
        uint16_t half = bootrom_halfword(offset & ~1u);
        result = (offset & 1u) != 0u ? (uint32_t)(half >> 8)
                                     : (uint32_t)(half & 0xFFu);
    } else if (width == 2u) {
        result = bootrom_halfword(offset);
    } else {
        result = (uint32_t)bootrom_halfword(offset) |
                 ((uint32_t)bootrom_halfword(offset + 2u) << 16);
    }
    *value = result;
    return SEMU_OK;
}

static semu_status bootrom_write(void *context, uint32_t offset,
                                 unsigned width, uint32_t value,
                                 semu_error *error)
{
    (void)context;
    (void)value;
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan bootrom write at 0x%08x width %u is unsupported",
                   offset, width);
    return SEMU_ERR_UNSUPPORTED;
}

static void bootrom_reset(void *context)
{
    (void)context;
}

static const semu_bus_device_ops bootrom_ops = {
    bootrom_read,
    bootrom_write,
    bootrom_reset
};

/* Any BootromLogger access refuses; the handler stores the caller address
 * as the write value, which the diagnostic reports. */
static semu_status logger_read(void *context, uint32_t offset,
                               unsigned width, uint32_t *value,
                               semu_error *error)
{
    (void)context;
    (void)offset;
    (void)value;
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan bootrom logger read at width %u is unsupported",
                   width);
    return SEMU_ERR_UNSUPPORTED;
}

static semu_status logger_write(void *context, uint32_t offset,
                                unsigned width, uint32_t value,
                                semu_error *error)
{
    (void)context;
    (void)offset;
    (void)width;
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan bootrom logger invoked by caller 0x%08x", value);
    return SEMU_ERR_UNSUPPORTED;
}

static const semu_bus_device_ops logger_ops = {
    logger_read,
    logger_write,
    bootrom_reset
};

static int bootrom_context;

semu_status semu_ulsan_bootrom_map(semu_bus *bus, semu_error *error)
{
    semu_status status;

    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Ulsan bootrom needs a bus");
        return SEMU_ERR_ARGUMENT;
    }
    status = semu_bus_map_device(bus, "ulsan.bootrom", BOOTROM_BASE,
                                 BOOTROM_SIZE, &bootrom_ops,
                                 &bootrom_context, error);
    if (status != SEMU_OK) {
        return status;
    }
    return semu_bus_map_device(bus, "ulsan.bootrom_logger", LOGGER_BASE, 4u,
                               &logger_ops, &bootrom_context, error);
}
