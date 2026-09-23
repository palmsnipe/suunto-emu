#include "sapporo_iom4_internal.h"

/*
 * E-SAP-0036 register dispatch for the Sapporo-2.35.34 IOM4 mirror.
 * Readbacks mirror the 1.16.1 lane probe pairs (5a2f3974.., 43a639e3..,
 * 854d2ebd..): tag-only upstream fields that the lane never reads back
 * accept writes and read zero; unknown offsets and non-4 widths refuse
 * fail-closed. Masks proven on the lane: FIFO access ports 0x00/0x20
 * index the eight physical words absolutely (DirectGet/DirectSet, no
 * count gating, no hooks); I2C config 0xff71 (a5a5a5a5 -> 0xa521,
 * f270 -> f270, 103f270 -> f270; SMPCNT/STRDIS tags drop); DEVADDR
 * 10-bit store truncated to 7 bits whenever the I2C ADERSZ bit is
 * clear; IOCLK retains 0xffff1f01 (a5a5a5a5 -> a5a50501,
 * 1d0e1301 -> raw); DMA window masks 0x3/0x303/0xfff/0x3fffffff with
 * status writes ignored.
 */

semu_status semu_sapporo_iom4_read(semu_sapporo_iom4 *m, uint32_t offset,
                                   unsigned width, uint32_t *value,
                                   semu_error *error)
{
    if (m == NULL || width != 4u || value == NULL) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Sapporo IOM4 supports 32-bit reads only");
        return SEMU_ERR_UNSUPPORTED;
    }
    *value = 0u;
    if (offset < R_FIFO_IN) {
        *value = m->out_words[(offset / 4u) % FIFO_WORDS];
    } else if (offset >= R_FIFO_IN && offset < 0x40u) {
        *value = m->in_words[((offset - R_FIFO_IN) / 4u) % FIFO_WORDS];
    } else switch (offset) {
    case R_FIFO_SIZE:
        *value = m->out_count * 4u | (32u - m->out_count * 4u) << 8 |
                 (m->in_count * 4u) << 16 | (32u - m->in_count * 4u) << 24;
        break;
    case R_FIFO_THRESH: *value = m->thr_read | (m->thr_write << 8); break;
    case R_FIFO_POP:    *value = in_pop(m); break;
    case R_FIFO_CTRL:
        *value = (uint32_t)(m->popwr ? 1 : 0) |
                 (uint32_t)(m->fifo_run ? 2 : 0);
        break;
    case R_FIFO_PTRS:
        *value = (m->out_count & 0xfu) | ((m->in_count & 0xfu) << 8);
        break;
    case R_IOCLK:       *value = m->ioclk; break;
    case R_SUBMOD:
        *value = 0xe20u | (uint32_t)(m->spi_en ? 1 : 0) |
                 (uint32_t)(m->i2c_en ? 0x10 : 0);
        break;
    case R_COMMAND:     *value = m->cmd_reg; break;
    case R_DCX:         break;
    case R_OFFSET_HI:   *value = m->offset_hi; break;
    case R_CMDSTAT:
        *value = (m->active_cmd & 0x1fu) | (m->cmdstat_status << 5) |
                 ((m->size_left & 0xfffu) << 8);
        break;
    case R_INTEN:       *value = m->inten; break;
    case R_INTSTAT:     *value = m->intstat; break;
    case R_INTCLR:      *value = 0xffff8000u; break;
    case R_INTSET:      *value = 0u; break;
    case R_TRIG_EN:     *value = m->trig_en; break;
    case R_TRIG_STAT:   *value = m->trig_stat; break;
    case R_DMA_CFG:     *value = m->dma_cfg; break;
    case R_DMA_TOTAL:   *value = m->dma_total; break;
    case R_DMA_TARGET:  *value = m->dma_target; break;
    case R_DMA_STATUS:  *value = m->dma_status; break;
    case R_CQ_CONF: case R_CQ_TARGET: case R_CQ_FLAG: case R_CQ_PAUSE:
    case R_CQ_CUR: case R_CQ_END:
        break;
    case R_MODULE_STATUS:
        *value = (m->engine_status == ST_ACTIVE ? 2u : 0u) |
                 (m->engine_status == ST_IDLE ? 4u : 0u);
        break;
    case R_SPI_CFG:     *value = 0x00200000u; break;
    case R_I2C_CFG:     *value = m->i2c_cfg; break;
    case R_DEVICE_CFG:  *value = m->devconf; break;
    default:
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Sapporo IOM4 offset 0x%08x is unobserved",
                       (unsigned)offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_sapporo_iom4_write(semu_sapporo_iom4 *m, uint32_t offset,
                                    unsigned width, uint32_t value,
                                    semu_error *error)
{
    if (m == NULL || width != 4u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Sapporo IOM4 supports 32-bit writes only");
        return SEMU_ERR_UNSUPPORTED;
    }
    if (offset < R_FIFO_IN) {
        m->out_words[(offset / 4u) % FIFO_WORDS] = value;
    } else if (offset >= R_FIFO_IN && offset < 0x40u) {
        m->in_words[((offset - R_FIFO_IN) / 4u) % FIFO_WORDS] = value;
    } else switch (offset) {
    case R_FIFO_THRESH:
        m->thr_read = value & 0x3fu;
        m->thr_write = (value >> 8) & 0x3fu;
        update_threshold(m);
        break;
    case R_FIFO_PUSH:
        out_push(m, value);
        break;
    case R_FIFO_CTRL:
        m->popwr = (value & 1u) != 0u;
        m->fifo_run = (value & 2u) != 0u;
        if ((value & 2u) == 0u) {
            m->out_count = 0u;
            m->in_count = 0u;
            m->out_tail = 0u;
            m->in_tail = 0u;
        }
        break;
    case R_IOCLK:
        m->ioclk = value & 0xffff1f01u;
        break;
    case R_SUBMOD:
        m->spi_en = (value & 1u) != 0u;
        m->i2c_en = (value & 0x10u) != 0u;
        if (m->spi_en != 0 && m->i2c_en != 0) {
            m->spi_en = 0;
            m->i2c_en = 0;
        }
        break;
    case R_COMMAND:
        return command_write(m, value, error);
    case R_DCX: case R_INTSTAT: case R_TRIG_STAT: case R_DMA_STATUS:
    case R_CQ_CONF: case R_CQ_TARGET: case R_CQ_FLAG: case R_CQ_PAUSE:
    case R_CQ_CUR: case R_CQ_END: case R_MODULE_STATUS: case R_SPI_CFG:
    case R_FIFO_POP:
        break;
    case R_OFFSET_HI:   m->offset_hi = value; break;
    case R_INTEN:
        m->inten = value & INT_MASK;
        update_irq(m);
        break;
    case R_INTCLR:
        m->intstat &= ~(value & INT_MASK);
        update_irq(m);
        break;
    case R_INTSET:
        m->intstat |= value & INT_MASK;
        update_irq(m);
        break;
    case R_TRIG_EN:     m->trig_en = value & 3u; break;
    case R_DMA_CFG:     m->dma_cfg = value & 0x303u; break;
    case R_DMA_TOTAL:   m->dma_total = value & 0xfffu; break;
    case R_DMA_TARGET:  m->dma_target = value & 0x3fffffffu; break;
    case R_I2C_CFG:     m->i2c_cfg = value & 0xff71u; break;
    case R_DEVICE_CFG:
        /* A waiting FIFO command cannot bypass the haptic command admission
         * by changing its endpoint after the command has begun. */
        if (m->active_cmd != 0u &&
            (m->devconf == ADDR_HAPTIC || (value & 0x7fu) == ADDR_HAPTIC)) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "Sapporo haptic endpoint switch during command");
            return SEMU_ERR_UNSUPPORTED;
        }
        m->devconf = value & 0x3ffu;
        if ((m->i2c_cfg & 1u) == 0u && m->devconf > 0x7fu) {
            m->devconf &= 0x7fu;
        }
        break;
    default:
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Sapporo IOM4 offset 0x%08x is unobserved",
                       (unsigned)offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}
