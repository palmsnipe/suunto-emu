#include "sapporo_239_files_internal.h"

#include <stdlib.h>
#include <string.h>


#define FILE_COUNT S239_FILE_COUNT
#define LEGACY_FILE_COUNT 11u
#define SNAPSHOT_TAG UINT32_C(0x46393253) /* "S29F" */
/* v1: fixed table only, byte-exact historical layout.
   v2 (E-SAP239-REPO38D123-001): once any dynamic storage/<key>/data.jsn
   slot exists, a self-terminating storage section follows the handles:
   per record (u32 one-based slot index, u32 name length excluding the
   terminator, name bytes, u32 payload size, payload), closed by a u32
   zero end marker. Reads accept v1 and v2; v1 bytes never change. */
#define SNAPSHOT_VERSION 2u
#define SNAPSHOT_VERSION_LEGACY 1u

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

int semu_s239_storage_path(const char *path)
{
    size_t prefix = sizeof(S239_STORAGE_PREFIX) - 1u;
    size_t suffix = sizeof(S239_STORAGE_SUFFIX) - 1u;
    size_t length, i;
    if (path == NULL) return 0;
    length = strlen(path);
    if (length <= prefix + suffix || length >= S239_STORAGE_NAME_MAX) return 0;
    if (memcmp(path, S239_STORAGE_PREFIX, prefix) != 0) return 0;
    if (memcmp(path + length - suffix, S239_STORAGE_SUFFIX, suffix) != 0)
        return 0;
    /* E-SAP239-REFUSED-PATH-001 capture (ticket 798, twice-identical):
       the guest builder (0x00198960, "%s%x" pathjoin) composes nested
       key regions; a valid key region is hex segments separated by
       single interior slashes - never a leading, trailing, or doubled
       slash. Flat single-segment keys keep their historical shape. */
    {
        int hex_seen = 0;
        for (i = prefix; i + suffix < length; ++i) {
            char c = path[i];
            if (c == '/') {
                /* A slash right after the prefix or another slash, or
                   one just before the suffix, is not a separator. */
                if (!hex_seen) return 0;
                hex_seen = 0;
                continue;
            }
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
                return 0;
            hex_seen = 1;
        }
        return hex_seen;
    }
}

int semu_s239_storage_index(const semu_sapporo_239_files *files,
                            const char *path)
{
    size_t i;
    if (files == NULL || path == NULL) return -1;
    for (i = 0u; i < S239_STORAGE_SLOTS; ++i)
        if (files->storage[i].present &&
            strcmp(files->storage[i].name, path) == 0)
            return (int)(S239_FILE_COUNT + (uint32_t)i);
    return -1;
}

int semu_s239_storage_admit(semu_sapporo_239_files *files, const char *path)
{
    size_t i;
    int free_index = -1;
    uint8_t *allocation;
    if (files == NULL || !semu_s239_storage_path(path))
        return -(int)SEMU_ERR_STATE;
    /* Scan the append-only name table (the extent, not the present-slot
       count, bounds the names the pool ever admits). */
    for (i = 0u; i < (size_t)files->storage_names; ++i) {
        if (strcmp(files->storage[i].name, path) != 0) continue;
        if (files->storage[i].present)
            return (int)(S239_FILE_COUNT + (uint32_t)i);
        free_index = (int)i;
        break; /* Closed file: native open re-presents the same slot. */
    }
    if (free_index < 0) {
        if (files->storage_names >= S239_STORAGE_SLOTS) return -1;
        free_index = (int)files->storage_names;
    }
    allocation = (uint8_t *)calloc(S239_STORAGE_CAPACITY, 1u);
    if (allocation == NULL) return -(int)SEMU_ERR_NOMEM;
    if (free_index >= (int)files->storage_names) {
        /* Fresh name: append it to the immutable name table. */
        memcpy(files->storage[free_index].name, path, strlen(path) + 1u);
        files->storage_names = (uint32_t)free_index + 1u;
    }
    files->storage[free_index].data = allocation;
    files->storage[free_index].size = 0u;
    files->storage[free_index].present = 1;
    return (int)(S239_FILE_COUNT + (uint32_t)free_index);
}

size_t semu_s239_file_capacity(const semu_sapporo_239_files *files,
                               uint32_t file_index)
{
    (void)files;
    if (file_index < FILE_COUNT)
        return semu_s239_file_capacities[file_index] >
                       S239_FILE_RUNTIME_CAPACITY
            ? semu_s239_file_capacities[file_index]
            : S239_FILE_RUNTIME_CAPACITY;
    return S239_STORAGE_CAPACITY;
}

