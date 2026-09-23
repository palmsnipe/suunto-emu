#include "machine_internal.h"

#include "../core/bus_internal.h"
#include "../core/scheduler_internal.h"
#include "../core/snapshot_io.h"
#include "../core/storage_internal.h"
#include "../cpu/armv7m/armv7m_internal.h"
#include "../compat/sapporo_239.h"
#include "../compat/sapporo_239_gps.h"
#include "../compat/sapporo_239_gps_reopen.h"
#include "../devices/sapporo_devices_internal.h"
#include "../soc/apollo4/apollo4_internal.h"
#include "machine_snapshot_events.h"
#include "machine_snapshot_scheduler.h"
#include <stdlib.h>
#include <string.h>

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
    semu_machine_image machine_image;
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
    status = semu_machine_snapshot_read_layers(machine, &reader, &machine_image, error);
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
    SECTION(SEMU_SNAPSHOT_SECTION_DISPLAY);
    status = semu_machine_snapshot_read_display(machine, &reader, error);
    if (status == SEMU_OK) DONE("display");
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
    status = semu_machine_snapshot_apply_layers(machine, &machine_image, error);
    semu_machine_snapshot_free_scheduler_image(&scheduler_image);
    if (status != SEMU_OK) return status;
    if (semu_sapporo_devices_bind_gps_layers(machine->devices, machine->layers,
            machine->layer_count, machine->logger, error) != SEMU_OK)
        return error->code;
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
        SEMU_SNAPSHOT_SECTION_NEMA, SEMU_SNAPSHOT_SECTION_MACHINE,
        SEMU_SNAPSHOT_SECTION_DISPLAY
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
    BEGIN(); status = semu_machine_snapshot_write_layers(machine, &writer, error); if (status != SEMU_OK) goto fail_writer; FINISH(SEMU_SNAPSHOT_SECTION_MACHINE);
    BEGIN(); status = semu_machine_snapshot_write_display(machine, &writer, error); if (status != SEMU_OK) goto fail_writer; FINISH(SEMU_SNAPSHOT_SECTION_DISPLAY);
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
    if (status == SEMU_OK && machine->frame_callback != NULL &&
        machine->display_snapshot.backend_id != 0u) {
        const semu_frame *frame = machine->display_snapshot.published_frame(
            machine->display_backend_context);
        if (frame != NULL) machine->frame_callback(machine->frame_context, frame);
    }
    return status;
}
