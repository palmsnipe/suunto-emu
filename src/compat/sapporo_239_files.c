#include "sapporo_239_files_internal.h"

#include <stdlib.h>
#include <string.h>

#define FILE_COUNT S239_FILE_COUNT
#define LEGACY_FILE_COUNT 11u
#define SNAPSHOT_TAG UINT32_C(0x46393253) /* "S29F" */
#define SNAPSHOT_VERSION 1u

const char *const semu_s239_file_paths[FILE_COUNT] = {
    "settings/sync.txt", "settings/uiv2.txt", "settings/general",
    "logs/entries.bin", "message/history.bin", "sleepln/sleep.bin",
    "tssln/tss.bin", "pois/poi.bin", "actitmln/247.bin",
    "settings/personal", "zapp/storage.sbm", "actitmln/ongoing.bin"
};

const size_t semu_s239_file_capacities[FILE_COUNT] = {
    0u, 235u, 1505u, 12u, 10800u, 17888u,
    2384u, 68272u, 46112u, 1727u, 64u, 152u /* E-SAP-COMPAT-ONGOING-239-001. */
};

int semu_s239_file_index(const char *path)
{
    size_t i;
    if (path == NULL) return -1;
    for (i = 0u; i < FILE_COUNT; ++i)
        if (strcmp(path, semu_s239_file_paths[i]) == 0) return (int)i;
    return -1;
}

s239_handle_slot *semu_s239_find_handle(semu_sapporo_239_files *files,
                                        uint32_t value)
{
    size_t i;
    if (files == NULL) return NULL;
    for (i = 0u; i < S239_FILE_MAX_HANDLES; ++i)
        if (files->handles[i].active && files->handles[i].value == value)
            return &files->handles[i];
    return NULL;
}

semu_sapporo_239_files *semu_sapporo_239_files_create(semu_error *error)
{
    semu_sapporo_239_files *files =
        (semu_sapporo_239_files *)calloc(1u, sizeof(*files));
    if (files == NULL)
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate Sapporo 2.39 logical files");
    return files;
}

void semu_sapporo_239_files_reset(semu_sapporo_239_files *files)
{
    size_t i;
    if (files == NULL) return;
    for (i = 0u; i < FILE_COUNT; ++i) free(files->files[i].data);
    memset(files, 0, sizeof(*files));
}

void semu_sapporo_239_files_destroy(semu_sapporo_239_files *files)
{
    if (files != NULL) {
        semu_sapporo_239_files_reset(files);
        free(files);
    }
}

size_t semu_sapporo_239_files_count(const semu_sapporo_239_files *files)
{
    size_t i, count = 0u;
    if (files == NULL) return 0u;
    for (i = 0u; i < FILE_COUNT; ++i) count += files->files[i].present != 0;
    return count;
}

size_t semu_sapporo_239_file_size(const semu_sapporo_239_files *files,
                                  const char *path)
{
    int index = semu_s239_file_index(path);
    if (files == NULL || index < 0 || !files->files[index].present)
        return SIZE_MAX;
    return files->files[index].size;
}

static semu_status write_files(const semu_sapporo_239_files *files,
                               semu_snapshot_writer *writer,
                               size_t count,
                               semu_error *error)
{
    size_t i;
    for (i = 0u; i < count; ++i) {
        const s239_file_slot *slot = &files->files[i];
        if (semu_snapshot_writer_u8(writer, (uint8_t)(slot->present != 0),
                error) != SEMU_OK) return error->code;
        if (slot->present &&
            (semu_snapshot_writer_u32(writer, (uint32_t)slot->size, error) !=
                 SEMU_OK ||
             semu_snapshot_writer_bytes(writer, slot->data, slot->size,
                                         error) != SEMU_OK))
            return error->code;
    }
    return SEMU_OK;
}

semu_status semu_sapporo_239_files_snapshot_write(
    const semu_sapporo_239_files *files, semu_snapshot_writer *writer,
    semu_error *error)
{
    size_t i, count;
    if (files == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "invalid Sapporo 2.39 file snapshot writer");
        return SEMU_ERR_ARGUMENT;
    }
    /* Preserve historical bytes until the appended path is actually created. */
    count = files->files[LEGACY_FILE_COUNT].present ? FILE_COUNT : LEGACY_FILE_COUNT;
    if (semu_snapshot_writer_u32(writer, SNAPSHOT_TAG, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, SNAPSHOT_VERSION, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, (uint32_t)count, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, files->next_handle, error) != SEMU_OK ||
        write_files(files, writer, count, error) != SEMU_OK)
        return error->code;
    for (i = 0u; i < S239_FILE_MAX_HANDLES; ++i) {
        const s239_handle_slot *slot = &files->handles[i];
        if (semu_snapshot_writer_u8(writer, (uint8_t)(slot->active != 0),
                error) != SEMU_OK) return error->code;
        if (slot->active &&
            (semu_snapshot_writer_u32(writer, slot->value, error) != SEMU_OK ||
             semu_snapshot_writer_u32(writer, slot->cursor, error) != SEMU_OK ||
             semu_snapshot_writer_u8(writer, slot->file, error) != SEMU_OK ||
             semu_snapshot_writer_u8(writer, slot->mode, error) != SEMU_OK))
            return error->code;
    }
    return SEMU_OK;
}

