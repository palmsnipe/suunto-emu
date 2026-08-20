#include "machine_internal.h"

#include "../core/bus_internal.h"
#include "../core/scheduler_internal.h"
#include "../core/snapshot_io.h"
#include "../core/storage_internal.h"
#include "../cpu/armv7m/armv7m_internal.h"
#include "../devices/sapporo_devices_internal.h"
#include "../soc/apollo4/apollo4_internal.h"
#include "machine_snapshot_events.h"
#include "machine_snapshot_scheduler.h"
#include <stdlib.h>
#include <string.h>

typedef struct layer_image {
    char id[SEMU_ID_MAX];
    uint8_t enabled;
    uint64_t hits;
    uint64_t intervention_hits[SEMU_SAPPORO_222_IV_COUNT];
    size_t intervention_count;
} layer_image;

typedef struct machine_image {
    uint64_t instruction_epoch;
    uint64_t virtual_time_epoch;
    semu_stop_reason stop_reason;
    layer_image layers[SEMU_MAX_LAYERS];
    size_t layer_count;
} machine_image;

static semu_status add_section(semu_snapshot *snapshot, uint32_t id,
                               semu_snapshot_writer *writer, semu_error *error)
{
    return semu_snapshot_write_section(snapshot, id, writer->data,
                                       writer->size, error);
}

static semu_status write_storage(const semu_machine *machine,
                                 semu_snapshot_writer *writer,
                                 semu_error *error)
{
    uint8_t present = machine->flash_storage != NULL;
    if (semu_snapshot_writer_u8(writer, present, error) != SEMU_OK)
        return error->code;
    if (present != 0u && semu_storage_snapshot_write(
            machine->flash_storage, writer, error) != SEMU_OK)
        return error->code;
    return SEMU_OK;
}

static semu_status read_storage(semu_machine *machine,
                                semu_snapshot_reader *reader,
                                semu_error *error)
{
    uint8_t present;
    if (semu_snapshot_reader_u8(reader, &present, error) != SEMU_OK)
        return error->code;
    if (present > 1u || (present != 0u) != (machine->flash_storage != NULL)) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "snapshot storage presence differs from machine");
        return SEMU_ERR_CONFLICT;
    }
    if (present != 0u && semu_storage_snapshot_read(
            machine->flash_storage, reader, error) != SEMU_OK)
        return error->code;
    return SEMU_OK;
}

static semu_status write_machine(const semu_machine *machine,
                                 semu_snapshot_writer *writer, semu_error *error)
{
    size_t i, j;
    if (semu_snapshot_writer_u64(writer, machine->instruction_epoch, error) != SEMU_OK ||
        semu_snapshot_writer_u64(writer, machine->virtual_time_epoch, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, (uint32_t)machine->stop_reason, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, (uint32_t)machine->layer_count, error) != SEMU_OK)
        return error->code;
    for (i = 0u; i < machine->layer_count; ++i) {
        const semu_layer_state *layer = &machine->layers[i];
        size_t length = layer->descriptor == NULL ? 0u : strlen(layer->descriptor->id);
        if (length >= SEMU_ID_MAX || layer->descriptor == NULL ||
            layer->descriptor->intervention_count > SEMU_SAPPORO_222_IV_COUNT ||
            semu_snapshot_writer_u32(writer, (uint32_t)length, error) != SEMU_OK ||
            semu_snapshot_writer_bytes(writer, (const uint8_t *)layer->descriptor->id,
                                       length, error) != SEMU_OK ||
            semu_snapshot_writer_u8(writer, (uint8_t)(layer->enabled != 0), error) != SEMU_OK ||
            semu_snapshot_writer_u64(writer, layer->hits, error) != SEMU_OK ||
            semu_snapshot_writer_u32(writer,
                (uint32_t)layer->descriptor->intervention_count, error) != SEMU_OK)
            return error->code != SEMU_OK ? error->code : SEMU_ERR_FORMAT;
        for (j = 0u; j < layer->descriptor->intervention_count; ++j)
            if (semu_snapshot_writer_u64(writer,
                    layer->descriptor->interventions[j].hits, error) != SEMU_OK)
                return error->code;
    }
    return SEMU_OK;
}

