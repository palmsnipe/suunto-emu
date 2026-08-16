/*
 * NEMA GPU bus device (E-NEMA-RING-001: SapporoNemaP.cs).
 * Implements the register interface at 0x40090000.  When the firmware
 * writes CMDRINGSTOP after initialization, extracts the new ring
 * entries and submits them to the display backend for rendering.
 * Raises IRQ 28 on successful completion.
 */

#include "sapporo_nema_gpu.h"

#include <stdlib.h>
#include <string.h>

/* Register offsets (SapporoNemaP.cs lines 2197-2208) */
#define NEMA_REG_SETUP        0x094u
#define NEMA_REG_CMDSTATUS    0x0e8u
#define NEMA_REG_CMDRINGSTOP  0x0ecu
#define NEMA_REG_CMDADDR      0x0f0u
#define NEMA_REG_CMDSIZE      0x0f4u
#define NEMA_REG_INTERRUPT    0x0f8u
#define NEMA_REG_STATUS       0x0fcu
#define NEMA_REG_CLID         0x148u
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
    semu_display_backend_submit_fn backend_submit;
    void *backend_context;
    semu_frame_callback frame_callback;
    void *frame_context;
    semu_apollo4_irq_fn irq_sink;
    void *irq_context;
    uint32_t registers[NEMA_REG_COUNT];
    int initialization_complete;
    uint32_t previous_ring_stop;
    uint32_t frame_generation;
};

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

static int ring_is_configured(const semu_nema_gpu *gpu)
{
    uint32_t addr = gpu->registers[NEMA_REG_CMDADDR / 4u];
    uint32_t size = gpu->registers[NEMA_REG_CMDSIZE / 4u];
    return addr != 0u && size != 0u && is_sram_range(addr, size);
}

static uint32_t normalize_ring_pointer(uint32_t raw)
{
    return raw & ~NEMA_RING_CTRL_MASK;
}

static void trigger_rendering(semu_nema_gpu *gpu, uint32_t raw_stop)
{
    uint32_t address = gpu->registers[NEMA_REG_CMDADDR / 4u];
    uint32_t size = gpu->registers[NEMA_REG_CMDSIZE / 4u];
    uint32_t stop = normalize_ring_pointer(raw_stop);
    semu_error error;
    semu_transaction_result result;

    if (gpu->backend_submit == NULL) {
        gpu->previous_ring_stop = stop;
        return;
    }
    if (!is_sram_range(address, size) || size % 4u != 0u ||
        gpu->previous_ring_stop < address ||
        gpu->previous_ring_stop >= address + size ||
        stop < address || stop >= address + size) {
        gpu->previous_ring_stop = stop;
        return;
    }

    {
        uint32_t word_count = size / 4u;
        uint32_t old_word = (gpu->previous_ring_stop - address) / 4u;
        uint32_t new_word = (stop - address) / 4u;
        uint32_t submitted = (new_word + word_count - old_word) % word_count;

        if (submitted == 0u) {
            gpu->previous_ring_stop = stop;
            return;
        }

        semu_error_clear(&error);
        if (new_word >= old_word) {
            result = gpu->backend_submit(gpu->backend_context,
                gpu->bus, gpu->previous_ring_stop, submitted, 0u,
                gpu->frame_callback, gpu->frame_context, &error);
        } else {
            result = gpu->backend_submit(gpu->backend_context,
                gpu->bus, address, word_count, 0u,
                gpu->frame_callback, gpu->frame_context, &error);
        }

        if (result == SEMU_TRANSACTION_OK) {
            gpu->registers[NEMA_REG_INTERRUPT / 4u] = 1u;
            gpu->frame_generation++;
            gpu->registers[NEMA_REG_FRAME_GEN / 4u] =
                gpu->frame_generation;
            if (gpu->irq_sink != NULL) {
                gpu->irq_sink(gpu->irq_context, NEMA_GPU_IRQ, 1);
            }
        }
    }
    gpu->previous_ring_stop = stop;
}

static semu_status nema_gpu_read(void *context, uint32_t offset,
    unsigned width, uint32_t *value, semu_error *error)
{
    semu_nema_gpu *gpu = (semu_nema_gpu *)context;
    (void)error;
    if (gpu == NULL || value == NULL) {
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || offset >= NEMA_GPU_SIZE) {
        *value = 0u;
        return SEMU_OK;
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
    (void)error;
    if (gpu == NULL) {
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || offset >= NEMA_GPU_SIZE) {
        return SEMU_OK;
    }
    gpu->registers[offset / 4u] = value;

    if (is_command_register(offset)) {
        if (gpu->initialization_complete &&
            offset == NEMA_REG_CMDRINGSTOP) {
            if ((value & NEMA_RING_CTRL_MASK) == NEMA_BOOTSTRAP_CTRL) {
                gpu->previous_ring_stop = normalize_ring_pointer(value);
                return SEMU_OK;
            }
            trigger_rendering(gpu, value);
        } else if (offset == NEMA_REG_STATUS && value == 0u &&
                   ring_is_configured(gpu)) {
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
    if (gpu != NULL) {
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
    semu_display_backend_submit_fn backend_submit,
    void *backend_context,
    semu_frame_callback frame_callback,
    void *frame_context,
    semu_apollo4_irq_fn irq_sink,
    void *irq_context,
    semu_error *error)
{
    semu_nema_gpu *gpu;
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "nema_gpu: null bus");
        return NULL;
    }
    gpu = (semu_nema_gpu *)calloc(1u, sizeof(*gpu));
    if (gpu == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate nema_gpu");
        return NULL;
    }
    gpu->bus = bus;
    gpu->backend_submit = backend_submit;
    gpu->backend_context = backend_context;
    gpu->frame_callback = frame_callback;
    gpu->frame_context = frame_context;
    gpu->irq_sink = irq_sink;
    gpu->irq_context = irq_context;
    return gpu;
}

void semu_nema_gpu_destroy(semu_nema_gpu *gpu)
{
    free(gpu);
}

void semu_nema_gpu_reset(semu_nema_gpu *gpu)
{
    nema_gpu_reset_impl(gpu);
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
