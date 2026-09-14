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
 * A later pass stores 0 to OFFSETHI (+0x128), the transaction address
 * high half (a plain 32-bit value field), then 0x28 to
 * I2CDeviceConfiguration (+0x2C4) - the 7-bit-trapped DEVADDR value
 * 0x28 selecting the target device (E-ULS-0026).
 *
 * The lane answers all of these from the upstream IOMaster
 * register-collection semantics, and lane probes of the device confirm
 * every byte-exact value: reads are the written value masked to the
 * stored fields (tagged flags store, tags and reserved bits do not),
 * +0x11C carries the read-only submodule-type enum bits 0xE20, and
 * +0x280 keeps its 0x00200000 reset constant.
 *
 * The next pass arms the transaction: the device address 0x28 to
 * +0x2C4, the SRAM buffer address to +0x220, 0x4 to +0x21C, and the
 * doorbell 0x401 on +0x120. The full-trace lane run (E-ULS-0029)
 * shows every boot doorbell running the wrapper DMA path: the +0x218
 * value-0 store is the end-of-transaction state, transactions enable
 * DMA (direction memory-to-device), load the count from the SRAM
 * target, and hand the payload to the register-file recorder behind
 * device address 0x28; reads (size 1, offset enabled, OFFSETLO in
 * bits 31:24) select a recorder register and copy its value back into
 * the DMA target. Completion auto-clears DMAEN in the +0x218 mirror
 * and moves the internal status bits; the guest never reads +0x204 or
 * +0x224 in the observed window, so those stay refused.
 *
 * This device reproduces the register-collection behaviour for exactly
 * the observed offsets (see the header for the mask table) plus the
 * observed doorbell data plane. All other IOM4 addresses and widths
 * refuse; targets beyond the wrapper's SRAM/flash bounds or endpoints
 * beyond the observed recorder accept the doorbell but transfer
 * nothing - the lane's own refusal path. The boot now runs until it
 * waits in WFI (12,611,224), where the missing interrupt plane - the
 * IOM4 command-complete line into NVIC IRQ 10 (E-ULS-0028) - takes
 * over.
 */

#include "ulsan_iom4.h"

#include <string.h>

#include "semu/bus.h"
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
    uint32_t offset_high;      /* +0x128: OFFSETHI full 32-bit value     */
    uint32_t dcx_control;      /* +0x124: DCXEN bit 4 (tag DCXSEL no)   */
    uint32_t command;          /* +0x120: CMD|OFFSETCNT|CONT|TSIZE|
                                *         CMDSEL|OFFSETLO (no [22,24))  */
    uint32_t i2c_device;       /* +0x2C4: DEVADDR[6:0] (7-bit truncated) */
    uint32_t dma_config;       /* +0x218: DMAEN|DMADIR|DMAPRI|DPWROFF 0x303 */
    uint32_t dma_total;        /* +0x21C: TOTCOUNT[11:0]                */
    uint32_t dma_target;       /* +0x220: TARGADDR[28:0]                */
    uint32_t dma_status;       /* +0x224 mirror: never read in the
                                * observed window (refused)           */
    uint32_t interrupt_status; /* INTSTAT bits, set by completions;
                                * +0x204 stays refused (unobserved)   */
    uint8_t endpoint_selected; /* 0x28 recorder select register       */
    uint8_t endpoint_regs[256];/* 0x28 recorder register file         */
    semu_bus *bus;             /* self-reference for DMA payloads     */
    semu_apollo4_irq_fn irq_sink;   /* machine line sink (IRQ 10)   */
    void *irq_context;              /* sink context                 */
    unsigned irq_level;             /* last driven line level       */
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


/* Doorbell data plane (E-ULS-0029). The lane wrapper (Apollo4IomDma)
 * arms DMA from +0x218 bit 0 (direction bit 1), +0x21C count, +0x220
 * target; every boot transaction targets the recorder at device
 * address 0x28 through a four-byte payload. Refusal rules and the
 * completion side effects mirror the wrapper source and trace. */