static semu_status read_machine(semu_snapshot_reader *reader,
                                machine_image *image, semu_error *error)
{
    uint32_t reason, count;
    size_t i, j;
    memset(image, 0, sizeof(*image));
    if (semu_snapshot_reader_u64(reader, &image->instruction_epoch, error) != SEMU_OK ||
        semu_snapshot_reader_u64(reader, &image->virtual_time_epoch, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &reason, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &count, error) != SEMU_OK)
        return error->code;
    if (reason > (uint32_t)SEMU_STOP_USER || count > SEMU_MAX_LAYERS) {
        semu_error_set(error, SEMU_ERR_FORMAT, "invalid machine snapshot header");
        return SEMU_ERR_FORMAT;
    }
    image->stop_reason = (semu_stop_reason)reason;
    image->layer_count = count;
    for (i = 0u; i < image->layer_count; ++i) {
        layer_image *layer = &image->layers[i];
        uint32_t length, intervention_count;
        uint8_t enabled;
        if (semu_snapshot_reader_u32(reader, &length, error) != SEMU_OK)
            return error->code;
        if (length == 0u || length >= SEMU_ID_MAX) {
            semu_error_set(error, SEMU_ERR_FORMAT, "invalid machine layer id");
            return SEMU_ERR_FORMAT;
        }
        if (semu_snapshot_reader_bytes(reader, (uint8_t *)layer->id, length,
                                       error) != SEMU_OK ||
            semu_snapshot_reader_u8(reader, &enabled, error) != SEMU_OK ||
            semu_snapshot_reader_u64(reader, &layer->hits, error) != SEMU_OK ||
            semu_snapshot_reader_u32(reader, &intervention_count, error) != SEMU_OK)
            return error->code;
        layer->id[length] = '\0';
        if (memchr(layer->id, '\0', length) != NULL || enabled > 1u || intervention_count > SEMU_SAPPORO_222_IV_COUNT) {
            semu_error_set(error, SEMU_ERR_FORMAT, "invalid machine layer state");
            return SEMU_ERR_FORMAT;
        }
        layer->enabled = enabled;
        layer->intervention_count = intervention_count;
        for (j = 0u; j < layer->intervention_count; ++j)
            if (semu_snapshot_reader_u64(reader,
                    &layer->intervention_hits[j], error) != SEMU_OK)
                return error->code;
    }
    return SEMU_OK;
}

static semu_status apply_machine_image(semu_machine *machine,
                                       const machine_image *image,
                                       semu_error *error)
{
    size_t i, j;
    if (image->layer_count != machine->layer_count) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "snapshot layer set differs from machine");
        return SEMU_ERR_CONFLICT;
    }
    for (i = 0u; i < image->layer_count; ++i) {
        semu_layer_state *layer = &machine->layers[i];
        if (layer->descriptor == NULL || strcmp(layer->descriptor->id,
                                                 image->layers[i].id) != 0 ||
            layer->descriptor->intervention_count != image->layers[i].intervention_count) {
            semu_error_set(error, SEMU_ERR_CONFLICT, "snapshot layer identity differs from machine");
            return SEMU_ERR_CONFLICT;
        }
    }
    machine->instruction_epoch = image->instruction_epoch;
    machine->virtual_time_epoch = image->virtual_time_epoch;
    machine->stop_reason = image->stop_reason;
    for (i = 0u; i < image->layer_count; ++i) {
        semu_layer_state *layer = &machine->layers[i];
        layer->enabled = image->layers[i].enabled;
        layer->hits = image->layers[i].hits;
        for (j = 0u; j < image->layers[i].intervention_count; ++j)
            ((semu_layer_intervention *)&layer->descriptor->interventions[j])->hits =
                image->layers[i].intervention_hits[j];
    }
    return SEMU_OK;
}

static semu_status resolve_event(semu_machine *machine,
                                 const semu_scheduled_event_state *state,
                                 semu_event_callback *callback, void **context,
                                 semu_error *error)
{
    if (state->kind == SEMU_SCHED_EVENT_SYSTICK)
        return semu_cpu_snapshot_resolve_event(machine->cpu, state->kind,
                                               state->subject, callback, context, error);
    if (state->kind == SEMU_SCHED_EVENT_CTIMER ||
        state->kind == SEMU_SCHED_EVENT_STIMER ||
        state->kind == SEMU_SCHED_EVENT_UART_RX ||
        state->kind == SEMU_SCHED_EVENT_UART_TX)
        return semu_apollo4_snapshot_resolve_event(machine->soc, state->kind,
                                                   state->subject, callback, context, error);
    if (state->kind == SEMU_SCHED_EVENT_CXD_RX ||
        state->kind == SEMU_SCHED_EVENT_CXD_AWAKE)
        return semu_sapporo_devices_snapshot_resolve_event(
            machine->devices, state->subject, callback, context, error);
    if (state->kind == SEMU_SCHED_EVENT_NEMA_COMPLETION)
        return semu_nema_gpu_snapshot_resolve_event(
            machine->nema_gpu, state->subject, callback, context, error);
    semu_error_set(error, SEMU_ERR_FORMAT, "snapshot event kind is unsupported");
    return SEMU_ERR_FORMAT;
}