int semu_s239_slot_present(const semu_sapporo_239_files *files,
                           uint32_t file_index)
{
    if (files == NULL) return 0;
    if (file_index < FILE_COUNT) return files->files[file_index].present;
    if (file_index < S239_FILE_COUNT + S239_STORAGE_SLOTS)
        return files->storage[file_index - S239_FILE_COUNT].present;
    return 0;
}

void semu_s239_slot_name(const semu_sapporo_239_files *files,
                         uint32_t file_index, char out[65])
{
    const char *name = NULL;
    size_t i = 0u;
    if (file_index < FILE_COUNT) name = semu_s239_file_paths[file_index];
    else if (files != NULL &&
             file_index < S239_FILE_COUNT + S239_STORAGE_SLOTS)
        name = files->storage[file_index - S239_FILE_COUNT].name;
    if (name == NULL) { out[0] = '\0'; return; }
    for (; name[i] != '\0' && i < 64u; ++i) out[i] = name[i];
    out[i] = '\0';
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
    for (i = 0u; i < S239_STORAGE_SLOTS; ++i) free(files->storage[i].data);
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
    for (i = 0u; i < S239_STORAGE_SLOTS; ++i)
        count += files->storage[i].present != 0;
    return count;
}

size_t semu_sapporo_239_file_size(const semu_sapporo_239_files *files,
                                  const char *path)
{
    int index;
    int storage;
    if (files == NULL || path == NULL) return SIZE_MAX;
    index = semu_s239_file_index(path);
    if (index >= 0)
        return files->files[index].present ? files->files[index].size
                                           : SIZE_MAX;
    storage = semu_s239_storage_index(files, path);
    if (storage >= 0)
        return files->storage[(size_t)(storage - (int)S239_FILE_COUNT)].size;
    return SIZE_MAX;
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
    size_t i, count, storage_used = 0u;
    uint32_t version;
    if (files == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "invalid Sapporo 2.39 file snapshot writer");
        return SEMU_ERR_ARGUMENT;
    }
    /* Preserve historical bytes until the appended path is actually created. */
    count = files->files[LEGACY_FILE_COUNT].present ? FILE_COUNT : LEGACY_FILE_COUNT;
    for (i = 0u; i < S239_STORAGE_SLOTS; ++i)
        if (files->storage[i].present) storage_used = i + 1u;
    /* Handles keep storage indices after their file closed, so the
       version must not drop once the index space is in use. The name
       table is bounded by S239_FILE_MAX_HANDLES opens, so it is written
       exactly once per snapshot and read back as a count plus names. */
    version = (storage_used != 0u || files->storage_names != 0u)
        ? SNAPSHOT_VERSION : SNAPSHOT_VERSION_LEGACY;
    if (semu_snapshot_writer_u32(writer, SNAPSHOT_TAG, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, version, error) != SEMU_OK ||
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
    if (version == SNAPSHOT_VERSION_LEGACY) return SEMU_OK;
    if (semu_snapshot_writer_u32(writer, (uint32_t)files->storage_names,
        error) != SEMU_OK)
        return error->code;
    for (i = 0u; i < (size_t)files->storage_names; ++i) {
        const s239_storage_slot *slot = &files->storage[i];
        size_t name_length = strlen(slot->name);
        uint8_t present = (uint8_t)(slot->present != 0);
        if (slot->data == NULL || name_length == 0u ||
            name_length >= S239_STORAGE_NAME_MAX ||
            semu_snapshot_writer_u8(writer, present, error) != SEMU_OK ||
            semu_snapshot_writer_u32(writer, (uint32_t)name_length,
                error) != SEMU_OK ||
            semu_snapshot_writer_bytes(writer, (const uint8_t *)slot->name,
                name_length, error) != SEMU_OK ||
            semu_snapshot_writer_u32(writer, (uint32_t)slot->size,
                error) != SEMU_OK ||
            semu_snapshot_writer_bytes(writer, slot->data, slot->size,
                error) != SEMU_OK)
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
        /* Restore bound follows the runtime capacity law
           max(partition size, 1037): guest growth is session state and
           must round-trip; oversized payloads stay fail-closed. */
        {
            size_t bound = semu_s239_file_capacities[i] >
                    S239_FILE_RUNTIME_CAPACITY
                ? semu_s239_file_capacities[i]
                : S239_FILE_RUNTIME_CAPACITY;
            if (size > bound) goto invalid;
            if (bound != 0u) {
                temp->files[i].data = (uint8_t *)malloc(bound);
                if (temp->files[i].data == NULL) {
                    semu_error_set(error, SEMU_ERR_NOMEM,
                                   "cannot restore Sapporo 2.39 logical file");
                    return SEMU_ERR_NOMEM;
                }
                memset(temp->files[i].data, 0, bound);
            }
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
            slot->file >= S239_FILE_COUNT + S239_STORAGE_SLOTS ||
            slot->mode < 1u || slot->mode > 3u ||
            slot->cursor > semu_s239_file_capacity(temp, slot->file))
            goto invalid;
        slot->active = 1;
    }
    /* Fixed-slot presence is validated after the storage section is
       read; v1 and v2 both pass through this function unchanged. */
    return SEMU_OK;
invalid:
    semu_error_set(error, SEMU_ERR_FORMAT,
                   "invalid Sapporo 2.39 logical handle snapshot");
    return SEMU_ERR_FORMAT;
}


