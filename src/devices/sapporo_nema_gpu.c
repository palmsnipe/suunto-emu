/*
 * NEMA GPU bus device (E-NEMA-RING-001: SapporoNemaP.cs).
 * Implements the register interface at 0x40090000.  When the firmware
 * writes CMDRINGSTOP after initialization, extracts the new ring
 * entries and submits them to the display backend for rendering.
 * Raises IRQ 28 on successful completion.
 */

#include "sapporo_nema_gpu.h"

#include "../display/nema_completion.h"
#include "../display/nema_framing.h"

#include <stdlib.h>
#include <string.h>

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
    semu_display_backend_submit_fn backend_submit;
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
};

typedef struct {
    semu_nema_gpu *gpu;
    size_t child_count;
    semu_transaction_result result;
} render_submission;

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

static void completion_reg_write(void *context, uint32_t offset,
                                 uint32_t value)
{
    semu_nema_gpu *gpu = (semu_nema_gpu *)context;
    if (gpu != NULL && offset < NEMA_GPU_SIZE && (offset & 3u) == 0u) {
        gpu->registers[offset / 4u] = value;
    }
}

static void completion_irq(void *context, unsigned irq, int asserted)
{
    semu_nema_gpu *gpu = (semu_nema_gpu *)context;
    if (gpu != NULL && irq == NEMA_GPU_IRQ && gpu->irq_sink != NULL) {
        gpu->irq_sink(gpu->irq_context, irq, asserted);
    }
}

static void submit_child(void *context, uint32_t child_address,
                         uint32_t child_entries)
{
    render_submission *submission = (render_submission *)context;
    semu_nema_gpu *gpu = submission->gpu;
    semu_error error;

    if (submission->result != SEMU_TRANSACTION_OK) {
        return;
    }
    if (gpu->backend_submit == NULL || child_entries == 0u) {
        submission->result = SEMU_TRANSACTION_REFUSE;
        return;
    }
    semu_error_clear(&error);
    submission->result = gpu->backend_submit(
        gpu->backend_context, gpu->bus, child_address, child_entries,
        0u, gpu->frame_callback, gpu->frame_context, &error);
    if (submission->result == SEMU_TRANSACTION_OK) {
        ++submission->child_count;
    }
}

static semu_status find_completion_marker(semu_nema_gpu *gpu,
                                           uint32_t ring_base,
                                           uint32_t ring_words,
                                           uint32_t old_word,
                                           uint32_t new_word,
                                           uint32_t *list_id,
                                           int *found,
                                           semu_error *error)
{
    uint32_t submitted;
    uint32_t i;

    if (list_id == NULL || found == NULL || ring_words == 0u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "nema: invalid completion scan arguments");
        return SEMU_ERR_ARGUMENT;
    }
    *found = 0;
    submitted = (new_word - old_word) % ring_words;
    if (submitted == 0u) {
        submitted = ring_words;
    }
    for (i = 0u; i < submitted; ++i) {
        uint32_t index = (old_word + i) % ring_words;
        uint32_t word;
        semu_status status = semu_bus_read(
            gpu->bus, ring_base + index * 4u, 4u, &word, error);
        if (status != SEMU_OK) {
            return status;
        }
        if (word == NEMA_REG_CLID) {
            uint32_t next_index = (index + 1u) % ring_words;
            uint32_t interrupt_index = (index + 2u) % ring_words;
            uint32_t value_index = (index + 3u) % ring_words;
            uint32_t interrupt_word;
            uint32_t value_word;

            status = semu_bus_read(gpu->bus,
                ring_base + next_index * 4u, 4u, list_id, error);
            if (status != SEMU_OK) return status;
            status = semu_bus_read(gpu->bus,
                ring_base + interrupt_index * 4u, 4u,
                &interrupt_word, error);
            if (status != SEMU_OK) return status;
            status = semu_bus_read(gpu->bus,
                ring_base + value_index * 4u, 4u, &value_word, error);
            if (status != SEMU_OK) return status;
            if (interrupt_word == NEMA_REG_INTERRUPT && value_word == 1u) {
                *found = 1;
                return SEMU_OK;
            }
        }
    }
    semu_error_clear(error);
    return SEMU_OK;
}

