#include "sapporo_iom4_internal.h"

#include <stdlib.h>
#include <string.h>

/*
 * E-SAP-0036 lane-mirror law for the Sapporo-2.35.34 IOM4 at
 * 0x40054000. Proven by lane probe pairs iom4law 5a2f3974.., iom4law2
 * 43a639e3.., iom4law4 854d2ebd.. and the masked iom4law5 run (all
 * byte-identical x2) on the 2.35 machine, with the lane wrapper
 * SapporoApollo4Iom4.cs and upstream AmbiqApollo4_IOMaster.cs (master,
 * sha 164bf8a8..) as the structure reference. Offsets follow the
 * upstream Registers enum; 0x210..0x224 is the wrapper-owned DMA
 * window (the delegated controller never sees those offsets), command
 * completion is synchronous inside the 0x120 write, and the endpoints
 * are the 2.35 machine's observed device 0x28 (deterministic zeros)
 * plus MAX17050 fuel gauge 0x36 and E-SAP-0040 scoped haptic startup
 * at 0x50. Register dispatch lives in
 * sapporo_iom4_regs.c.
 */

void update_irq(semu_sapporo_iom4 *m)
{
    int level = (m->intstat & m->inten) != 0u;
    if (level != m->irq_level && m->irq_sink != NULL) {
        m->irq_level = level;
        m->irq_sink(m->irq_context, 10u, level);
    }
}

void int_set(semu_sapporo_iom4 *m, uint32_t bit, int value)
{
    if (value != 0) {
        m->intstat |= bit;
    } else {
        m->intstat &= ~bit;
    }
    update_irq(m);
}

void update_threshold(semu_sapporo_iom4 *m)
{
    int_set(m, INT_FIFOTH, m->out_count * 4u < m->thr_write ||
                           m->in_count * 4u > m->thr_read);
}

static void ep_write(semu_sapporo_iom4 *m, uint32_t value, uint32_t n,
                     int msb_first)
{
    uint8_t bytes[4];
    uint32_t i;
    for (i = 0u; i < n; ++i) {
        uint32_t shift = msb_first ? 8u * (n - 1u - i) : 8u * i;
        bytes[i] = (uint8_t)((value >> shift) & 0xffu);
    }
    if (m->devconf == ADDR_GAUGE) {
        semu_sapporo_iom4_gauge_write(&m->gauge, bytes, n);
    } else if (m->devconf == ADDR_HAPTIC) {
        haptic_write(m, bytes, n);
    }
}

static void try_finish(semu_sapporo_iom4 *m)
{
    if (m->size_left != 0u) {
        return;
    }
    if (m->active_cont == 0 && m->devconf == ADDR_GAUGE) {
        semu_sapporo_iom4_gauge_finish(&m->gauge);
    }
    m->active_cmd = 0u;
    m->cmdstat_status = ST_IDLE;
    m->engine_status = ST_IDLE;
    int_set(m, INT_CMD, 1);
}

static void send_data(semu_sapporo_iom4 *m, uint32_t word)
{
    uint32_t n = m->size_left < 4u ? m->size_left : 4u;
    ep_write(m, word, n, 0);
    m->size_left -= n;
    try_finish(m);
}

