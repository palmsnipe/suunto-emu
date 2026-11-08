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

#include "ulsan_iom4_endpoints.h"

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

static semu_ulsan_iom4 iom4_instance;

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
#define IOM4_DMA_ENABLE 0x1u
#define IOM4_DMA_DIR_MEM_TO_DEV 0x2u
#define IOM4_SRAM_START 0x10000000u
#define IOM4_SRAM_END 0x10267000u
#define IOM4_FLASH_START 0x00010000u
#define IOM4_FLASH_END 0x00200000u
#define IOM4_INTSTAT_FIFO_THRESHOLD (UINT32_C(1) << 1)
#define IOM4_FIFO_CAPACITY 32u
#define IOM4_INTSTAT_COMMAND_COMPLETE (UINT32_C(1) << 0)
#define IOM4_INTSTAT_DMA_COMPLETE (UINT32_C(1) << 10)
#define IOM4_INTSTAT_DMA_ERROR (UINT32_C(1) << 11)
#define IOM4_DMA_STATUS_COMPLETE 2u
#define IOM4_DMA_STATUS_ERROR 4u
#define IOM4_RACE_S_ACTUATOR 0x50u

/* E-ULS-0100: IOM4 delivers a waveform as one four-byte FIFO word
 * followed by a one-byte selector. Register 0x01 stays at its reset value
 * because none of the admitted writes modify it. */
static int iom4_actuator_write_allowed(uint32_t command, const uint8_t *data)
{
    switch (command) {
    case 0x501u:
        return data[0] == 0x40u && data[1] <= 0x20u &&
               (data[4] == 0u || data[4] == 1u || data[4] == 3u ||
                data[4] == 9u || data[4] == 0x0au || data[4] == 0x0du);
    case 0x201u:
        switch (data[0]) {
        case 0x09u:
            return data[1] == 0u || data[1] == 0x12u || data[1] == 0x32u ||
                   data[1] == 0x40u || data[1] == 0xb2u;
        case 0x0bu: return data[1] == 0x1au;
        case 0x0du: return data[1] == 1u;
        case 0x11u: return data[1] == 2u;
        case 0x12u: return data[1] == 0x6fu;
        case 0x13u: return data[1] == 0xa3u;
        default: return 0;
        }
    default: return 0;
    }
}

