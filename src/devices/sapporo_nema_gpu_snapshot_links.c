#include "sapporo_nema_gpu_internal.h"

semu_status semu_nema_gpu_snapshot_event_id_matches(
    const semu_nema_gpu *gpu, uint32_t subject, semu_event_id event_id,
    semu_error *error)
{
    if (gpu == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "NEMA snapshot event identity requires GPU");
        return SEMU_ERR_ARGUMENT;
    }
    return nema_completion_snapshot_event_id_matches(
        semu_nema_gpu_snapshot_completion(gpu), subject, event_id, error);
}

semu_status semu_nema_gpu_snapshot_event_links_match(
    const semu_nema_gpu *gpu, const semu_scheduled_event_state *events,
    size_t count, semu_error *error)
{
    if (gpu == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "NEMA snapshot event linkage requires GPU");
        return SEMU_ERR_ARGUMENT;
    }
    return nema_completion_snapshot_event_links_match(
        semu_nema_gpu_snapshot_completion(gpu), events, count, error);
}