static void trigger_rendering(semu_nema_gpu *gpu, uint32_t raw_stop)
{
    uint32_t address = gpu->registers[NEMA_REG_CMDADDR / 4u];
    uint32_t size = gpu->registers[NEMA_REG_CMDSIZE / 4u];
    uint32_t stop = normalize_ring_pointer(raw_stop);
    semu_error error;
    semu_transaction_result result;
    int immediate_completion = 0;

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

        {
            render_submission submission;
            uint32_t list_id = 0u;
            int marker_found = 0;
            semu_status status;

            submission.gpu = gpu;
            submission.child_count = 0u;
            submission.result = SEMU_TRANSACTION_OK;
            semu_error_clear(&error);
            status = nema_framing_parse(gpu->bus, address, word_count,
                old_word, new_word, submit_child, &submission, NULL, NULL,
                &error);
            if (status != SEMU_OK) {
                result = SEMU_TRANSACTION_REFUSE;
            } else {
                result = submission.result;
                if (result == SEMU_TRANSACTION_OK) {
                    status = find_completion_marker(gpu, address, word_count,
                        old_word, new_word, &list_id, &marker_found, &error);
                    if (status != SEMU_OK) {
                        result = SEMU_TRANSACTION_REFUSE;
                    } else if (marker_found &&
                               (gpu->scheduler == NULL ||
                                gpu->completion == NULL ||
                                nema_completion_schedule(gpu->completion,
                                    gpu->scheduler, list_id,
                                    completion_reg_write, gpu,
                                    completion_irq, gpu, &error) != SEMU_OK)) {
                        result = SEMU_TRANSACTION_REFUSE;
                    }
                }
                /* The native completion marker is a marker-only ring
                 * transaction after the rendered child list.  It is
                 * consumed by the completion path, not by the raster
                 * backend. */
                if (result == SEMU_TRANSACTION_OK && !marker_found &&
                    submission.child_count == 0u) {
                    result = new_word >= old_word
                        ? gpu->backend_submit(gpu->backend_context, gpu->bus,
                            gpu->previous_ring_stop, submitted, 0u,
                            gpu->frame_callback, gpu->frame_context, &error)
                        : gpu->backend_submit(gpu->backend_context, gpu->bus,
                            address, word_count, 0u, gpu->frame_callback,
                            gpu->frame_context, &error);
                    immediate_completion = 1;
                }
            }
        }

        if (result == SEMU_TRANSACTION_OK) {
            gpu->frame_generation++;
            gpu->registers[NEMA_REG_FRAME_GEN / 4u] =
                gpu->frame_generation;
            if (immediate_completion) {
                gpu->registers[NEMA_REG_INTERRUPT / 4u] = 1u;
                if (gpu->irq_sink != NULL) {
                    gpu->irq_sink(gpu->irq_context, NEMA_GPU_IRQ, 1);
                }
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
    semu_display_backend_submit_fn backend_submit,
    void *backend_context,
    semu_frame_callback frame_callback,
    void *frame_context,
    semu_apollo4_irq_fn irq_sink,
    void *irq_context,
    semu_scheduler *scheduler,
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
    gpu->scheduler = scheduler;
    if (nema_completion_create(&gpu->completion, error) != SEMU_OK) {
        free(gpu);
        return NULL;
    }
    return gpu;
}

void semu_nema_gpu_destroy(semu_nema_gpu *gpu)
{
    if (gpu != NULL) {
        nema_completion_cancel(gpu->completion);
        nema_completion_destroy(gpu->completion);
    }
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

semu_status semu_nema_gpu_snapshot_write(
    const semu_nema_gpu *gpu, semu_snapshot_writer *writer, semu_error *error)
{
    size_t index;
    if (gpu == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "NEMA snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
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
    if (initialized != 0u && !ring_is_configured(&candidate)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "initialized NEMA snapshot has no command ring");
        return SEMU_ERR_FORMAT;
    }
    if (nema_completion_snapshot_read(candidate.completion, reader, error) != SEMU_OK)
        return error->code;
    nema_completion_rebind_active(candidate.completion, completion_reg_write,
                                  gpu, completion_irq, gpu);
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
        gpu->completion, subject, event_id, error);
}