static int iom4_target_allowed(semu_ulsan_iom4 *s, uint32_t command,
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
static void iom4_update_irq(semu_ulsan_iom4 *s)
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

static void iom4_dma_mark_error(semu_ulsan_iom4 *s)
{
    s->dma_status = IOM4_DMA_STATUS_ERROR;
    s->interrupt_status |= IOM4_INTSTAT_DMA_ERROR;
    iom4_update_irq(s);
}

static void iom4_doorbell_data_phase(semu_ulsan_iom4 *s, uint32_t command)
{
    uint32_t kind = command & 0xFu;
    uint32_t size = (command >> 8) & 0xFFFu;
    uint32_t count = s->dma_total < size ? s->dma_total : size;
    uint8_t payload[4096];
    const int actuator = s->race_s_244 && s->actuator_enabled &&
                         s->i2c_device == IOM4_RACE_S_ACTUATOR;
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
    if (s->race_s_244 && (count > IOM4_FIFO_CAPACITY ||
                         to_device != (kind == 1u))) {
        iom4_dma_mark_error(s);
        return;
    }
    if (actuator && (count != size || count != s->dma_total ||
                     (!to_device && command != 0x01000112u))) {
        iom4_dma_mark_error(s);
        return;
    }
    if (!actuator && !ulsan_iom4_endpoint_known(s->i2c_device)) {
        return; /* endpoint beyond the observed traffic: no transfer */
    }

    if (to_device && s->race_s_244) {
        /* Source reads must come from mapped memory, without MMIO callbacks. */
        for (i = 0u; i < count; ++i) {
            if (semu_bus_copy_out(s->bus, s->dma_target + i, &payload[i], 1u,
                                  &error) != SEMU_OK) {
                iom4_dma_mark_error(s);
                return;
            }
        }
        if (actuator) {
            if (!iom4_actuator_write_allowed(command, payload)) {
                iom4_dma_mark_error(s);
                return;
            }
        } else {
            ulsan_iom4_endpoint_write(&s->endpoints, s->i2c_device, payload, count);
        }
    } else if (to_device) {
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
        ulsan_iom4_endpoint_write(&s->endpoints, s->i2c_device, payload, count);
    } else {
        uint32_t offset_enable = command & 0x10u; /* OFFSETEN bit 4 */
        /* Validate the full destination before writing the endpoint out. */
        for (i = 0u; i < count; i++) {
            uint32_t probe;
            semu_status status = s->race_s_244
                ? semu_bus_validate_write(s->bus, s->dma_target + i, 1u, &error)
                : semu_bus_read(s->bus, s->dma_target + i, 1u, &probe, &error);
            if (status != SEMU_OK) {
                iom4_dma_mark_error(s);
                return;
            }
        }
        if (actuator) {
            payload[0] = 0u;
        } else {
            if (offset_enable != 0u) {
                /* Upstream sends OFFSETLO first. The recorder selects it
                 * and the gauge takes it as its register pointer. */
                uint8_t offset_byte = (uint8_t)((command >> 24) & 0xFFu);
                ulsan_iom4_endpoint_write(&s->endpoints, s->i2c_device, &offset_byte, 1u);
            }
            ulsan_iom4_endpoint_read(&s->endpoints, s->i2c_device, payload, count);
        }
        for (i = 0u; i < count; i++) {
            uint32_t status;
            status = semu_bus_write(s->bus, s->dma_target + i, 1u,
                                    payload[i], &error);
            if (status != SEMU_OK) {
                iom4_dma_mark_error(s);
                return;
            }
        }
    }
    if (!actuator) ulsan_iom4_endpoint_finish(&s->endpoints, s->i2c_device);
    s->dma_config &= ~IOM4_DMA_ENABLE; /* wrapper CompleteDma auto-clear */
    s->dma_status = IOM4_DMA_STATUS_COMPLETE;
    s->interrupt_status |= IOM4_INTSTAT_DMA_COMPLETE |
                           IOM4_INTSTAT_COMMAND_COMPLETE;
    if (s->race_s_244) {
        /* E-ULS-0067: completed startup FIFO transfers report 0x403. */
        s->interrupt_status |= IOM4_INTSTAT_FIFO_THRESHOLD;
    }
    iom4_update_irq(s);
}

static semu_status iom4_read(void *context, uint32_t offset, unsigned width,
                             uint32_t *value, semu_error *error)
{
    semu_ulsan_iom4 *s = (semu_ulsan_iom4 *)context;
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
    case 0x104u: *value = s->fifo_threshold; return SEMU_OK;
    case 0x118u: *value = s->io_clock; return SEMU_OK;
    case 0x11Cu:
        *value = s->submodule | IOM4_SUBMODULE_READ_BITS;
        return SEMU_OK;
    case 0x200u: *value = s->interrupts_enable; return SEMU_OK;
    /* INTSTAT: read-only status the vector-10 handler polls
     * (E-ULS-0030, helper 0x0015e7a6 reads base+0x204). */
    case 0x204u: *value = s->interrupt_status; return SEMU_OK;
    case 0x210u: *value = s->dma_trigger; return SEMU_OK;
    case 0x128u: *value = s->offset_high; return SEMU_OK;
    case 0x124u: *value = s->dcx_control; return SEMU_OK;
    case 0x120u: *value = s->command; return SEMU_OK;
    case 0x2C4u: *value = s->i2c_device; return SEMU_OK;
    case 0x218u: *value = s->dma_config; return SEMU_OK;
    case 0x21Cu: *value = s->dma_total; return SEMU_OK;
    case 0x220u: *value = s->dma_target; return SEMU_OK;
    case 0x248u:
        *value = s->module_err | IOM4_MODULE_STATUS_IDLE;
        return SEMU_OK;
    case 0x280u: *value = IOM4_SPI_CONFIG_RESET; return SEMU_OK;
    case 0x2C0u: *value = s->i2c_config; return SEMU_OK;
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
    semu_ulsan_iom4 *s = (semu_ulsan_iom4 *)context;
    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan IOM4 write at 0x%08x width %u is unsupported",
                       offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    switch (offset) {
    case 0x128u:
        s->offset_high = value; /* full OFFSETHI field */
        return SEMU_OK;
    case 0x124u:
        /* DCXSEL is a plain tag (drops); DCXEN bit 4 stores. */
        s->dcx_control = value & 0x10u;
        return SEMU_OK;
    case 0x120u:
        /* Transaction doorbell: every field but the reserved pair
         * stores, and the command drives the DMA data phase above
         * (E-ULS-0029). */
        s->command = value & 0xFF3FFFFFu;
        iom4_doorbell_data_phase(s, value & 0xFF3FFFFFu);
        return SEMU_OK;
    case 0x2C4u:
        /* DEVADDR[9:0] with the lane's 7-bit truncation (extended
         * addressing is never enabled in the observed window). */
        s->i2c_device = value & 0x7Fu;
        return SEMU_OK;
    case 0x218u:
        /* Lane wrapper owns this register and stores value & 0x303. */
        s->dma_config = value & 0x303u;
        return SEMU_OK;
    case 0x21Cu:
        /* Lane wrapper owns this register and stores value & 0xfff. */
        s->dma_total = value & 0xFFFu;
        return SEMU_OK;
    case 0x220u:
        /* Lane wrapper owns this register; 29-bit TARGADDR field. */
        s->dma_target = value & 0x1FFFFFFFu;
        return SEMU_OK;
    case 0x248u:
        s->module_err = value & 0x1u; /* ERR only */
        return SEMU_OK;
    case 0x104u:
        s->fifo_threshold = value & IOM4_FIFO_THRESHOLD_MASK;
        return SEMU_OK;
    case 0x118u:
        s->io_clock = value & IOM4_IO_CLOCK_MASK;
        return SEMU_OK;
    case 0x11Cu:
        s->submodule = value & IOM4_SUBMODULE_WRITE_MASK;
        /* Upstream write callback: enabling both modules clears both. */
        if ((s->submodule & IOM4_SUBMODULE_WRITE_MASK) ==
            IOM4_SUBMODULE_WRITE_MASK) {
            s->submodule = 0u;
        }
        return SEMU_OK;
    case 0x200u:
        s->interrupts_enable = value & IOM4_INTERRUPTS_MASK;
        iom4_update_irq(s);
        return SEMU_OK;
    case 0x208u:
        /* INTCLR: write-1-clear. The observed window cleared an
         * all-zero status (storeless-equivalent); the lane handler's
         * clear now re-evaluates the line (E-ULS-0030). */
        s->interrupt_status &= ~value;
        iom4_update_irq(s);
        return SEMU_OK;
    case 0x210u:
        s->dma_trigger = value & 0x3u;
        return SEMU_OK;
    case 0x2C0u:
        s->i2c_config = value & IOM4_I2C_CONFIG_MASK;
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
    semu_ulsan_iom4 *s = (semu_ulsan_iom4 *)context;
    s->fifo_threshold = 0u;
    s->io_clock = 0u;
    s->submodule = 0u;
    s->interrupts_enable = 0u;
    s->dma_trigger = 0u;
    s->i2c_config = 0u;
    s->module_err = 0u;
    s->offset_high = 0u;
    s->dcx_control = 0u;
    s->command = 0u;
    s->i2c_device = 0u;
    s->dma_config = 0u;
    s->dma_total = 0u;
    s->dma_target = 0u;
    s->dma_status = 0u;
    s->interrupt_status = 0u;
    ulsan_iom4_endpoints_reset(&s->endpoints);
    iom4_update_irq(s);
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
    iom4_reset(&iom4_instance);
    iom4_instance.bus = bus;
    return semu_bus_map_device(bus, "ulsan.iom4", IOM4_BASE, IOM4_SIZE,
                               &iom4_ops, &iom4_instance, error);
}

semu_status semu_ulsan_244_iom4_map(semu_bus *bus, semu_ulsan_iom4 *state,
                                    semu_apollo4_irq_fn sink, void *context,
                                    semu_error *error)
{
    semu_status status;
    if (state == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Ulsan IOM4 needs state");
        return SEMU_ERR_ARGUMENT;
    }
    status = semu_bus_map_device(bus, "ulsan.iom4", IOM4_BASE, IOM4_SIZE,
                                 &iom4_ops, state, error);
    if (status == SEMU_OK) {
        *state = (semu_ulsan_iom4){0};
        state->bus = bus;
        state->irq_sink = sink;
        state->irq_context = context;
        state->race_s_244 = 1;
        iom4_reset(state);
    }
    return status;
}
