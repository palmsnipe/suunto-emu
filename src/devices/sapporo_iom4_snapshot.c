#include "sapporo_iom4_internal.h"

#include "../core/snapshot_io.h"

/*
 * Ticket 792: v2 snapshot codec for the live Sapporo-2.35.34 IOM4
 * mirror (E-SAP-0036/0040 state). Round-trips only state the running
 * model exposes; bus, IRQ sink and context bindings stay with the
 * instance. A mid-command or loaded-DMA state refuses on both save and
 * load, exactly as the ticket's refusal boundary requires.
 */

semu_status semu_sapporo_iom4_snapshot_write(
    const semu_sapporo_iom4 *iom4, semu_snapshot_writer *writer,
    semu_error *error)
{
    const semu_sapporo_iom4 *m = iom4;
    size_t index;
    if (m == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo IOM4 snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (m->active_cmd != 0u || m->size_left != 0u || m->loaded_dma != 0 ||
        m->dma_active_count != 0u) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "Sapporo IOM4 mid-command state refuses a snapshot");
        return SEMU_ERR_STATE;
    }
    if (semu_snapshot_writer_u8(writer, (uint8_t)m->irq_level, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)m->out_tail, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)m->in_tail, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)m->out_count, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)m->in_count, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)m->popwr, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)m->fifo_run, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)m->spi_en, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)m->i2c_en, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)m->active_cont, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)m->loaded_dma, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, m->gauge.reg, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)m->gauge.expecting_register,
                                error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, m->haptic_selected, error) != SEMU_OK)
        return error->code;
    for (index = 0u; index < FIFO_WORDS; ++index) {
        if (semu_snapshot_writer_u32(writer, m->out_words[index], error) != SEMU_OK ||
            semu_snapshot_writer_u32(writer, m->in_words[index], error) != SEMU_OK)
            return error->code;
    }
    for (index = 0u; index < 256u; ++index) {
        if (semu_snapshot_writer_u16(writer, m->gauge.registers[index],
                                     error) != SEMU_OK)
            return error->code;
    }
    if (semu_snapshot_writer_u32(writer, m->thr_read, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->thr_write, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->ioclk, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->offset_hi, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->cmd_reg, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->cmdstat_status, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->engine_status, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->inten, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->intstat, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->trig_en, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->trig_stat, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->dma_cfg, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->dma_total, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->dma_target, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->dma_status, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->i2c_cfg, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->devconf, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->gauge.read_index, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, m->gauge.write_index, error) != SEMU_OK ||
        semu_snapshot_writer_bytes(writer, m->haptic_registers,
                                   sizeof(m->haptic_registers), error) != SEMU_OK)
        return error->code;
    return SEMU_OK;
}

semu_status semu_sapporo_iom4_snapshot_read(
    semu_sapporo_iom4 *iom4, semu_snapshot_reader *reader, semu_error *error)
{
    semu_sapporo_iom4 *m = iom4;
    semu_sapporo_iom4 candidate;
    uint8_t flags[14];
    size_t index;
    if (m == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo IOM4 snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *m;
    for (index = 0u; index < sizeof(flags); ++index) {
        if (semu_snapshot_reader_u8(reader, &flags[index], error) != SEMU_OK)
            return error->code;
    }
    for (index = 0u; index < FIFO_WORDS; ++index) {
        if (semu_snapshot_reader_u32(reader, &candidate.out_words[index],
                                     error) != SEMU_OK ||
            semu_snapshot_reader_u32(reader, &candidate.in_words[index],
                                    error) != SEMU_OK)
            return error->code;
    }
    for (index = 0u; index < 256u; ++index) {
        if (semu_snapshot_reader_u16(reader, &candidate.gauge.registers[index],
                                     error) != SEMU_OK)
            return error->code;
    }
    if (semu_snapshot_reader_u32(reader, &candidate.thr_read, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.thr_write, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.ioclk, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.offset_hi, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.cmd_reg, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.cmdstat_status, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.engine_status, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.inten, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.intstat, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.trig_en, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.trig_stat, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.dma_cfg, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.dma_total, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.dma_target, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.dma_status, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.i2c_cfg, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.devconf, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.gauge.read_index, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.gauge.write_index, error) != SEMU_OK ||
        semu_snapshot_reader_bytes(reader, candidate.haptic_registers,
                                    sizeof(candidate.haptic_registers),
                                    error) != SEMU_OK)
        return error->code;
    candidate.irq_level = flags[0];
    candidate.out_tail = flags[1];
    candidate.in_tail = flags[2];
    candidate.out_count = flags[3];
    candidate.in_count = flags[4];
    candidate.popwr = flags[5];
    candidate.fifo_run = flags[6];
    candidate.spi_en = flags[7];
    candidate.i2c_en = flags[8];
    candidate.active_cont = flags[9];
    candidate.loaded_dma = flags[10];
    candidate.gauge.reg = flags[11];
    candidate.gauge.expecting_register = flags[12];
    candidate.haptic_selected = flags[13];
    /* Only states the E-SAP-0036/0040 write law can produce round-trip;
     * the masks are the probe-pinned register write masks and the FIFO
     * invariants are the wrapper's own ring law. */
    if (candidate.irq_level > 1 || candidate.out_tail >= FIFO_WORDS ||
        candidate.in_tail >= FIFO_WORDS ||
        candidate.out_count >= FIFO_WORDS || candidate.in_count >= FIFO_WORDS ||
        candidate.popwr > 1 || candidate.fifo_run > 1 ||
        candidate.spi_en > 1 || candidate.i2c_en > 1 ||
        candidate.active_cont > 1 || candidate.loaded_dma != 0 ||
        candidate.gauge.expecting_register > 1 ||
        candidate.active_cmd != 0u || candidate.size_left != 0u ||
        candidate.dma_active_count != 0u ||
        (candidate.spi_en != 0 && candidate.i2c_en != 0) ||
        (candidate.irq_sink != NULL &&
         (candidate.irq_level != 0) !=
             ((candidate.intstat & candidate.inten) != 0u)) ||
        candidate.thr_read > 0x3fu || candidate.thr_write > 0x3fu ||
        (candidate.ioclk & ~UINT32_C(0xffff1f01)) != 0u ||
        candidate.trig_en > 3u ||
        (candidate.dma_cfg & ~UINT32_C(0x303)) != 0u ||
        (candidate.dma_total & ~UINT32_C(0xfff)) != 0u ||
        (candidate.dma_target & ~UINT32_C(0x3fffffff)) != 0u ||
        (candidate.dma_status != 0u && candidate.dma_status != ST_ACTIVE &&
         candidate.dma_status != ST_IDLE && candidate.dma_status != ST_ERROR) ||
        (candidate.i2c_cfg & ~UINT32_C(0xff71)) != 0u ||
        candidate.devconf > 0x3ffu ||
        ((candidate.i2c_cfg & 1u) == 0u && candidate.devconf > 0x7fu) ||
        (candidate.inten & ~INT_MASK) != 0u ||
        (candidate.intstat & ~INT_MASK) != 0u ||
        (candidate.trig_stat & ~UINT32_C(4)) != 0u ||
        (candidate.cmdstat_status != ST_ERROR &&
         candidate.cmdstat_status != ST_IDLE &&
         candidate.cmdstat_status != ST_WAIT) ||
        (candidate.engine_status != ST_ACTIVE &&
         candidate.engine_status != ST_IDLE) ||
        candidate.haptic_selected > 0x44u) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "Sapporo IOM4 snapshot state is unreachable");
        return SEMU_ERR_FORMAT;
    }
    *m = candidate;
    return SEMU_OK;
}
