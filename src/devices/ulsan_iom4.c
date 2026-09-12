/*
 * Ulsan 2.35.36 IOM4 block at 0x40054000 (ticket 730, E-ULS-0023).
 *
 * Boot touches these registers twice. The first burst stores 0x1010
 * to +0x104, 0x1D0E1301 to +0x118, and 0x0103F270 to +0x2C0, polls
 * +0x11C and reads the configuration and queue-status block. The second
 * pass (E-ULS-0025) stores 0x10 to +0x11C - enabling the I2C master
 * submodule - polls +0x248 for the IDLEST bit (0x00000004 while idle,
 * the lane state through the whole observed boot), and writes INTCLR
 * (+0x208), whose clear bits operate on the all-zero observed status.
 *
 * The lane answers all of these from the upstream IOMaster
 * register-collection semantics, and lane probes of the device confirm
 * every byte-exact value: reads are the written value masked to the
 * stored fields (tagged flags store, tags and reserved bits do not),
 * +0x11C carries the read-only submodule-type enum bits 0xE20, and
 * +0x280 keeps its 0x00200000 reset constant.
 *
 * This device reproduces the register-collection behaviour for exactly
 * the observed offsets (see the header for the mask table). All other
 * IOM4 addresses and widths refuse; the command-queue doorbell, the
 * I2C/SPI endpoints, and the DMA engine beyond these registers stay
 * unmodelled.
 */

#include "ulsan_iom4.h"

#include "semu/types.h"

#define IOM4_BASE 0x40054000u
#define IOM4_SIZE 0x1000u

/* Reset-time read-only bits proven by the lane probe. */
#define IOM4_SUBMODULE_READ_BITS UINT32_C(0x00000E20)
#define IOM4_SPI_CONFIG_RESET UINT32_C(0x00200000)

/* IOModuleStatus: IDLEST (bit 2) is set while the submodule state
 * machine sits idle - the lane state through the whole observed boot
 * window (lane probe: 0x00000004 at 1 s and after completion). */
#define IOM4_MODULE_STATUS_IDLE UINT32_C(0x00000004)

typedef struct {
    uint32_t fifo_threshold;   /* +0x104: FIFORTHR[5:0], FIFOWTHR[13:8] */
    uint32_t io_clock;         /* +0x118: reserved bits drop, rest stores */
    uint32_t submodule;        /* +0x11C: SMOD0EN bit 0, SMOD1EN bit 4   */
    uint32_t interrupts_enable; /* +0x200: INTENi[14:0]                  */
    uint32_t dma_trigger;      /* +0x210: store & 0x3 (wrapper masked)   */
    uint32_t i2c_config;       /* +0x2C0: 0x0000F373 field mask          */
    uint32_t module_err;       /* +0x248 bit 0 ERR (tagged flag stores)  */
} iom4_state;

static iom4_state iom4_instance;

#define IOM4_FIFO_THRESHOLD_MASK UINT32_C(0x00003F3F)
/* DIV3 is a tagged flag: the register collection still stores it. Only
 * reserved bits [1,8) and [13,16) drop out. */
#define IOM4_IO_CLOCK_MASK \
    (UINT32_C(0xFFFFFFFF) & ~(UINT32_C(0xFE) | (UINT32_C(0x7) << 13)))
#define IOM4_SUBMODULE_WRITE_MASK \
    (UINT32_C(0x1) | (UINT32_C(0x1) << 4))
#define IOM4_INTERRUPTS_MASK UINT32_C(0x00007FFF)
/* Store mask of the lane register collection: ADDRSZ, SDADLY,
 * MI2CRST, SCLENDLY, SDAENDLY hold; SMPCNT/STRDIS and reserved bits
 * are discarded. Proven byte-exactly by the lane idle probe: the
 * 0x0103F270 configuration write leaves the lane at 0x0000F270. */
#define IOM4_I2C_CONFIG_MASK \
    (UINT32_C(0x1) | (UINT32_C(0x3) << 4) | (UINT32_C(0x1) << 6) | \
     (UINT32_C(0xF) << 8) | (UINT32_C(0xF) << 12))

/* Writes to fully tagged/tagged-only registers store nothing (the lane
 * framework discards those bits); reads still answer 0. */
