/*
 * NEMA GPU bus device (E-NEMA-RING-001: SapporoNemaP.cs).
 * Implements the register interface at 0x40090000.  When the firmware
 * writes CMDRINGSTOP after initialization, extracts the new ring
 * entries and submits them to the display backend for rendering.
 * Raises IRQ 28 on successful completion.
 */

#include "sapporo_nema_gpu_internal.h"

#include "../display/nema_completion.h"
#include "../display/nema_framing.h"

#include <stdlib.h>
#include <string.h>

static int is_command_register(uint32_t offset)
{
    return offset == NEMA_REG_CMDSTATUS ||
           offset == NEMA_REG_CMDRINGSTOP ||
           offset == NEMA_REG_CMDADDR ||
           offset == NEMA_REG_CMDSIZE ||
           offset == NEMA_REG_INTERRUPT ||
           offset == NEMA_REG_STATUS ||
           offset == NEMA_REG_CLID;
}

static int is_sram_range(uint32_t address, uint32_t size)
{
    return size > 0u && size <= NEMA_MAX_CAPTURE_SIZE &&
           address >= NEMA_SRAM_START && address < NEMA_SRAM_END &&
           size <= NEMA_SRAM_END - address;
}

int nema_gpu_ring_configured(const semu_nema_gpu *gpu)
{
    uint32_t addr = gpu->registers[NEMA_REG_CMDADDR / 4u];
    uint32_t size = gpu->registers[NEMA_REG_CMDSIZE / 4u];
    return addr != 0u && size != 0u && is_sram_range(addr, size);
}

static uint32_t normalize_ring_pointer(uint32_t raw)
{
    return raw & ~NEMA_RING_CTRL_MASK;
}

void nema_gpu_completion_write(void *context, uint32_t offset,
                                 uint32_t value)
{
    semu_nema_gpu *gpu = (semu_nema_gpu *)context;
    if (gpu != NULL && offset < NEMA_GPU_SIZE && (offset & 3u) == 0u) {
        gpu->registers[offset / 4u] = value;
    }
}

void nema_gpu_completion_irq(void *context, unsigned irq, int asserted)
{
    semu_nema_gpu *gpu = (semu_nema_gpu *)context;
    if (gpu != NULL && irq == NEMA_GPU_IRQ && gpu->irq_sink != NULL) {
        gpu->irq_sink(gpu->irq_context, irq, asserted);
    }
}

static semu_status nema_gpu_read(void *context, uint32_t offset,
    unsigned width, uint32_t *value, semu_error *error)
{
    semu_nema_gpu *gpu = (semu_nema_gpu *)context;
    if (gpu == NULL || value == NULL) {
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || offset >= NEMA_GPU_SIZE || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "nema: unsupported register read shape");
        return SEMU_ERR_UNSUPPORTED;
    }
    if (offset == NEMA_REG_MODULE_ID) {
        *value = NEMA_MODULE_ID;
        return SEMU_OK;
    }
    if (offset == NEMA_REG_FRAME_GEN) {
        *value = gpu->frame_generation;
        return SEMU_OK;
    }
    *value = gpu->registers[offset / 4u];
    return SEMU_OK;
}