#define IOM4_END_POINT 0x28u
#define IOM4_DMA_ENABLE 0x1u
#define IOM4_DMA_DIR_MEM_TO_DEV 0x2u
#define IOM4_SRAM_START 0x10000000u
#define IOM4_SRAM_END 0x10267000u
#define IOM4_FLASH_START 0x00010000u
#define IOM4_FLASH_END 0x00200000u
#define IOM4_INTSTAT_COMMAND_COMPLETE (UINT32_C(1) << 0)
#define IOM4_INTSTAT_DMA_COMPLETE (UINT32_C(1) << 10)
#define IOM4_INTSTAT_DMA_ERROR (UINT32_C(1) << 11)
#define IOM4_DMA_STATUS_COMPLETE 2u
#define IOM4_DMA_STATUS_ERROR 4u

static int iom4_target_allowed(iom4_state *s, uint32_t command,
                               uint32_t count, int *to_device)
{
    uint64_t end = (uint64_t)s->dma_target + count;
    *to_device = (s->dma_config & IOM4_DMA_DIR_MEM_TO_DEV) != 0u;
    if (end >= (uint64_t)IOM4_SRAM_START && end <= (uint64_t)IOM4_SRAM_END &&
        s->dma_target >= IOM4_SRAM_START) {
        return 1;
    }
    /* The wrapper only allows flash as a write-direction source. */
    if (*to_device && end >= (uint64_t)IOM4_FLASH_START &&
        end <= (uint64_t)IOM4_FLASH_END) {
        return 1;
    }
    (void)command;
    return 0;
}

/* Upstream drives the line as OR(status & enable); the lane trace
 * (E-ULS-0030) shows assert at completion and deassert inside the
 * handler's INTCLR write. Transitions are edge-only notifications. */
static void iom4_update_irq(iom4_state *s)
{
    unsigned level = (s->interrupt_status & s->interrupts_enable) != 0u;
    if (level == s->irq_level) {
        return;
    }
    s->irq_level = level;
    if (s->irq_sink != NULL) {
        s->irq_sink(s->irq_context, 10u, level != 0u); /* E-ULS-0028 */
    }
}

static void iom4_dma_mark_error(iom4_state *s)
{
    s->dma_status = IOM4_DMA_STATUS_ERROR;
    s->interrupt_status |= IOM4_INTSTAT_DMA_ERROR;
    iom4_update_irq(s);
}

