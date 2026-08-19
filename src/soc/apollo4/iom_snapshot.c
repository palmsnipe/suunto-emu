#include "iom_internal.h"

semu_status semu_apollo4_iom_snapshot_write(
    const semu_apollo4_iom *iom, semu_snapshot_writer *writer,
    semu_error *error)
{
    size_t index;
    const uint32_t values[] = {
        iom != NULL ? iom->inten : 0u, iom != NULL ? iom->intstat : 0u,
        iom != NULL ? iom->dma_trig_en : 0u,
        iom != NULL ? iom->dma_trig_stat : 0u,
        iom != NULL ? iom->dma_config : 0u, iom != NULL ? iom->dma_count : 0u,
        iom != NULL ? iom->dma_target : 0u, iom != NULL ? iom->dma_status : 0u,
        iom != NULL ? iom->device_config : 0u
    };
    if (iom == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "IOM snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (semu_snapshot_writer_u8(writer, (uint8_t)(iom->endpoint_attached != 0), error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)(iom->irq_level != 0), error) != SEMU_OK)
        return error->code;
    for (index = 0u; index < SEMU_ARRAY_LEN(values); ++index)
        if (semu_snapshot_writer_u32(writer, values[index], error) != SEMU_OK)
            return error->code;
    for (index = 0u; index < SEMU_APOLLO4_IOM_OBSERVED_REGISTER_COUNT; ++index)
        if (semu_snapshot_writer_u32(writer, iom->observed_registers[index], error) != SEMU_OK)
            return error->code;
    return SEMU_OK;
}

semu_status semu_apollo4_iom_snapshot_read(
    semu_apollo4_iom *iom, semu_snapshot_reader *reader,
    semu_error *error)
{
    semu_apollo4_iom candidate;
    uint32_t *values[9];
    uint8_t attached;
    uint8_t irq_level;
    size_t index;
    if (iom == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "IOM snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *iom;
    if (semu_snapshot_reader_u8(reader, &attached, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &irq_level, error) != SEMU_OK)
        return error->code;
    if (attached > 1u || irq_level > 1u) {
        semu_error_set(error, SEMU_ERR_FORMAT, "invalid IOM snapshot flag");
        return SEMU_ERR_FORMAT;
    }
    candidate.endpoint_attached = attached;
    candidate.irq_level = irq_level;
    values[0] = &candidate.inten;
    values[1] = &candidate.intstat;
    values[2] = &candidate.dma_trig_en;
    values[3] = &candidate.dma_trig_stat;
    values[4] = &candidate.dma_config;
    values[5] = &candidate.dma_count;
    values[6] = &candidate.dma_target;
    values[7] = &candidate.dma_status;
    values[8] = &candidate.device_config;
    for (index = 0u; index < SEMU_ARRAY_LEN(values); ++index)
        if (semu_snapshot_reader_u32(reader, values[index], error) != SEMU_OK)
            return error->code;
    for (index = 0u; index < SEMU_APOLLO4_IOM_OBSERVED_REGISTER_COUNT; ++index)
        if (semu_snapshot_reader_u32(reader, &candidate.observed_registers[index], error) != SEMU_OK)
            return error->code;
    *iom = candidate;
    return SEMU_OK;
}