static semu_status nema_gpu_write(void *context, uint32_t offset,
    unsigned width, uint32_t value, semu_error *error)
{
    semu_nema_gpu *gpu = (semu_nema_gpu *)context;
    if (gpu == NULL) {
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || offset >= NEMA_GPU_SIZE || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "nema: unsupported register write shape");
        return SEMU_ERR_UNSUPPORTED;
    }
    if (gpu->submitting) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "nema: submission already active");
        return SEMU_ERR_CONFLICT;
    }
    if (gpu->initialization_complete && offset == NEMA_REG_CMDRINGSTOP &&
        (value & NEMA_RING_CTRL_MASK) != NEMA_BOOTSTRAP_CTRL)
        return nema_gpu_submit(gpu, value, error);
    gpu->registers[offset / 4u] = value;

    if (is_command_register(offset)) {
        if (gpu->initialization_complete &&
            offset == NEMA_REG_CMDRINGSTOP) {
            if ((value & NEMA_RING_CTRL_MASK) == NEMA_BOOTSTRAP_CTRL) {
                gpu->previous_ring_stop = normalize_ring_pointer(value);
                return SEMU_OK;
            }
        } else if (offset == NEMA_REG_STATUS && value == 0u &&
                   nema_gpu_ring_configured(gpu)) {
            gpu->initialization_complete = 1;
            gpu->previous_ring_stop = normalize_ring_pointer(
                gpu->registers[NEMA_REG_CMDRINGSTOP / 4u]);
        } else if (offset == NEMA_REG_INTERRUPT && value == 0u) {
            if (gpu->irq_sink != NULL) {
                gpu->irq_sink(gpu->irq_context, NEMA_GPU_IRQ, 0);
            }
        }
    }
    return SEMU_OK;
}

static void nema_gpu_reset_impl(void *context)
{
    semu_nema_gpu *gpu = (semu_nema_gpu *)context;
    if (gpu != NULL && !gpu->submitting) {
        nema_completion_reset(gpu->completion);
        memset(gpu->registers, 0, sizeof(gpu->registers));
        gpu->initialization_complete = 0;
        gpu->previous_ring_stop = 0u;
        gpu->frame_generation = 0u;
    }
}

static const semu_bus_device_ops nema_gpu_ops = {
    .read = nema_gpu_read,
    .write = nema_gpu_write,
    .reset = nema_gpu_reset_impl
};

semu_nema_gpu *semu_nema_gpu_create(semu_bus *bus,
    const semu_display_backend_ops *backend,
    void *backend_context,
    semu_frame_callback frame_callback,
    void *frame_context,
    semu_apollo4_irq_fn irq_sink,
    void *irq_context,
    semu_scheduler *scheduler,
    semu_error *error)
{
    semu_nema_gpu *gpu;
    if (bus == NULL || (backend != NULL && (backend->prepare == NULL ||
        backend->commit == NULL || backend->abort == NULL))) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "nema_gpu: null bus or incomplete backend operations");
        return NULL;
    }
    gpu = (semu_nema_gpu *)calloc(1u, sizeof(*gpu));
    if (gpu == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate nema_gpu");
        return NULL;
    }
    gpu->bus = bus;
    if (backend != NULL) gpu->backend = *backend;
    gpu->backend_context = backend_context;
    gpu->frame_callback = frame_callback;
    gpu->frame_context = frame_context;
    gpu->irq_sink = irq_sink;
    gpu->irq_context = irq_context;
    gpu->scheduler = scheduler;
    if (nema_completion_create(&gpu->completion, error) != SEMU_OK) {
        free(gpu);
        return NULL;
    }
    return gpu;
}

void semu_nema_gpu_destroy(semu_nema_gpu *gpu)
{
    if (gpu != NULL && gpu->submitting) return;
    if (gpu != NULL) {
        nema_completion_cancel(gpu->completion);
        nema_completion_destroy(gpu->completion);
    }
    free(gpu);
}

int semu_nema_gpu_busy(const semu_nema_gpu *gpu)
{
    return gpu != NULL && gpu->submitting;
}

semu_status semu_nema_gpu_reset(semu_nema_gpu *gpu)
{
    if (gpu == NULL) return SEMU_ERR_ARGUMENT;
    if (gpu->submitting) return SEMU_ERR_CONFLICT;
    nema_gpu_reset_impl(gpu);
    return SEMU_OK;
}

semu_status semu_nema_gpu_attach(semu_nema_gpu *gpu, semu_error *error)
{
    if (gpu == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "nema_gpu attach: null");
        return SEMU_ERR_ARGUMENT;
    }
    return semu_bus_map_device(gpu->bus, "sapporo.nema_gpu",
        NEMA_GPU_BASE, NEMA_GPU_SIZE, &nema_gpu_ops, gpu, error);
}