static int iom4_is_storeless_read_zero(uint32_t offset)
{
    return offset == 0x228u || offset == 0x22Cu || offset == 0x234u ||
           offset == 0x23Cu || offset == 0x240u || offset == 0x244u ||
           offset == 0x208u; /* write-1-clear over an all-zero status */
}

static semu_status iom4_read(void *context, uint32_t offset, unsigned width,
                             uint32_t *value, semu_error *error)
{
    (void)context;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan IOM4 read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan IOM4 read at 0x%08x width %u is unsupported",
                       offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    switch (offset) {
    case 0x104u: *value = iom4_instance.fifo_threshold; return SEMU_OK;
    case 0x118u: *value = iom4_instance.io_clock; return SEMU_OK;
    case 0x11Cu:
        *value = iom4_instance.submodule | IOM4_SUBMODULE_READ_BITS;
        return SEMU_OK;
    case 0x200u: *value = iom4_instance.interrupts_enable; return SEMU_OK;
    case 0x210u: *value = iom4_instance.dma_trigger; return SEMU_OK;
    case 0x248u:
        *value = iom4_instance.module_err | IOM4_MODULE_STATUS_IDLE;
        return SEMU_OK;
    case 0x280u: *value = IOM4_SPI_CONFIG_RESET; return SEMU_OK;
    case 0x2C0u: *value = iom4_instance.i2c_config; return SEMU_OK;
    default:
        if (iom4_is_storeless_read_zero(offset)) {
            *value = 0u;
            return SEMU_OK;
        }
        break;
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan IOM4 read at 0x%08x is unsupported (unobserved)",
                   offset);
    return SEMU_ERR_UNSUPPORTED;
}

static semu_status iom4_write(void *context, uint32_t offset,
                              unsigned width, uint32_t value,
                              semu_error *error)
{
    (void)context;
    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan IOM4 write at 0x%08x width %u is unsupported",
                       offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    switch (offset) {
    case 0x248u:
        iom4_instance.module_err = value & 0x1u; /* ERR only */
        return SEMU_OK;
    case 0x104u:
        iom4_instance.fifo_threshold = value & IOM4_FIFO_THRESHOLD_MASK;
        return SEMU_OK;
    case 0x118u:
        iom4_instance.io_clock = value & IOM4_IO_CLOCK_MASK;
        return SEMU_OK;
    case 0x11Cu:
        iom4_instance.submodule = value & IOM4_SUBMODULE_WRITE_MASK;
        /* Upstream write callback: enabling both modules clears both. */
        if ((iom4_instance.submodule & IOM4_SUBMODULE_WRITE_MASK) ==
            IOM4_SUBMODULE_WRITE_MASK) {
            iom4_instance.submodule = 0u;
        }
        return SEMU_OK;
    case 0x200u:
        iom4_instance.interrupts_enable = value & IOM4_INTERRUPTS_MASK;
        return SEMU_OK;
    case 0x210u:
        iom4_instance.dma_trigger = value & 0x3u;
        return SEMU_OK;
    case 0x2C0u:
        iom4_instance.i2c_config = value & IOM4_I2C_CONFIG_MASK;
        return SEMU_OK;
    default:
        if (iom4_is_storeless_read_zero(offset) || offset == 0x280u) {
            /* Tagged-only stores: framework discards every bit. */
            return SEMU_OK;
        }
        break;
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan IOM4 write at 0x%08x is unsupported (unobserved)",
                   offset);
    return SEMU_ERR_UNSUPPORTED;
}

static void iom4_reset(void *context)
{
    (void)context;
    iom4_instance.fifo_threshold = 0u;
    iom4_instance.io_clock = 0u;
    iom4_instance.submodule = 0u;
    iom4_instance.interrupts_enable = 0u;
    iom4_instance.dma_trigger = 0u;
    iom4_instance.i2c_config = 0u;
    iom4_instance.module_err = 0u;
}

static const semu_bus_device_ops iom4_ops = {
    iom4_read,
    iom4_write,
    iom4_reset
};

semu_status semu_ulsan_iom4_map(semu_bus *bus, semu_error *error)
{
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan IOM4 needs a bus");
        return SEMU_ERR_ARGUMENT;
    }
    iom4_reset(NULL);
    return semu_bus_map_device(bus, "ulsan.iom4", IOM4_BASE, IOM4_SIZE,
                               &iom4_ops, &iom4_instance, error);
}
