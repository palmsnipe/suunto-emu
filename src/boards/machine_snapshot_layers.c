#include "machine_internal.h"
#include "../compat/sapporo_239.h"
#include "../compat/sapporo_239_gps_awake.h"
#include <string.h>

static semu_status validate_awake(const semu_machine *machine,
    const semu_machine_image *image, semu_error *error)
{
    semu_layer_state states[SEMU_MAX_LAYERS];
    semu_layer_descriptor descriptors[SEMU_MAX_LAYERS];
    semu_layer_intervention counters[SEMU_MAX_LAYERS][SEMU_SAPPORO_222_IV_COUNT];
    const semu_layer_state *awake = NULL, *startup = NULL, *reopen = NULL;
    size_t i, j;
    for (i = 0u; i < machine->layer_count; ++i) {
        states[i] = machine->layers[i];
        if (image != NULL) {
            descriptors[i] = *states[i].descriptor;
            for (j = 0u; j < descriptors[i].intervention_count; ++j) {
                counters[i][j] = descriptors[i].interventions[j];
                counters[i][j].hits = image->layers[i].intervention_hits[j];
            }
            descriptors[i].interventions = counters[i];
            states[i].descriptor = &descriptors[i];
            states[i].hits = image->layers[i].hits;
            states[i].enabled = image->layers[i].enabled;
        }
        if (semu_sapporo_239_gps_awake_is_layer(states[i].descriptor)) awake = &states[i];
        if (semu_sapporo_239_gps_is_layer(states[i].descriptor)) startup = &states[i];
        if (semu_sapporo_239_gps_reopen_is_layer(states[i].descriptor)) reopen = &states[i];
    }
    if (awake != NULL && semu_sapporo_239_gps_awake_validate(
            awake, startup, reopen, error) != SEMU_OK) return SEMU_ERR_FORMAT;
    if (strcmp(machine->profile.id, "sapporo-2.39.20") != 0) return SEMU_OK;
    for (i = 0u; i < semu_scheduler_event_count(machine->scheduler); ++i)
        if (semu_scheduler_event_get(machine->scheduler, i)->kind == SEMU_SCHED_EVENT_CXD_AWAKE &&
            (awake == NULL || awake->hits == 0u)) {
            semu_error_set(error, SEMU_ERR_FORMAT, "snapshot awake pulse has no attributed hit");
            return SEMU_ERR_FORMAT;
        }
    return SEMU_OK;
}

semu_status semu_machine_snapshot_write_layers(const semu_machine *machine,
                                 semu_snapshot_writer *writer, semu_error *error)
{
    size_t i, j;
    int has_sapporo_239 = 0;
    if (validate_awake(machine, NULL, error) != SEMU_OK) return SEMU_ERR_FORMAT;
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
        if (layer->descriptor == &semu_sapporo_239_wbsto_layer)
            has_sapporo_239 = 1;
    }
    if (has_sapporo_239)
        return semu_sapporo_239_files_snapshot_write(
            machine->sapporo_239_files, writer, error);
    return SEMU_OK;
}

semu_status semu_machine_snapshot_read_layers(semu_machine *machine,
                                semu_snapshot_reader *reader,
                                semu_machine_image *image, semu_error *error)
{
    uint32_t reason, count;
    size_t i, j;
    int has_sapporo_239 = 0;
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
        semu_machine_layer_image *layer = &image->layers[i];
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
        if (strcmp(layer->id, semu_sapporo_239_wbsto_layer.id) == 0)
            has_sapporo_239 = 1;
        for (j = 0u; j < layer->intervention_count; ++j)
            if (semu_snapshot_reader_u64(reader,
                    &layer->intervention_hits[j], error) != SEMU_OK)
                return error->code;
    }
    if (has_sapporo_239 && !semu_snapshot_reader_done(reader))
        return semu_sapporo_239_files_snapshot_read(
            machine->sapporo_239_files, reader, error);
    semu_sapporo_239_files_reset(machine->sapporo_239_files);
    return SEMU_OK;
}

semu_status semu_machine_snapshot_apply_layers(semu_machine *machine,
                                       const semu_machine_image *image,
                                       semu_error *error)
{
    size_t i, j;
    if (image->layer_count != machine->layer_count) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "snapshot layer set differs from machine");
        return SEMU_ERR_CONFLICT;
    }
    for (i = 0u; i < image->layer_count; ++i) {
        semu_layer_state *layer = &machine->layers[i];
        uint64_t total = 0u;
        int old_sapporo_239 = layer->descriptor ==
                &semu_sapporo_239_wbsto_layer &&
            (image->layers[i].intervention_count == 2u ||
             image->layers[i].intervention_count == 3u);
        if (layer->descriptor == NULL || strcmp(layer->descriptor->id,
                                                 image->layers[i].id) != 0 ||
            (layer->descriptor->intervention_count !=
                 image->layers[i].intervention_count && !old_sapporo_239)) {
            semu_error_set(error, SEMU_ERR_CONFLICT, "snapshot layer identity differs from machine");
            return SEMU_ERR_CONFLICT;
        }
        if (image->layers[i].hits > layer->descriptor->maximum_hits) {
            semu_error_set(error, SEMU_ERR_FORMAT, "snapshot layer hit budget exceeded");
            return SEMU_ERR_FORMAT;
        }
        for (j = 0u; j < image->layers[i].intervention_count; ++j) {
            uint64_t hits = image->layers[i].intervention_hits[j];
            if (layer->descriptor->interventions == NULL ||
                hits > layer->descriptor->interventions[j].max_hits ||
                UINT64_MAX - total < hits) {
                semu_error_set(error, SEMU_ERR_FORMAT, "invalid snapshot intervention hits");
                return SEMU_ERR_FORMAT;
            }
            total += hits;
        }
        /* semu_layer_hit can also record hits without an intervention. */
        if ((semu_sapporo_239_gps_is_layer(layer->descriptor) ||
             semu_sapporo_239_gps_reopen_is_layer(layer->descriptor)) &&
            (!image->layers[i].enabled ||
             !semu_sapporo_239_gps_counts_valid(image->layers[i].hits,
                image->layers[i].intervention_hits[0],
                image->layers[i].intervention_hits[1]))) {
            semu_error_set(error, SEMU_ERR_FORMAT, "invalid GPS startup lifecycle");
            return SEMU_ERR_FORMAT;
        }
        if (total > image->layers[i].hits) {
            semu_error_set(error, SEMU_ERR_FORMAT, "snapshot intervention hits exceed total");
            return SEMU_ERR_FORMAT;
        }
    }
    if (validate_awake(machine, image, error) != SEMU_OK) return SEMU_ERR_FORMAT;
    machine->instruction_epoch = image->instruction_epoch;
    machine->virtual_time_epoch = image->virtual_time_epoch;
    machine->stop_reason = image->stop_reason;
    for (i = 0u; i < image->layer_count; ++i) {
        semu_layer_state *layer = &machine->layers[i];
        layer->enabled = image->layers[i].enabled;
        layer->hits = image->layers[i].hits;
        for (j = 0u; j < layer->descriptor->intervention_count; ++j)
            ((semu_layer_intervention *)&layer->descriptor->interventions[j])->hits = 0u;
        for (j = 0u; j < image->layers[i].intervention_count; ++j)
            ((semu_layer_intervention *)&layer->descriptor->interventions[j])->hits =
                image->layers[i].intervention_hits[j];
    }
    return SEMU_OK;
}
