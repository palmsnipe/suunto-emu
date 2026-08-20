#include "sapporo_devices_internal.h"

#include "../core/scheduler_internal.h"

semu_status semu_sapporo_devices_snapshot_write(
    const semu_sapporo_devices *devices, semu_snapshot_writer *writer,
    semu_error *error)
{
    uint8_t has_flash;
    if (devices == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo device snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    has_flash = devices->flash != NULL;
    if (semu_snapshot_writer_u8(writer, has_flash, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer,
            (uint8_t)(devices->fixture_context.gps_running_status_armed != 0),
            error) != SEMU_OK ||
        semu_sapporo_hsppad143_snapshot_write(devices->pressure, writer, error) != SEMU_OK ||
        semu_sapporo_lsm6dsl_snapshot_write(devices->accelerometer, writer, error) != SEMU_OK ||
        semu_sapporo_tli493d_snapshot_write(devices->magnetometer, writer, error) != SEMU_OK ||
        semu_sapporo_haptic_snapshot_write(devices->haptic, writer, error) != SEMU_OK ||
        semu_sapporo_opt3007_snapshot_write(devices->ambient_light, writer, error) != SEMU_OK ||
        semu_sapporo_max17050_snapshot_write(devices->battery_gauge, writer, error) != SEMU_OK ||
        semu_sapporo_cxd5610_snapshot_write(devices->gps, writer, error) != SEMU_OK ||
        semu_sapporo_ohr2_snapshot_write(devices->ohr2, writer, error) != SEMU_OK)
        return error->code;
    if (has_flash && semu_sapporo_flash_snapshot_write(devices->flash, writer, error) != SEMU_OK)
        return error->code;
    return SEMU_OK;
}

static semu_status read_child(
    semu_sapporo_devices *devices, semu_snapshot_reader *reader,
    semu_error *error)
{
    uint8_t has_flash, armed;
    if (semu_snapshot_reader_u8(reader, &has_flash, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &armed, error) != SEMU_OK)
        return error->code;
    if (has_flash > 1u || armed > 1u || (has_flash != 0u) != (devices->flash != NULL)) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "Sapporo device snapshot geometry does not match machine");
        return SEMU_ERR_CONFLICT;
    }
    if (semu_sapporo_hsppad143_snapshot_read(devices->pressure, reader, error) != SEMU_OK ||
        semu_sapporo_lsm6dsl_snapshot_read(devices->accelerometer, reader, error) != SEMU_OK ||
        semu_sapporo_tli493d_snapshot_read(devices->magnetometer, reader, error) != SEMU_OK ||
        semu_sapporo_haptic_snapshot_read(devices->haptic, reader, error) != SEMU_OK ||
        semu_sapporo_opt3007_snapshot_read(devices->ambient_light, reader, error) != SEMU_OK ||
        semu_sapporo_max17050_snapshot_read(devices->battery_gauge, reader, error) != SEMU_OK ||
        semu_sapporo_cxd5610_snapshot_read(devices->gps, reader, error) != SEMU_OK ||
        semu_sapporo_ohr2_snapshot_read(devices->ohr2, reader, error) != SEMU_OK)
        return error->code;
    if (has_flash && semu_sapporo_flash_snapshot_read(devices->flash, reader, error) != SEMU_OK)
        return error->code;
    devices->fixture_context.gps_running_status_armed = armed;
    return SEMU_OK;
}

semu_status semu_sapporo_devices_snapshot_read(
    semu_sapporo_devices *devices, semu_snapshot_reader *reader,
    semu_error *error)
{
    semu_snapshot_writer backup;
    semu_snapshot_reader rollback_reader;
    semu_error rollback_error;
    semu_status status;
    size_t offset;

    if (devices == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo device snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    offset = reader->offset;
    semu_snapshot_writer_init(&backup);
    status = semu_sapporo_devices_snapshot_write(devices, &backup, error);
    if (status != SEMU_OK) {
        semu_snapshot_writer_destroy(&backup);
        return status;
    }
    status = read_child(devices, reader, error);
    if (status == SEMU_OK) {
        semu_snapshot_writer_destroy(&backup);
        return SEMU_OK;
    }
    reader->offset = offset;
    semu_snapshot_reader_init(&rollback_reader, backup.data, backup.size);
    semu_error_clear(&rollback_error);
    if (read_child(devices, &rollback_reader, &rollback_error) != SEMU_OK ||
        !semu_snapshot_reader_done(&rollback_reader)) {
        semu_snapshot_writer_destroy(&backup);
        semu_error_set(error, SEMU_ERR_STATE,
                       "Sapporo device snapshot rollback failed");
        return SEMU_ERR_STATE;
    }
    semu_snapshot_writer_destroy(&backup);
    return status;
}

semu_status semu_sapporo_devices_snapshot_resolve_event(
    semu_sapporo_devices *devices, uint32_t subject,
    semu_event_callback *callback, void **context, semu_error *error)
{
    if (devices == NULL || callback == NULL || context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo device snapshot event arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    return semu_sapporo_cxd5610_snapshot_resolve_event(
        devices->gps, subject, callback, context, error);
}

semu_status semu_sapporo_devices_snapshot_event_id_matches(
    const semu_sapporo_devices *devices, uint32_t subject,
    semu_event_id event_id, semu_error *error)
{
    if (devices == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo snapshot event identity requires devices");
        return SEMU_ERR_ARGUMENT;
    }
    return semu_sapporo_cxd5610_snapshot_event_id_matches(
        devices->gps, subject, event_id, error);
}
