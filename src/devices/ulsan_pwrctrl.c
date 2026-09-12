/*
 * Ulsan 2.35.36 power-control block at 0x40021000 (ticket 730, E-ULS-0008).
 *
 * Observed boot transactions, all 32-bit (reference-lane trace lines for
 * 2.35.36 run1/run2, identical, and the in-tree fault record):
 *   writes 0x04 = 0x00008000 (PWRENMSPI1), 0x00040000 (PWRENDISP),
 *           0x00080000 (PWRENDISPPHY), 0x00400000 (PWRENUSB);
 *           0x24 = 0x3 (PWRENSSRAM), 0x58 = 0x1 (PWRENDSP0RAM),
 *           0x60 = 0x4 (ICACHEPWDDSP0OFF), 0x78 = 0x1 (PWRENDSP1RAM),
 *           0x80 = 0x4 (ICACHEPWDDSP1OFF), 0x100 = 0x1 (SIMOBUCKEN).
 *     Every lane log line names the complete written value as unhandled
 *     bits, so the reference model applied no state change to any of them;
 *     this device accepts exactly the pairs below and stores nothing. The
 *     two extra pairs (0x14,0x3F) and (0x1C,0x08) are the read-modify-
 *     write stores the guest itself computes from the probed constant
 *     register values (observed in an in-tree instrumented trace).
 *   reads  the observed offsets below. The boot sampler at 0x00096b5c
 *           (called from 0x0009d20a) loads 0x00096b66 and keeps bit 20;
 *           the reference lane stores the consumed result into guest RAM
 *           0x1005b079 as 0x01, so bit 20 reads back set. The lane dumps
 *           the whole block at boot-time shim entries (lane-probe6) with
 *           shim entries, reproduced in the observed_reads table below; the
 *           0x14/0x18/0x1C trio is
 *           unchanged across the guest read-modify-write chains at
 *           0x000965ec/0x000966f8 and at run end, so they read as fixed
 *           values in this phase. 0x04/0x08 hold 0x100000 during boot and
 *           read 0 only after the lane has idled; boot never reads them
 *           after that point. Reads of unobserved offsets refuse.
 *
 * Anything else fails closed with a bounded diagnostic: values other than
 * the observed pairs are refused at their offsets, offsets outside the
 * observed tables refuse, and only 32-bit aligned accesses are valid. The
 * block is stateless: reads never depend on prior writes, matching the
 * lane's observed no-write-state behavior.
 */

#include "ulsan_pwrctrl.h"

#include "semu/types.h"

enum {
    OFFSET_DEVICE_POWER_ENABLE = 0x04u,
    OFFSET_DEVICE_POWER_STATUS = 0x08u,
    OFFSET_SHARED_SRAM_ENABLE = 0x24u,
    OFFSET_DSP0_MEMORY_ENABLE = 0x58u,
    OFFSET_DSP0_MEMORY_RETENTION = 0x60u,
    OFFSET_DSP1_MEMORY_ENABLE = 0x78u,
    OFFSET_DSP1_MEMORY_RETENTION = 0x80u,
    OFFSET_SIMO_BUCK_ENABLE = 0x100u,
    OFFSET_LEGACY_STATUS_14 = 0x14u,
    OFFSET_LEGACY_STATUS_18 = 0x18u,
    OFFSET_LEGACY_CONTROL_1C = 0x1cu
};

typedef struct {
    uint32_t offset;
    uint32_t value;
} observed_write;