static semu_status apply_sections(semu_machine *machine,
                                  const semu_snapshot *snapshot,
                                  semu_error *error)
{
    const uint8_t *data;
    size_t size;
    semu_snapshot_reader reader;
    semu_machine_scheduler_image scheduler_image = {0};
    machine_image machine_image;
    uint64_t virtual_time;
    uint32_t stop_reason;
    size_t index;
    semu_status status;
#define SECTION(id) do { \
    if (semu_snapshot_read_section(snapshot, (id), &data, &size) != SEMU_OK) { \
        semu_error_set(error, SEMU_ERR_FORMAT, "snapshot section %u is missing", (id)); \
        semu_machine_snapshot_free_scheduler_image(&scheduler_image); return SEMU_ERR_FORMAT; \
    } \
    semu_snapshot_reader_init(&reader, data, size); \
} while (0)
#define DONE(name) do { if (!semu_snapshot_reader_done(&reader)) { \
    semu_error_set(error, SEMU_ERR_FORMAT, "snapshot %s section has trailing data", (name)); \
    semu_machine_snapshot_free_scheduler_image(&scheduler_image); return SEMU_ERR_FORMAT; } } while (0)
    SECTION(SEMU_SNAPSHOT_SECTION_MACHINE);
    status = read_machine(&reader, &machine_image, error);
    if (status != SEMU_OK) return status;
    DONE("machine");
    SECTION(SEMU_SNAPSHOT_SECTION_SCHEDULER);
    status = semu_machine_snapshot_read_scheduler(&reader, &scheduler_image, error);
    if (status != SEMU_OK) return status;
    DONE("scheduler");
    SECTION(SEMU_SNAPSHOT_SECTION_VIRTUAL_TIME);
    if (semu_snapshot_reader_u64(&reader, &virtual_time, error) != SEMU_OK) {
        semu_machine_snapshot_free_scheduler_image(&scheduler_image); return error->code;
    }
    DONE("virtual-time");
    SECTION(SEMU_SNAPSHOT_SECTION_STOP_REASON);
    if (semu_snapshot_reader_u32(&reader, &stop_reason, error) != SEMU_OK ||
        stop_reason > (uint32_t)SEMU_STOP_USER) {
        semu_machine_snapshot_free_scheduler_image(&scheduler_image);
        if (error->code == SEMU_OK) semu_error_set(error, SEMU_ERR_FORMAT, "invalid stop reason section");
        return error->code;
    }
    DONE("stop-reason");
    if (UINT64_MAX - machine_image.virtual_time_epoch < scheduler_image.now ||
        machine_image.virtual_time_epoch + scheduler_image.now != virtual_time ||
        stop_reason != (uint32_t)machine_image.stop_reason) {
        semu_machine_snapshot_free_scheduler_image(&scheduler_image);
        semu_error_set(error, SEMU_ERR_CONFLICT, "snapshot machine time/reason disagrees");
        return SEMU_ERR_CONFLICT;
    }
    SECTION(SEMU_SNAPSHOT_SECTION_RAM);
    status = semu_bus_snapshot_read(machine->bus, &reader, error);
    if (status == SEMU_OK) DONE("RAM");
    else { semu_machine_snapshot_free_scheduler_image(&scheduler_image); return status; }
    SECTION(SEMU_SNAPSHOT_SECTION_CPU_STATE);
    status = semu_cpu_snapshot_read(machine->cpu, &reader, error);
    if (status == SEMU_OK) DONE("CPU");
    else { semu_machine_snapshot_free_scheduler_image(&scheduler_image); return status; }
    SECTION(SEMU_SNAPSHOT_SECTION_SOC_STATE);
    status = semu_apollo4_snapshot_read(machine->soc, &reader, error);
    if (status == SEMU_OK) DONE("SoC");
    else { semu_machine_snapshot_free_scheduler_image(&scheduler_image); return status; }
    SECTION(SEMU_SNAPSHOT_SECTION_DEVICES);
    status = semu_sapporo_devices_snapshot_read(machine->devices, &reader, error);
    if (status == SEMU_OK) DONE("devices");
    else { semu_machine_snapshot_free_scheduler_image(&scheduler_image); return status; }
    SECTION(SEMU_SNAPSHOT_SECTION_STORAGE);
    status = read_storage(machine, &reader, error);
    if (status == SEMU_OK) DONE("storage");
    else { semu_machine_snapshot_free_scheduler_image(&scheduler_image); return status; }
    SECTION(SEMU_SNAPSHOT_SECTION_NEMA);
    status = semu_nema_gpu_snapshot_read(machine->nema_gpu, &reader, error);
    if (status == SEMU_OK) DONE("NEMA");
    else { semu_machine_snapshot_free_scheduler_image(&scheduler_image); return status; }
    status = semu_machine_snapshot_validate_event_links(
        machine, scheduler_image.events, scheduler_image.count, error);
    if (status != SEMU_OK) {
        semu_machine_snapshot_free_scheduler_image(&scheduler_image);
        return status;
    }
    status = semu_scheduler_restore_begin(machine->scheduler,
        scheduler_image.now, scheduler_image.next_sequence,
        scheduler_image.next_id, error);
    if (status != SEMU_OK) { semu_machine_snapshot_free_scheduler_image(&scheduler_image); return status; }
    for (index = 0u; index < scheduler_image.count; ++index) {
        semu_event_callback callback;
        void *context;
        status = semu_machine_snapshot_validate_event_id(
            machine, &scheduler_image.events[index], error);
        if (status == SEMU_OK)
            status = resolve_event(machine, &scheduler_image.events[index],
                                   &callback, &context, error);
        if (status == SEMU_OK)
            status = semu_scheduler_restore_event(machine->scheduler,
                &scheduler_image.events[index], callback, context, error);
        if (status != SEMU_OK) {
            semu_machine_snapshot_free_scheduler_image(&scheduler_image);
            return status;
        }
    }
    status = apply_machine_image(machine, &machine_image, error);
    semu_machine_snapshot_free_scheduler_image(&scheduler_image);
    if (status != SEMU_OK) return status;
    semu_log_set_time(machine->logger, semu_scheduler_now(machine->scheduler));
