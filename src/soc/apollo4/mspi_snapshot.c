#include "mspi_internal.h"

#include <stddef.h>

#define MSPI_DMA_STATUS_COMPLETE UINT32_C(0x02)
#define MSPI_DMA_STATUS_ERROR UINT32_C(0x04)

static int valid_mspi2_registers(const semu_apollo4_mspi *mspi)
{
    uint32_t command = mspi->registers[SEMU_APOLLO4_MSPI2_COMMAND / 4u];
    uint32_t dma_config =
        mspi->registers[SEMU_APOLLO4_MSPI2_DMA_CONFIG / 4u];
    uint32_t dma_status =
        mspi->registers[SEMU_APOLLO4_MSPI2_DMA_STATUS / 4u];
    return (command == 0u || command == UINT32_C(0xc1) ||
            command == UINT32_C(0xe1)) &&
           (dma_config == 0u || dma_config == UINT32_C(0x10) ||
            dma_config == UINT32_C(0x14)) && dma_status == 0u;
}

semu_status semu_apollo4_mspi_snapshot_write(
    const semu_apollo4_mspi *mspi, semu_snapshot_writer *writer,
    semu_error *error)
{
    if (mspi == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "MSPI snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    size_t index;
    if (semu_snapshot_writer_u8(writer, (uint8_t)(mspi->endpoint_attached != 0), error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, mspi->status, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, mspi->dma_status, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)(mspi->irq_level != 0), error) != SEMU_OK)
        return error->code;
    for (index = 0u; index < SEMU_ARRAY_LEN(mspi->registers); ++index) {
        if (semu_snapshot_writer_u32(writer, mspi->registers[index], error) !=
            SEMU_OK)
            return error->code;
    }
    if (semu_snapshot_writer_bytes(writer, mspi->dma_buffer,
                                   sizeof(mspi->dma_buffer), error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, mspi->dma_transaction.address, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, mspi->dma_transaction.chip_select, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, (uint64_t)mspi->dma_transaction.tx_size, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, (uint64_t)mspi->dma_transaction.rx_size, error) != SEMU_OK)
        return error->code;
    return SEMU_OK;
}

semu_status semu_apollo4_mspi_snapshot_read(
    semu_apollo4_mspi *mspi, semu_snapshot_reader *reader,
    semu_error *error)
{
    semu_apollo4_mspi candidate;
    uint8_t attached;
    uint8_t irq_level;
    uint64_t tx_size;
    uint64_t rx_size;
    size_t index;
    if (mspi == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "MSPI snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *mspi;
    if (semu_snapshot_reader_u8(reader, &attached, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.status, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.dma_status, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &irq_level, error) != SEMU_OK)
        return error->code;
    for (index = 0u; index < SEMU_ARRAY_LEN(candidate.registers); ++index) {
        if (semu_snapshot_reader_u32(reader, &candidate.registers[index],
                                     error) != SEMU_OK)
            return error->code;
    }
    if (semu_snapshot_reader_bytes(reader, candidate.dma_buffer,
                                   sizeof(candidate.dma_buffer), error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &candidate.dma_transaction.address, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &candidate.dma_transaction.chip_select, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &tx_size, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &rx_size, error) != SEMU_OK)
        return error->code;
    if (attached > 1u || irq_level > 1u ||
        (candidate.dma_status != 0u &&
         candidate.dma_status != MSPI_DMA_STATUS_COMPLETE &&
         candidate.dma_status != MSPI_DMA_STATUS_ERROR) ||
        (irq_level != 0u) !=
            ((candidate.status &
              candidate.registers[SEMU_APOLLO4_MSPI_INTEN / 4u]) != 0u) ||
        (candidate.base == SEMU_APOLLO4_MSPI2_BASE &&
         !valid_mspi2_registers(&candidate)) ||
        candidate.dma_transaction.chip_select != 0u ||
        tx_size > sizeof(candidate.dma_buffer) || rx_size != 0u) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid MSPI snapshot transaction");
        return SEMU_ERR_FORMAT;
    }
    candidate.endpoint_attached = attached;
    candidate.irq_level = irq_level;
    candidate.dma_transaction.tx = candidate.dma_buffer;
    candidate.dma_transaction.tx_size = (size_t)tx_size;
    candidate.dma_transaction.rx = NULL;
    candidate.dma_transaction.rx_size = 0u;
    *mspi = candidate;
    return SEMU_OK;
}