static const observed_write observed_writes[] = {
    /* Zero write at the continuation branch 0x00096a32: the lane logs no
     * unhandled bits for a valueless write and its idle dump holds 0. */
    { OFFSET_DEVICE_POWER_ENABLE, UINT32_C(0x00000000) },
    /* Lane write-log lines for the boot continuation (values as logged):
     * 0x58 bit 0 PWRENDSP0RAM, 0x60 bit 2 ICACHEPWDDSP0OFF, 0x78 bit 0
     * PWRENDSP1RAM, 0x80 bit 2 ICACHEPWDDSP1OFF. */
    { 0x58u, UINT32_C(0x00000001) },
    { 0x58u, UINT32_C(0x00000000) }, /* clear of the logged bit        */
    { 0x60u, UINT32_C(0x00000004) },
    { 0x60u, UINT32_C(0x00000000) }, /* clear of the logged bit        */
    { 0x78u, UINT32_C(0x00000001) },
    { 0x78u, UINT32_C(0x00000000) }, /* clear of the logged bit        */
    { 0x80u, UINT32_C(0x00000004) },
    { 0x80u, UINT32_C(0x00000000) }, /* clear of the logged bit        */
    { OFFSET_DEVICE_POWER_ENABLE, UINT32_C(0x00008000) },
    { OFFSET_DEVICE_POWER_ENABLE, UINT32_C(0x00040000) },
    { OFFSET_DEVICE_POWER_ENABLE, UINT32_C(0x00080000) },
    { OFFSET_DEVICE_POWER_ENABLE, UINT32_C(0x00400000) },
    { OFFSET_SHARED_SRAM_ENABLE, UINT32_C(0x00000003) },
    { OFFSET_DSP0_MEMORY_ENABLE, UINT32_C(0x00000001) },
    { OFFSET_DSP0_MEMORY_RETENTION, UINT32_C(0x00000004) },
    { OFFSET_DSP1_MEMORY_ENABLE, UINT32_C(0x00000001) },
    { OFFSET_DSP1_MEMORY_RETENTION, UINT32_C(0x00000004) },
    { OFFSET_SIMO_BUCK_ENABLE, UINT32_C(0x00000001) },
    { 0x140u, UINT32_C(0x00000000) }, { 0x144u, UINT32_C(0x00000000) },
    { 0x148u, UINT32_C(0x00000000) }, { 0x14Cu, UINT32_C(0x00000000) },
    { 0x150u, UINT32_C(0x00000000) }, { 0x154u, UINT32_C(0x00000000) },
    { 0x158u, UINT32_C(0x00000000) }, { 0x15Cu, UINT32_C(0x00000000) },
    { 0x160u, UINT32_C(0x00000000) }, { 0x164u, UINT32_C(0x00000000) },
    { 0x168u, UINT32_C(0x00000000) }, { 0x16Cu, UINT32_C(0x00000000) },
    { 0x170u, UINT32_C(0x00000000) }, { 0x174u, UINT32_C(0x00000000) },
    { 0x178u, UINT32_C(0x00000000) }, { 0x17Cu, UINT32_C(0x00000000) },
    { 0x180u, UINT32_C(0x00000000) }, { 0x184u, UINT32_C(0x00000000) },
    { 0x188u, UINT32_C(0x00000000) },
    { OFFSET_LEGACY_STATUS_14, UINT32_C(0x0000003F) },
    { OFFSET_LEGACY_CONTROL_1C, UINT32_C(0x00000008) }
};

typedef struct {
    uint32_t offset;
    uint32_t value;
} observed_read;

static const observed_read observed_reads[] = {
    { 0x00u, UINT32_C(0x00000009) },  /* performance control      */
    { 0x04u, UINT32_C(0x00100000) },  /* device power enable      */
    { 0x08u, UINT32_C(0x00100000) },  /* device power status      */
    { 0x0Cu, UINT32_C(0x00000000) },
    { 0x10u, UINT32_C(0x00000000) },
    { 0x14u, UINT32_C(0x0000003F) },  /* legacy status 1          */
    { 0x18u, UINT32_C(0x0000003F) },  /* legacy status 2          */
    { 0x1Cu, UINT32_C(0x00000008) },  /* legacy control           */
    { 0x24u, UINT32_C(0x00000000) },  /* shared sram enable       */
    { 0x28u, UINT32_C(0x00000003) },  /* shared sram status       */
    { 0x2Cu, UINT32_C(0x000003FC) },  /* shared sram retention    */
    { 0x58u, UINT32_C(0x00000000) },  /* dsp0 memory enable       */
    { 0x5Cu, UINT32_C(0x00000000) },  /* dsp0 memory status       */
    { 0x60u, UINT32_C(0x00000000) },
    { 0x78u, UINT32_C(0x00000000) },  /* dsp1 memory enable       */
    { 0x7Cu, UINT32_C(0x00000000) },  /* dsp1 memory status       */
    { 0x80u, UINT32_C(0x00000000) },
    { 0x100u, UINT32_C(0x00000000) }  /* simo buck enable         */
};