static semu_status read_files(semu_sapporo_239_files *temp,
                              semu_snapshot_reader *reader,
                              size_t count,
                              semu_error *error)
{
    size_t i;
    for (i = 0u; i < count; ++i) {
        uint8_t present;
        uint32_t size;
        if (semu_snapshot_reader_u8(reader, &present, error) != SEMU_OK)
            return error->code;
        if (present > 1u) goto invalid;
        if (!present) continue;
        if (semu_snapshot_reader_u32(reader, &size, error) != SEMU_OK)
            return error->code;
        if (size > semu_s239_file_capacities[i]) goto invalid;
        if (semu_s239_file_capacities[i] != 0u) {
            temp->files[i].data = (uint8_t *)malloc(
                semu_s239_file_capacities[i]);
            if (temp->files[i].data == NULL) {
                semu_error_set(error, SEMU_ERR_NOMEM,
                               "cannot restore Sapporo 2.39 logical file");
                return SEMU_ERR_NOMEM;
            }
            memset(temp->files[i].data, 0, semu_s239_file_capacities[i]);
        }
        if (semu_snapshot_reader_bytes(reader, temp->files[i].data,
                size, error) != SEMU_OK) return error->code;
        temp->files[i].present = 1;
        temp->files[i].size = size;
    }
    return SEMU_OK;
invalid:
    semu_error_set(error, SEMU_ERR_FORMAT,
                   "invalid Sapporo 2.39 logical file snapshot");
    return SEMU_ERR_FORMAT;
}

static semu_status read_handles(semu_sapporo_239_files *temp,
                                semu_snapshot_reader *reader,
                                semu_error *error)
{
    size_t i;
    for (i = 0u; i < S239_FILE_MAX_HANDLES; ++i) {
        s239_handle_slot *slot = &temp->handles[i];
        uint8_t active;
        if (semu_snapshot_reader_u8(reader, &active, error) != SEMU_OK)
            return error->code;
        if (active > 1u) goto invalid;
        if (!active) continue;
        if (semu_snapshot_reader_u32(reader, &slot->value, error) != SEMU_OK ||
            semu_snapshot_reader_u32(reader, &slot->cursor, error) != SEMU_OK ||
            semu_snapshot_reader_u8(reader, &slot->file, error) != SEMU_OK ||
            semu_snapshot_reader_u8(reader, &slot->mode, error) != SEMU_OK)
            return error->code;
        if (slot->value != S239_FILE_HANDLE_BASE + (uint32_t)i *
                S239_FILE_HANDLE_STRIDE || i >= temp->next_handle ||
            slot->file >= FILE_COUNT || !temp->files[slot->file].present ||
            slot->mode < 1u || slot->mode > 3u ||
            slot->cursor > semu_s239_file_capacities[slot->file]) goto invalid;
        slot->active = 1;
    }
    return SEMU_OK;
invalid:
    semu_error_set(error, SEMU_ERR_FORMAT,
                   "invalid Sapporo 2.39 logical handle snapshot");
    return SEMU_ERR_FORMAT;
}

semu_status semu_sapporo_239_files_snapshot_read(
    semu_sapporo_239_files *files, semu_snapshot_reader *reader,
    semu_error *error)
{
    semu_sapporo_239_files temp;
    uint32_t tag, version, count;
    semu_status status;
    if (files == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "invalid Sapporo 2.39 file snapshot reader");
        return SEMU_ERR_ARGUMENT;
    }
    memset(&temp, 0, sizeof(temp));
    status = semu_snapshot_reader_u32(reader, &tag, error);
    if (status == SEMU_OK) status = semu_snapshot_reader_u32(reader, &version, error);
    if (status == SEMU_OK) status = semu_snapshot_reader_u32(reader, &count, error);
    if (status == SEMU_OK) status = semu_snapshot_reader_u32(reader, &temp.next_handle, error);
    if (status == SEMU_OK && (tag != SNAPSHOT_TAG || version != SNAPSHOT_VERSION ||
        (count != FILE_COUNT && count != LEGACY_FILE_COUNT) ||
        temp.next_handle > S239_FILE_MAX_HANDLES)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid Sapporo 2.39 file snapshot header");
        status = SEMU_ERR_FORMAT;
    }
    if (status == SEMU_OK) status = read_files(&temp, reader, count, error);
    if (status == SEMU_OK) status = read_handles(&temp, reader, error);
    if (status == SEMU_OK) {
        semu_sapporo_239_files_reset(files);
        *files = temp;
    } else {
        semu_sapporo_239_files_reset(&temp);
    }
    return status;
}