/* Records carry their own one-based slot index and the section closes
   with a zero marker, so the reader consumes exactly the written bytes
   and the section framing stays byte-identical across a round trip. */
static semu_status read_storage(semu_sapporo_239_files *temp,
                                semu_snapshot_reader *reader,
                                semu_error *error)
{
    uint32_t names;
    uint32_t i;
    if (semu_snapshot_reader_u32(reader, &names, error) != SEMU_OK)
        return error->code;
    if (names > S239_STORAGE_SLOTS) goto invalid;
    for (i = 0u; i < names; ++i) {
        s239_storage_slot *slot = &temp->storage[i];
        uint8_t present;
        uint32_t name_length, size;
        uint32_t j;
        if (semu_snapshot_reader_u8(reader, &present, error) != SEMU_OK)
            return error->code;
        if (present > 1u) goto invalid;
        if (semu_snapshot_reader_u32(reader, &name_length, error) != SEMU_OK)
            return error->code;
        if (name_length == 0u || name_length >= S239_STORAGE_NAME_MAX ||
            semu_snapshot_reader_bytes(reader, (uint8_t *)slot->name,
                name_length, error) != SEMU_OK) goto invalid;
        slot->name[name_length] = '\0';
        if (!semu_s239_storage_path(slot->name)) goto invalid;
        for (j = 0u; j < i; ++j)
            if (strcmp(temp->storage[j].name, slot->name) == 0) goto invalid;
        if (semu_snapshot_reader_u32(reader, &size, error) != SEMU_OK)
            return error->code;
        if (size > S239_STORAGE_CAPACITY) goto invalid;
        if (!present) continue; /* Closed name; slot stays reserved. */
        slot->data = (uint8_t *)malloc(S239_STORAGE_CAPACITY);
        if (slot->data == NULL) {
            semu_error_set(error, SEMU_ERR_NOMEM,
                           "cannot restore Sapporo 2.39 storage file");
            return SEMU_ERR_NOMEM;
        }
        memset(slot->data, 0, S239_STORAGE_CAPACITY);
        if (semu_snapshot_reader_bytes(reader, slot->data, size, error) !=
            SEMU_OK) return error->code;
        slot->size = size;
        slot->present = 1;
    }
    temp->storage_names = names;
    return SEMU_OK;
invalid:
    semu_error_set(error, SEMU_ERR_FORMAT,
                   "invalid Sapporo 2.39 storage file snapshot");
    return SEMU_ERR_FORMAT;
}



static semu_status validate_handles(const semu_sapporo_239_files *temp,
                                    uint32_t version)
{
    size_t i;
    for (i = 0u; i < S239_FILE_MAX_HANDLES; ++i) {
        const s239_handle_slot *slot = &temp->handles[i];
        if (!slot->active) continue;
        if (!semu_s239_slot_present(temp, slot->file))
            return SEMU_ERR_FORMAT;
        if (version == SNAPSHOT_VERSION_LEGACY &&
            slot->file >= S239_FILE_COUNT)
            return SEMU_ERR_FORMAT; /* v1 has no storage index space. */
    }
    return SEMU_OK;
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
    if (status == SEMU_OK && (tag != SNAPSHOT_TAG ||
        (version != SNAPSHOT_VERSION && version != SNAPSHOT_VERSION_LEGACY) ||
        (count != FILE_COUNT && count != LEGACY_FILE_COUNT) ||
        temp.next_handle > S239_FILE_MAX_HANDLES)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid Sapporo 2.39 file snapshot header");
        status = SEMU_ERR_FORMAT;
    }
    if (status == SEMU_OK) status = read_files(&temp, reader, count, error);
    if (status == SEMU_OK) status = read_handles(&temp, reader, error);
    if (status == SEMU_OK && version == SNAPSHOT_VERSION)
        status = read_storage(&temp, reader, error);
    if (status == SEMU_OK &&
        validate_handles(&temp, version) != SEMU_OK) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid Sapporo 2.39 logical handle snapshot");
        status = SEMU_ERR_FORMAT;
    }
    if (status == SEMU_OK) {
        semu_sapporo_239_files_reset(files);
        *files = temp;
    } else {
        semu_sapporo_239_files_reset(&temp);
    }
    return status;
}