static int recv_data(semu_sapporo_iom4 *m)
{
    uint8_t buf[4] = { 0, 0, 0, 0 };
    uint32_t n;
    if (m->in_count >= FIFO_WORDS) {
        return 0;
    }
    n = m->size_left < 4u ? m->size_left : 4u;
    if (m->devconf == ADDR_GAUGE) {
        semu_sapporo_iom4_gauge_read(&m->gauge, buf, n);
    } else if (m->devconf == ADDR_HAPTIC) {
        haptic_read(m, buf, n);
    }
    m->in_words[(m->in_tail + m->in_count) % FIFO_WORDS] =
        (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
        ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    m->in_count++;
    m->size_left -= n;
    try_finish(m);
    return 1;
}

int out_push(semu_sapporo_iom4 *m, uint32_t word)
{
    if (m->out_count >= FIFO_WORDS) {
        int_set(m, INT_WROVF, 1);
        return 0;
    }
    m->out_words[(m->out_tail + m->out_count) % FIFO_WORDS] = word;
    m->out_count++;
    if (m->active_cmd == 1u && m->size_left > 0u) {
        uint32_t pending = m->out_words[m->out_tail];
        m->out_tail = (m->out_tail + 1u) % FIFO_WORDS;
        m->out_count--;
        send_data(m, pending);
    }
    update_threshold(m);
    return 1;
}

static int out_pop(semu_sapporo_iom4 *m, uint32_t *word)
{
    if (m->out_count == 0u) {
        return 0;
    }
    *word = m->out_words[m->out_tail];
    m->out_tail = (m->out_tail + 1u) % FIFO_WORDS;
    m->out_count--;
    return 1;
}

uint32_t in_pop(semu_sapporo_iom4 *m)
{
    uint32_t word;
    if (m->in_count == 0u) {
        int_set(m, INT_RDUND, 1);
        return 0u;
    }
    word = m->in_words[m->in_tail];
    m->in_tail = (m->in_tail + 1u) % FIFO_WORDS;
    m->in_count--;
    if (m->active_cmd == 2u && m->size_left > 0u) {
        recv_data(m);
    }
    update_threshold(m);
    return word;
}

static int target_valid(semu_sapporo_iom4 *m, int m2p, unsigned kind)
{
    uint32_t a = m->dma_target;
    return (a >= 0x10000000u && a < 0x10160000u) ||
           (a >= 0x20000000u && a < 0x20400000u) ||
           (m2p && kind == 1u && a >= 0x00040000u && a < 0x001c0000u);
}

/* The execution-model DMA contract requires whole-transfer admission before
 * changing the command, FIFO, endpoint, IRQ or memory. Inspect bytes so that
 * even a one-byte MMIO overlay refuses without invoking its callbacks.
 * Keep the evidenced invalid-target register/interrupt path in prepare_dma;
 * this preflight handles unsafe mappings inside the admitted address windows.
 * Source bytes are staged once, before any command/IRQ callback can run. */
static semu_status validate_dma(semu_sapporo_iom4 *m, uint32_t command,
                                uint8_t *source, semu_error *error)
{
    unsigned kind = (unsigned)(command & 0xfu);
    int m2p = (m->dma_cfg & 2u) != 0u;
    uint32_t size = (command >> 8) & 0xfffu;
    uint32_t count = m->dma_total < size ? m->dma_total : size;
    uint32_t i;
    if ((m->dma_cfg & 1u) == 0u ||
        (kind != 1u && kind != 2u) || !target_valid(m, m2p, kind) ||
        (m2p && kind != 1u) || (!m2p && kind != 2u)) {
        return SEMU_OK;
    }
    if (count != 0u && m->dma_target > UINT32_MAX - (count - 1u)) {
        semu_error_set(error, SEMU_ERR_RANGE, "Sapporo IOM4 DMA span overflows");
        return SEMU_ERR_RANGE;
    }
    for (i = 0u; i < count; ++i) {
        semu_status status = m2p ?
            semu_bus_copy_out(m->bus, m->dma_target + i, source + i, 1u, error) :
            semu_bus_validate_write(m->bus, m->dma_target + i, 1u, error);
        if (status != SEMU_OK) return status;
    }
    return SEMU_OK;
}

static void prepare_dma(semu_sapporo_iom4 *m, uint32_t command,
                         const uint8_t *source)
{
    unsigned kind = (unsigned)(command & 0xfu);
    uint32_t command_size = (command >> 8) & 0xfffu;
    uint32_t count, offset = 0u;
    m->loaded_dma = 0;
    if ((m->dma_cfg & 1u) == 0u || (kind != 1u && kind != 2u)) {
        return;
    }
    if (!target_valid(m, (m->dma_cfg & 2u) != 0u, kind)) {
        m->dma_status = 4u;
        int_set(m, INT_DMA_ERR, 1);
        return;
    }
    count = m->dma_total < command_size ? m->dma_total : command_size;
    if (count == 0u) {
        return;
    }
    m->dma_status = 1u;
    m->dma_active_count = count;
    if ((m->dma_cfg & 2u) != 0u && kind == 1u) {
        while (offset < count) {
            uint32_t chunk = count - offset < 4u ? count - offset : 4u;
            uint32_t word = 0u;
            uint32_t i;
            for (i = 0u; i < chunk; ++i) {
                word |= (uint32_t)source[offset + i] << (8u * i);
            }
            out_push(m, word);
            offset += chunk;
        }
    }
    m->loaded_dma = 1;
}

static void complete_dma(semu_sapporo_iom4 *m, uint32_t command)
{
    unsigned kind = (unsigned)(command & 0xfu);
    if (m->loaded_dma == 0) {
        return;
    }
    if ((m->dma_cfg & 2u) == 0u && kind == 2u) {
        uint32_t remaining = m->dma_active_count;
        uint32_t address = m->dma_target;
        semu_error local;
        while (remaining > 0u) {
            uint32_t word = in_pop(m);
            uint32_t chunk = remaining < 4u ? remaining : 4u;
            uint32_t i;
            for (i = 0u; i < chunk; ++i) {
                /* Every destination byte was admitted as RAM before the
                 * command. No allocation or device callback can fail here. */
                (void)semu_bus_write(m->bus, address++, 1u,
                                     (word >> (8u * i)) & 0xffu, &local);
            }
            remaining -= chunk;
        }
    }
    m->dma_status = 2u;
    m->trig_stat |= 4u;
    m->dma_cfg &= ~1u;
    int_set(m, INT_DMA_CMP, 1);
    m->loaded_dma = 0;
    m->dma_active_count = 0u;
}

/* E-SAP-0036 plus the E-SAP-0040 scoped haptic startup commands. */
static int endpoint_registered(semu_sapporo_iom4 *m)
{
    return m->devconf == ADDR_OBSERVED || m->devconf == ADDR_GAUGE ||
           m->devconf == ADDR_HAPTIC;
}

semu_status command_write(semu_sapporo_iom4 *m, uint32_t value,
                          semu_error *error)
{
    uint8_t source[0xfffu];
    unsigned cmd = (unsigned)(value & 0xfu);
    uint32_t offset_count = (value >> 4) & 7u;
    uint32_t size = (value >> 8) & 0xfffu;
    uint32_t offset_low = (value >> 24) & 0xffu;
    int invalid = 0;
    semu_status status = validate_dma(m, value, source, error);
    if (status != SEMU_OK) return status;
    status = haptic_validate(m, value, source, error);
    if (status != SEMU_OK) return status;
    m->cmd_reg = value;
    prepare_dma(m, value, source);
    /* The wrapper always delegates the command and always completes the
     * DMA afterwards, so the DMA side effects survive every early
     * return of the delegated controller (probe pair 43a639e3..). */
    do {
        if (m->spi_en == 0 && m->i2c_en == 0) {
            break;
        }
        if (m->active_cmd != 0u) {
            break;
        }
        if (cmd != 1u && cmd != 2u) {
            invalid = 1;
        } else if (cmd == 2u && size == 0u) {
            invalid = 1;
        } else if (cmd == 1u && size != 0u && m->out_count == 0u) {
            invalid = 1;
        } else if (offset_count > 5u) {
            invalid = 1;
        } else if (m->spi_en != 0) {
            invalid = 1;
        } else if (m->i2c_en != 0 && !endpoint_registered(m)) {
            invalid = 1;
        }
        if (invalid != 0) {
            m->cmdstat_status = ST_ERROR;
            m->engine_status = ST_IDLE;
            int_set(m, INT_ILLCMD, 1);
            break;
        }
        m->active_cmd = cmd;
        m->active_cont = ((value >> 7) & 1u) != 0u;
        m->size_left = size;
        if (offset_count > 0u) {
            if (offset_count > 1u) {
                ep_write(m, m->offset_hi, offset_count - 1u, 1);
            }
            ep_write(m, offset_low, 1u, 1);
        }
        if (m->size_left == 0u) {
            try_finish(m);
            break;
        }
        m->engine_status = ST_ACTIVE;
        while (m->size_left > 0u) {
            uint32_t word;
            if (cmd == 2u) {
                if (!recv_data(m)) {
                    break;
                }
            } else if (!out_pop(m, &word)) {
                break;
            } else {
                send_data(m, word);
            }
        }
        if (m->size_left != 0u) {
            m->cmdstat_status = ST_WAIT;
        }
        } while (0);
    complete_dma(m, value);
    semu_error_clear(error);
    return SEMU_OK;
}

void semu_sapporo_iom4_reset(semu_sapporo_iom4 *m)
{
    semu_bus *bus;
    semu_sapporo_iom4_irq_fn sink;
    void *context;
    int level;
    if (m == NULL) {
        return;
    }
    bus = m->bus;
    sink = m->irq_sink;
    context = m->irq_context;
    level = m->irq_level;
    memset(m, 0, sizeof(*m));
    m->bus = bus;
    m->irq_sink = sink;
    m->irq_context = context;
    m->fifo_run = 1;
    m->cmdstat_status = ST_IDLE;
    m->engine_status = ST_IDLE;
    semu_sapporo_iom4_gauge_reset(&m->gauge);
    /* Clearing the interrupt sources takes the line low again, exactly
     * as the UpdateIRQ path does when a flag is cleared in place. */
    if (sink != NULL && level != 0) {
        m->irq_level = 0;
        sink(context, 10u, 0);
    }
}

semu_sapporo_iom4 *semu_sapporo_iom4_create(
    semu_bus *bus, semu_sapporo_iom4_irq_fn irq_sink, void *irq_context,
    semu_error *error)
{
    semu_sapporo_iom4 *m;
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo IOM4 requires a bus");
        return NULL;
    }
    m = (semu_sapporo_iom4 *)calloc(1u, sizeof(*m));
    if (m == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate Sapporo IOM4");
        return NULL;
    }
    m->bus = bus;
    m->irq_sink = irq_sink;
    m->irq_context = irq_context;
    semu_sapporo_iom4_reset(m);
    semu_error_clear(error);
    return m;
}

void semu_sapporo_iom4_destroy(semu_sapporo_iom4 *iom4)
{
    free(iom4);
}