static void iom4_doorbell_data_phase(iom4_state *s, uint32_t command)
{
    uint32_t kind = command & 0xFu;
    uint32_t size = (command >> 8) & 0xFFFu;
    uint32_t count = s->dma_total < size ? s->dma_total : size;
    uint8_t payload[4096];
    int to_device = 0;
    unsigned i;
    semu_error error;

    if ((s->dma_config & IOM4_DMA_ENABLE) == 0u ||
        (kind != 1u && kind != 2u)) {
        return; /* wrapper PrepareDma decline: plain register store */
    }
    if (count == 0u || !iom4_target_allowed(s, command, count, &to_device)) {
        iom4_dma_mark_error(s); /* wrapper DmaError path */
        return;
    }
    if (s->i2c_device != IOM4_END_POINT) {
        return; /* endpoint beyond the observed recorder: no transfer */
    }

    if (to_device) {
        /* Validate the full SRAM source before touching the endpoint. */
        for (i = 0u; i < count; i++) {
            uint32_t byte;
            if (semu_bus_read(s->bus, s->dma_target + i, 1u, &byte,
                              &error) != SEMU_OK) {
                iom4_dma_mark_error(s);
                return;
            }
            payload[i] = (uint8_t)byte;
        }
        s->endpoint_selected = payload[0];
        for (i = 1u; i < count; i++) {
            s->endpoint_regs[(uint8_t)(s->endpoint_selected + i - 1u)] =
                payload[i];
        }
    } else {
        uint32_t offset_enable = command & 0x10u; /* OFFSETEN bit 4 */
        /* Validate the full destination before writing the endpoint out. */
        for (i = 0u; i < count; i++) {
            uint32_t probe;
            if (semu_bus_read(s->bus, s->dma_target + i, 1u, &probe,
                              &error) != SEMU_OK) {
                iom4_dma_mark_error(s);
                return;
            }
        }
        if (offset_enable != 0u) {
            /* Upstream sends the OFFSETLO byte to the endpoint as a
             * one-byte write first; the recorder selects on it without
             * storing (data length 1). */
            s->endpoint_selected = (uint8_t)((command >> 24) & 0xFFu);
        }
        for (i = 0u; i < count; i++) {
            uint32_t status;
            uint8_t byte =
                s->endpoint_regs[(uint8_t)(s->endpoint_selected + i)];
            status = semu_bus_write(s->bus, s->dma_target + i, 1u, byte,
                                    &error);
            if (status != SEMU_OK) {
                iom4_dma_mark_error(s);
                return;
            }
        }
    }
    s->dma_config &= ~IOM4_DMA_ENABLE; /* wrapper CompleteDma auto-clear */
    s->dma_status = IOM4_DMA_STATUS_COMPLETE;
    s->interrupt_status |= IOM4_INTSTAT_DMA_COMPLETE |
                           IOM4_INTSTAT_COMMAND_COMPLETE;
    iom4_update_irq(s);
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
    /* INTSTAT: read-only status the vector-10 handler polls
     * (E-ULS-0030, helper 0x0015e7a6 reads base+0x204). */
    case 0x204u: *value = iom4_instance.interrupt_status; return SEMU_OK;
    case 0x210u: *value = iom4_instance.dma_trigger; return SEMU_OK;
    case 0x128u: *value = iom4_instance.offset_high; return SEMU_OK;
    case 0x124u: *value = iom4_instance.dcx_control; return SEMU_OK;
    case 0x120u: *value = iom4_instance.command; return SEMU_OK;
    case 0x2C4u: *value = iom4_instance.i2c_device; return SEMU_OK;
    case 0x218u: *value = iom4_instance.dma_config; return SEMU_OK;
    case 0x21Cu: *value = iom4_instance.dma_total; return SEMU_OK;
    case 0x220u: *value = iom4_instance.dma_target; return SEMU_OK;
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
    case 0x128u:
        iom4_instance.offset_high = value; /* full OFFSETHI field */
        return SEMU_OK;
    case 0x124u:
        /* DCXSEL is a plain tag (drops); DCXEN bit 4 stores. */
        iom4_instance.dcx_control = value & 0x10u;
        return SEMU_OK;
    case 0x120u:
        /* Transaction doorbell: every field but the reserved pair
         * stores, and the command drives the DMA data phase above
         * (E-ULS-0029). */
        iom4_instance.command = value & 0xFF3FFFFFu;
        iom4_doorbell_data_phase(&iom4_instance, value & 0xFF3FFFFFu);
        return SEMU_OK;
    case 0x2C4u:
        /* DEVADDR[9:0] with the lane's 7-bit truncation (extended
         * addressing is never enabled in the observed window). */
        iom4_instance.i2c_device = value & 0x7Fu;
        return SEMU_OK;
    case 0x218u:
        /* Lane wrapper owns this register and stores value & 0x303. */
        iom4_instance.dma_config = value & 0x303u;
        return SEMU_OK;
    case 0x21Cu:
        /* Lane wrapper owns this register and stores value & 0xfff. */
        iom4_instance.dma_total = value & 0xFFFu;
        return SEMU_OK;
    case 0x220u:
        /* Lane wrapper owns this register; 29-bit TARGADDR field. */
        iom4_instance.dma_target = value & 0x1FFFFFFFu;
        return SEMU_OK;
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
        iom4_update_irq(&iom4_instance);
        return SEMU_OK;
    case 0x208u:
        /* INTCLR: write-1-clear. The observed window cleared an
         * all-zero status (storeless-equivalent); the lane handler's
         * clear now re-evaluates the line (E-ULS-0030). */
        iom4_instance.interrupt_status &= ~value;
        iom4_update_irq(&iom4_instance);
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
    iom4_instance.offset_high = 0u;
    iom4_instance.dcx_control = 0u;
    iom4_instance.command = 0u;
    iom4_instance.i2c_device = 0u;
    iom4_instance.dma_config = 0u;
    iom4_instance.dma_total = 0u;
    iom4_instance.dma_target = 0u;
    iom4_instance.dma_status = 0u;
    iom4_instance.interrupt_status = 0u;
    iom4_instance.endpoint_selected = 0u;
    memset(iom4_instance.endpoint_regs, 0, sizeof(iom4_instance.endpoint_regs));
    iom4_update_irq(&iom4_instance);
}

void semu_ulsan_iom4_set_irq_sink(semu_apollo4_irq_fn sink, void *context)
{
    iom4_instance.irq_sink = sink;
    iom4_instance.irq_context = context;
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
    iom4_instance.bus = bus;
    return semu_bus_map_device(bus, "ulsan.iom4", IOM4_BASE, IOM4_SIZE,
                               &iom4_ops, &iom4_instance, error);
}
