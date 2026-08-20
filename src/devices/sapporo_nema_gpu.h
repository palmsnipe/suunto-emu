#ifndef SEMU_SAPPORO_NEMA_GPU_H
#define SEMU_SAPPORO_NEMA_GPU_H

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/display.h"
#include "semu/frame.h"
#include "semu/scheduler.h"
#include "semu/types.h"
#include "../core/scheduler_internal.h"
#include "../core/snapshot_io.h"

/*
 * NEMA GPU bus device (E-NEMA-RING-001: SapporoNemaP.cs).
 * Maps at 0x40090000 (4 KB), IRQ 28.  Intercepts register writes
 * and triggers the display backend when the firmware submits a
 * command ring via CMDRINGSTOP after initialization.
 */

#define NEMA_GPU_BASE 0x40090000u
#define NEMA_GPU_SIZE 0x1000u
#define NEMA_GPU_IRQ  28u

typedef struct semu_nema_gpu semu_nema_gpu;

semu_nema_gpu *semu_nema_gpu_create(semu_bus *bus,
    semu_display_backend_submit_fn backend_submit,
    void *backend_context,
    semu_frame_callback frame_callback,
    void *frame_context,
    semu_apollo4_irq_fn irq_sink,
    void *irq_context,
    semu_scheduler *scheduler,
    semu_error *error);

void semu_nema_gpu_destroy(semu_nema_gpu *gpu);
void semu_nema_gpu_reset(semu_nema_gpu *gpu);

semu_status semu_nema_gpu_attach(semu_nema_gpu *gpu, semu_error *error);
semu_status semu_nema_gpu_snapshot_write(
    const semu_nema_gpu *gpu, semu_snapshot_writer *writer, semu_error *error);
semu_status semu_nema_gpu_snapshot_read(
    semu_nema_gpu *gpu, semu_snapshot_reader *reader, semu_error *error);
semu_status semu_nema_gpu_snapshot_resolve_event(
    semu_nema_gpu *gpu, uint32_t subject, semu_event_callback *callback,
    void **context, semu_error *error);
semu_status semu_nema_gpu_snapshot_event_id_matches(
    const semu_nema_gpu *gpu, uint32_t subject, semu_event_id event_id,
    semu_error *error);
semu_status semu_nema_gpu_snapshot_event_links_match(
    const semu_nema_gpu *gpu, const semu_scheduled_event_state *events,
    size_t count, semu_error *error);

#endif