#undef SECTION
#undef DONE
    return SEMU_OK;
}

semu_status semu_machine_snapshot_save(const semu_machine *machine,
                                       semu_snapshot *snapshot,
                                       semu_error *error)
{
    semu_snapshot *built;
    semu_snapshot_writer writer;
    char firmware_hash[SEMU_REPLAY_HASH_HEX_LEN];
    const uint32_t ids[] = {
        SEMU_SNAPSHOT_SECTION_CPU_STATE, SEMU_SNAPSHOT_SECTION_RAM,
        SEMU_SNAPSHOT_SECTION_VIRTUAL_TIME, SEMU_SNAPSHOT_SECTION_STOP_REASON,
        SEMU_SNAPSHOT_SECTION_SCHEDULER, SEMU_SNAPSHOT_SECTION_SOC_STATE,
        SEMU_SNAPSHOT_SECTION_DEVICES, SEMU_SNAPSHOT_SECTION_STORAGE,
        SEMU_SNAPSHOT_SECTION_NEMA, SEMU_SNAPSHOT_SECTION_MACHINE
    };
    size_t index;
    semu_status status;
#define BEGIN() semu_snapshot_writer_init(&writer)
#define FINISH(id) do { status = add_section(built, (id), &writer, error); \
    semu_snapshot_writer_destroy(&writer); if (status != SEMU_OK) goto fail; } while (0)
    if (machine == NULL || snapshot == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "machine snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    status = semu_machine_snapshot_manifest_hash(&machine->firmware,
                                                 firmware_hash, error);
    if (status != SEMU_OK) return status;
    built = semu_snapshot_create(error);
    if (built == NULL) return error->code;
    status = semu_snapshot_set_identity(built, machine->profile.id,
                                        firmware_hash, error);
    if (status != SEMU_OK) goto fail;
    status = semu_machine_snapshot_validate_scheduler_events(machine, error);
    if (status != SEMU_OK) goto fail;
    BEGIN(); status = semu_cpu_snapshot_write(machine->cpu, &writer, error); if (status != SEMU_OK) goto fail_writer; FINISH(SEMU_SNAPSHOT_SECTION_CPU_STATE);
    BEGIN(); status = semu_bus_snapshot_write(machine->bus, &writer, error); if (status != SEMU_OK) goto fail_writer; FINISH(SEMU_SNAPSHOT_SECTION_RAM);
    BEGIN(); status = semu_snapshot_writer_u64(&writer, semu_machine_virtual_time(machine), error); if (status != SEMU_OK) goto fail_writer; FINISH(SEMU_SNAPSHOT_SECTION_VIRTUAL_TIME);
    BEGIN(); status = semu_snapshot_writer_u32(&writer, (uint32_t)machine->stop_reason, error); if (status != SEMU_OK) goto fail_writer; FINISH(SEMU_SNAPSHOT_SECTION_STOP_REASON);
    BEGIN();
    status = semu_machine_snapshot_write_scheduler(machine->scheduler, &writer, error);
    if (status == SEMU_OK) {
        semu_snapshot_reader scheduler_reader;
        semu_machine_scheduler_image scheduler_image = {0};
        semu_snapshot_reader_init(&scheduler_reader, writer.data, writer.size);
        status = semu_machine_snapshot_read_scheduler(
            &scheduler_reader, &scheduler_image, error);
        if (status == SEMU_OK && !semu_snapshot_reader_done(&scheduler_reader)) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                           "scheduler snapshot has trailing data");
            status = SEMU_ERR_FORMAT;
        }
        if (status == SEMU_OK)
            status = semu_machine_snapshot_validate_event_links(
                machine, scheduler_image.events, scheduler_image.count, error);
        semu_machine_snapshot_free_scheduler_image(&scheduler_image);
    }
    if (status != SEMU_OK) goto fail_writer;
    FINISH(SEMU_SNAPSHOT_SECTION_SCHEDULER);
    BEGIN(); status = semu_apollo4_snapshot_write(machine->soc, &writer, error); if (status != SEMU_OK) goto fail_writer; FINISH(SEMU_SNAPSHOT_SECTION_SOC_STATE);
    BEGIN(); status = semu_sapporo_devices_snapshot_write(machine->devices, &writer, error); if (status != SEMU_OK) goto fail_writer; FINISH(SEMU_SNAPSHOT_SECTION_DEVICES);
    BEGIN(); status = write_storage(machine, &writer, error); if (status != SEMU_OK) goto fail_writer; FINISH(SEMU_SNAPSHOT_SECTION_STORAGE);
    BEGIN(); status = semu_nema_gpu_snapshot_write(machine->nema_gpu, &writer, error); if (status != SEMU_OK) goto fail_writer; FINISH(SEMU_SNAPSHOT_SECTION_NEMA);
    BEGIN(); status = write_machine(machine, &writer, error); if (status != SEMU_OK) goto fail_writer; FINISH(SEMU_SNAPSHOT_SECTION_MACHINE);
    semu_snapshot_reset(snapshot);
    for (index = 0u; index < SEMU_ARRAY_LEN(ids); ++index) {
        const uint8_t *data;
        size_t size;
        status = semu_snapshot_read_section(built, ids[index], &data, &size);
        if (status != SEMU_OK || semu_snapshot_write_section(snapshot, ids[index], data, size, error) != SEMU_OK) {
            status = status != SEMU_OK ? status : error->code;
            goto fail;
        }
    }
    status = semu_snapshot_set_identity(snapshot, machine->profile.id,
                                        firmware_hash, error);
    semu_snapshot_destroy(built);
    return status;