/*
 * The lane register framework answers 1-, 2- and 4-byte requests by
 * slicing the register word (proven: a 1-byte read at +0x4002105A
 * executes there with no log line). Byte lanes of an observed constant
 * read as that slice of the constant; writes of unproven widths keep
 * refusing until the lane log records one.
 */
static int validate_access(uint32_t offset, unsigned width,
                           const char *action, semu_error *error)
{
    if (width != 1u && width != 2u && width != 4u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan power control %s at 0x%08x width %u is "
                       "unsupported", action, offset, width);
        return 0;
    }
    if ((offset % width) != 0u || (offset & 3u) >= width * 4u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan power control %s at 0x%08x width %u is "
                       "unsupported", action, offset, width);
        return 0;
    }
    return 1;
}

static semu_status pwrctrl_read(void *context, uint32_t offset,
                                unsigned width, uint32_t *value,
                                semu_error *error)
{
    size_t index;

    (void)context;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan power control read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if (!validate_access(offset, width, "read", error)) {
        return SEMU_ERR_UNSUPPORTED;
    }
    for (index = 0u; index < sizeof(observed_reads) / sizeof(observed_reads[0]);
         ++index) {
        if (observed_reads[index].offset == (offset & ~3u)) {
            const uint32_t shift = (offset & 3u) * 8u;
            const uint32_t mask = (width == 4u)
                ? UINT32_C(0xFFFFFFFF)
                : ((UINT32_C(1) << (width * 8u)) - UINT32_C(1));
            *value = (observed_reads[index].value >> shift) & mask;
            return SEMU_OK;
        }
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan power control read at 0x%08x is unsupported",
                   offset);
    return SEMU_ERR_UNSUPPORTED;
}

static semu_status pwrctrl_write(void *context, uint32_t offset,
                                 unsigned width, uint32_t value,
                                 semu_error *error)
{
    size_t index;

    (void)context;
    if (!validate_access(offset, width, "write", error)) {
        return SEMU_ERR_UNSUPPORTED;
    }
    if (width != 4u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan power control write at 0x%08x width %u has "
                       "no lane-recorded value", offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    for (index = 0u; index < sizeof(observed_writes) / sizeof(observed_writes[0]);
         ++index) {
        if (observed_writes[index].offset == offset &&
            observed_writes[index].value == value) {
            /* The reference model reported every one of these writes as
             * fully unhandled bits: accepted, stored nowhere. */
            return SEMU_OK;
        }
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan power control write 0x%08x at 0x%08x is "
                   "unsupported", value, offset);
    return SEMU_ERR_UNSUPPORTED;
}

static void pwrctrl_reset(void *context)
{
    (void)context;
}

static const semu_bus_device_ops pwrctrl_ops = {
    pwrctrl_read,
    pwrctrl_write,
    pwrctrl_reset
};

/* Stateless context: the mapping carries no mutable data. */
static int pwrctrl_context;

semu_status semu_ulsan_pwrctrl_map(semu_bus *bus, semu_error *error)
{
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan power control needs a bus");
        return SEMU_ERR_ARGUMENT;
    }
    return semu_bus_map_device(bus, "ulsan.pwrctrl", 0x40021000u, 0x400u,
                               &pwrctrl_ops, &pwrctrl_context, error);
}
