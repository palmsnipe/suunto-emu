#include "sapporo_nema_gpu_internal.h"

semu_status semu_nema_gpu_snapshot_write(
    const semu_nema_gpu *gpu, semu_snapshot_writer *writer, semu_error *error)
{
    size_t index;
    if (gpu == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "NEMA snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (gpu->submitting) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "nema: snapshot during submission");
        return SEMU_ERR_CONFLICT;
    }
    for (index = 0u; index < NEMA_REG_COUNT; ++index)
        if (semu_snapshot_writer_u32(writer, gpu->registers[index], error) != SEMU_OK)
            return error->code;
    if (semu_snapshot_writer_u8(writer, (uint8_t)(gpu->initialization_complete != 0), error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, gpu->previous_ring_stop, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, gpu->frame_generation, error) != SEMU_OK ||
        nema_completion_snapshot_write(gpu->completion, writer, error) != SEMU_OK)
        return error->code;
    return SEMU_OK;
}

semu_status semu_nema_gpu_snapshot_read(
    semu_nema_gpu *gpu, semu_snapshot_reader *reader, semu_error *error)
{
    semu_nema_gpu candidate;
    uint8_t initialized;
    size_t index;
    if (gpu == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "NEMA snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *gpu;
    if (gpu->submitting) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "nema: snapshot during submission");
        return SEMU_ERR_CONFLICT;
    }
    for (index = 0u; index < NEMA_REG_COUNT; ++index)
        if (semu_snapshot_reader_u32(reader, &candidate.registers[index], error) != SEMU_OK)
            return error->code;
    if (semu_snapshot_reader_u8(reader, &initialized, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.previous_ring_stop, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.frame_generation, error) != SEMU_OK)
        return error->code;
    if (initialized > 1u) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid NEMA initialization flag");
        return SEMU_ERR_FORMAT;
    }
    if (initialized != 0u && !nema_gpu_ring_configured(&candidate)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "initialized NEMA snapshot has no command ring");
        return SEMU_ERR_FORMAT;
    }
    if (nema_completion_snapshot_read(candidate.completion, reader, error) != SEMU_OK)
        return error->code;
    nema_completion_rebind_active(candidate.completion, gpu->scheduler, nema_gpu_completion_write,
                                  gpu, nema_gpu_completion_irq, gpu);
    candidate.initialization_complete = initialized;
    *gpu = candidate;
    return SEMU_OK;
}

semu_status semu_nema_gpu_snapshot_resolve_event(
    semu_nema_gpu *gpu, uint32_t subject, semu_event_callback *callback,
    void **context, semu_error *error)
{
    if (gpu == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "NEMA snapshot event requires GPU");
        return SEMU_ERR_ARGUMENT;
    }
    return nema_completion_snapshot_resolve_event(
        gpu->completion, subject, callback, context, error);
}

const nema_completion *semu_nema_gpu_snapshot_completion(
    const semu_nema_gpu *gpu)
{
    return gpu != NULL ? gpu->completion : NULL;
}
