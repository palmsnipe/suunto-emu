#ifndef SEMU_SAPPORO_NEMA_GPU_INTERNAL_H
#define SEMU_SAPPORO_NEMA_GPU_INTERNAL_H
#include "sapporo_nema_gpu.h"
#include "semu/log.h"
#include "../display/nema_completion.h"

/* Register offsets (SapporoNemaP.cs lines 2197-2208) */
#define NEMA_REG_SETUP        0x094u
#define NEMA_REG_CMDSTATUS    0x0e8u
#define NEMA_REG_CMDRINGSTOP  0x0ecu
#define NEMA_REG_STATUS       0x0fcu
#define NEMA_REG_MODULE_ID    0x1ecu
#define NEMA_REG_CONFIG       0x1f0u
#define NEMA_REG_FRAME_GEN    0x1f4u

#define NEMA_MODULE_ID         0x86362000u
#define NEMA_RING_CTRL_MASK    0x7u
#define NEMA_BOOTSTRAP_CTRL    0x6u
#define NEMA_SRAM_START        0x10000000u
#define NEMA_SRAM_END          0x10180000u
#define NEMA_MAX_CAPTURE_SIZE  0x10000u
#define NEMA_REG_COUNT         (NEMA_GPU_SIZE / 4u)

struct semu_nema_gpu {
    semu_bus *bus;
    semu_display_backend_ops backend;
    void *backend_context;
    semu_frame_callback frame_callback;
    void *frame_context;
    semu_apollo4_irq_fn irq_sink;
    void *irq_context;
    semu_scheduler *scheduler;
    nema_completion *completion;
    uint32_t registers[NEMA_REG_COUNT];
    int initialization_complete;
    uint32_t previous_ring_stop;
    uint32_t frame_generation;
    int submitting;
    /* Transient, non-serialized diagnostic state (ticket 794,
     * E-EMU-SAP235-RINGKICK-CPU-INVISIBLE-001): the logger borrowed at
     * wiring time and the count of committed CMDRINGSTOP submissions since
     * initialization/reset, used as the stable ordinal in refusal lines. */
    semu_logger *logger;
    uint32_t submission_ordinal;
};


int nema_gpu_ring_configured(const semu_nema_gpu *gpu);
void nema_gpu_completion_write(void *context, uint32_t offset, uint32_t value);
void nema_gpu_completion_irq(void *context, unsigned irq, int asserted);
semu_status nema_gpu_submit(semu_nema_gpu *gpu, uint32_t raw_stop, semu_error *error);
const nema_completion *semu_nema_gpu_snapshot_completion(const semu_nema_gpu *gpu);
#endif