fail_writer:
    semu_snapshot_writer_destroy(&writer);
fail:
    semu_snapshot_destroy(built);
    return status;
#undef BEGIN
#undef FINISH
}

semu_status semu_machine_snapshot_load(semu_machine *machine,
                                       const semu_snapshot *snapshot,
                                       semu_error *error)
{
    semu_snapshot *backup;
    char firmware_hash[SEMU_REPLAY_HASH_HEX_LEN];
    semu_error original;
    semu_error rollback_error;
    semu_status status;
    if (machine == NULL || snapshot == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "machine snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    status = semu_machine_snapshot_manifest_hash(&machine->firmware,
                                                 firmware_hash, error);
    if (status != SEMU_OK) return status;
    if (semu_snapshot_profile_id(snapshot) == NULL ||
        semu_snapshot_firmware_hash(snapshot) == NULL ||
        strcmp(semu_snapshot_profile_id(snapshot), machine->profile.id) != 0 ||
        strcmp(semu_snapshot_firmware_hash(snapshot), firmware_hash) != 0) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "snapshot identity does not match machine");
        return SEMU_ERR_CONFLICT;
    }
    backup = semu_snapshot_create(error);
    if (backup == NULL) return error->code;
    status = semu_machine_snapshot_save(machine, backup, error);
    if (status == SEMU_OK) status = apply_sections(machine, snapshot, error);
    if (status != SEMU_OK) {
        original = *error;
        semu_error_clear(&rollback_error);
        (void)apply_sections(machine, backup, &rollback_error);
        *error = original;
    }
    semu_snapshot_destroy(backup);
    return status;
}
