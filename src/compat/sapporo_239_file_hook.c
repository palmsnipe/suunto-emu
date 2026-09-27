#include "sapporo_239_files_internal.h"
#include "sapporo_239.h"

#include <stdlib.h>
#include <string.h>

#define FILE_OPEN UINT32_C(0x000920b4)
#define FILE_CLOSE UINT32_C(0x000920f4)
#define FILE_TELL UINT32_C(0x00092146)
#define FILE_TRUNCATE UINT32_C(0x00092162)
#define FILE_SEEK UINT32_C(0x00092182)
#define FILE_WRITE UINT32_C(0x000921a8)
#define FILE_READ UINT32_C(0x000921dc)
#define FILE_FLUSH UINT32_C(0x0009221a)
#define FILE_SIZE UINT32_C(0x00092244)

int semu_sapporo_239_file_hook_pc(uint32_t pc)
{
    return pc == FILE_OPEN || pc == FILE_CLOSE || pc == FILE_TELL ||
           pc == FILE_TRUNCATE || pc == FILE_SEEK || pc == FILE_WRITE ||
           pc == FILE_READ || pc == FILE_FLUSH || pc == FILE_SIZE;
}

static semu_status refuse(semu_error *error, const char *message)
{
    semu_error_set(error, SEMU_ERR_STATE, "%s", message);
    return SEMU_ERR_STATE;
}

static semu_status read_path(semu_bus *bus, uint32_t address, char path[65],
                             semu_error *error)
{
    size_t i;
    if (address > UINT32_MAX - 63u)
        return refuse(error, "Sapporo 2.39 file path range overflow");
    for (i = 0u; i < 64u; ++i) {
        uint32_t value;
        if (semu_bus_read(bus, address + (uint32_t)i, 1u, &value, error) !=
            SEMU_OK) return error->code;
        if (value == 0u) {
            if (i == 0u) return refuse(error, "empty Sapporo 2.39 file path");
            path[i] = '\0';
            return SEMU_OK;
        }
        if (value >= 'A' && value <= 'Z') value += 'a' - 'A';
        if (!((value >= 'a' && value <= 'z') ||
              (value >= '0' && value <= '9') || value == '/' ||
              value == '.' || value == '_' || value == '-'))
            return refuse(error, "invalid Sapporo 2.39 file path");
        path[i] = (char)value;
    }
    return refuse(error, "unterminated Sapporo 2.39 file path");
}

static semu_status hit(semu_layer_state *layer, semu_logger *logger,
                       semu_error *error)
{
    return semu_layer_intervention_hit(
        layer, logger, SEMU_SAPPORO_239_IV_LOGICAL_FILE, error);
}

static void return_from_hook(semu_cpu_state *cpu, uint32_t result)
{
    cpu->r[0] = result;
    cpu->r[15] = cpu->r[14] & ~UINT32_C(1);
}

static semu_status open_file(semu_sapporo_239_files *files, semu_bus *bus,
                             semu_cpu_state *cpu, semu_layer_state *layer,
                             semu_logger *logger, semu_error *error)
{
    char path[65];
    uint32_t mode = cpu->r[1];
    s239_handle_slot *handle;
    uint8_t *allocation = NULL;
    int index;
    int storage_slot = -1;
    size_t capacity;
    if (read_path(bus, cpu->r[0], path, error) != SEMU_OK) return error->code;
    index = semu_s239_file_index(path);
    /* E-SAP-TIME-NATIVE-239-001: native storage retains this exact save. */
    if (mode == 2u && strcmp(path, "settings/time") == 0) return SEMU_OK;
    /* E-SAP-COMPAT-QUIET-READ-239-001: do not bypass table-owned state. */
    if (mode == 9u && index < 0) return SEMU_OK;
    if (mode < 1u || mode > 3u)
        return refuse(error, "unknown Sapporo 2.39 file open mode");
    if (index < 0) {
        /* E-SAP239-REPO38D123-001: admit the storage/<key>/data.jsn family
           on its write-create open only (guest mode 2, observed value); the
           path shape is validated fail-closed before any slot is created. */
        if (mode == 2u && semu_s239_storage_path(path)) {
            /* Native open semantics live in the admit scan: an admitted
               name (present or closed) re-presents its slot; only a new
               name appends. */
            storage_slot = semu_s239_storage_admit(files, path);
            if (storage_slot == -(int)SEMU_ERR_STATE)
                return refuse(error,
                    "invalid Sapporo 2.39 storage path shape");
            if (storage_slot == -(int)SEMU_ERR_NOMEM) {
                semu_error_set(error, SEMU_ERR_NOMEM,
                    "cannot allocate Sapporo 2.39 storage file");
                return SEMU_ERR_NOMEM;
            }
            if (storage_slot < 0)
                return refuse(error,
                    "Sapporo 2.39 storage slot pool exhausted");
            index = semu_s239_storage_index(files, path);
        } else if (mode == 2u) {
            return refuse(error, "unknown Sapporo 2.39 writable file path");
        } else {
            return SEMU_OK;
        }
    }
    if (storage_slot < 0 &&
        index < (int)S239_FILE_COUNT && !files->files[index].present &&
        mode != 2u)
        return SEMU_OK;
    if (files->next_handle >= S239_FILE_MAX_HANDLES)
        return refuse(error, "Sapporo 2.39 logical handle pool exhausted");
    capacity = semu_s239_file_capacity(files, (uint32_t)index);
    if (storage_slot < 0 && index < (int)S239_FILE_COUNT &&
        !files->files[index].present && capacity != 0u) {
        allocation = (uint8_t *)calloc(capacity, 1u);
        if (allocation == NULL) {
            semu_error_set(error, SEMU_ERR_NOMEM,
                           "cannot allocate Sapporo 2.39 logical file");
            return SEMU_ERR_NOMEM;
        }
    }
    if (hit(layer, logger, error) != SEMU_OK) {
        s239_storage_slot *undo_slot;
        free(allocation);
        if (storage_slot >= 0) {
            /* Undo the admission: free payload bytes but keep the name
               and the append-only name-table extent. */
            undo_slot = &files->storage[
                (size_t)(storage_slot - (int)S239_FILE_COUNT)];
            free(undo_slot->data);
            undo_slot->data = NULL;
            undo_slot->size = 0u;
            undo_slot->present = 0;
        }
        return error->code;
    }
    if (storage_slot < 0 && index < (int)S239_FILE_COUNT &&
        !files->files[index].present) {
        files->files[index].data = allocation;
        files->files[index].size = 0u;
        files->files[index].present = 1;
    }
    handle = &files->handles[files->next_handle];
    handle->value = S239_FILE_HANDLE_BASE + files->next_handle *
                    S239_FILE_HANDLE_STRIDE;
    handle->cursor = 0u;
    handle->file = (uint8_t)index;
    handle->mode = (uint8_t)mode;
    handle->active = 1;
    ++files->next_handle;
    semu_log_write(logger, SEMU_LOG_WARNING, "compat", "logical-file",
                   "operation=open path=%s mode=%u handle=0x%08x",
                   path, (unsigned)mode, (unsigned)handle->value);
    return_from_hook(cpu, handle->value);
    return SEMU_OK;
}

static int synthetic_handle_value(uint32_t value)
{
    uint32_t span = S239_FILE_MAX_HANDLES * S239_FILE_HANDLE_STRIDE;
    return value >= S239_FILE_HANDLE_BASE &&
           value < S239_FILE_HANDLE_BASE + span;
}

static semu_status seek_cursor(const s239_handle_slot *handle, size_t size,
                               size_t capacity, semu_cpu_state *cpu,
                               uint32_t *new_cursor, semu_error *error)
{
    uint32_t raw = cpu->r[1];
    int64_t offset = (raw & UINT32_C(0x80000000)) != 0u
        ? (int64_t)(uint64_t)raw - INT64_C(4294967296)
        : (int64_t)raw;
    int64_t base;
    int64_t cursor;
    if (cpu->r[2] == 0u) base = 0;
    else if (cpu->r[2] == 1u) base = (int64_t)handle->cursor;
    else if (cpu->r[2] == 2u) base = (int64_t)size;
    else return refuse(error, "unknown Sapporo 2.39 seek origin");
    cursor = base + offset;
    if (cursor < 0 || (uint64_t)cursor > capacity)
        return refuse(error, "Sapporo 2.39 seek is outside file capacity");
    *new_cursor = (uint32_t)cursor;
    return SEMU_OK;
}

static semu_status stage_write(size_t capacity,
                               const s239_handle_slot *handle, semu_bus *bus,
                               const semu_cpu_state *cpu, uint8_t **chunk,
                               semu_error *error)
{
    uint32_t count = cpu->r[2];
    if (handle->cursor > capacity || count > capacity - handle->cursor)
        return refuse(error, "Sapporo 2.39 logical file exceeds capacity");
    *chunk = count != 0u ? (uint8_t *)malloc(count) : NULL;
    if (count != 0u && *chunk == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot stage Sapporo 2.39 file write");
        return SEMU_ERR_NOMEM;
    }
    if (count != 0u && semu_bus_copy_out(bus, cpu->r[1], *chunk, count,
                                        error) != SEMU_OK) return error->code;
    return SEMU_OK;
}

/* Unified view over a fixed-table slot or an admitted storage slot.
   The size pointer targets the owning slot's own size field. */
typedef struct s239_slot_view {
    uint8_t *data;
    size_t *size;
} s239_slot_view;

static void slot_view(semu_sapporo_239_files *files, uint8_t file_index,
                      s239_slot_view *view)
{
    if (file_index < S239_FILE_COUNT) {
        view->data = files->files[file_index].data;
        view->size = &files->files[file_index].size;
    } else {
        view->data = files->storage[file_index - S239_FILE_COUNT].data;
        view->size = &files->storage[file_index - S239_FILE_COUNT].size;
    }
}

static semu_status validate_read(const s239_slot_view *file,
                                 const s239_handle_slot *handle,
                                 semu_bus *bus, const semu_cpu_state *cpu,
                                 uint32_t *result, semu_error *error)
{
    uint32_t i;
    size_t available = handle->cursor < *file->size
        ? *file->size - handle->cursor : 0u;
    *result = cpu->r[2] < available ? cpu->r[2] : (uint32_t)available;
    if (*result == 0u) return SEMU_OK;
    if (cpu->r[1] > UINT32_MAX - (*result - 1u))
        return refuse(error, "Sapporo 2.39 file read range overflow");
    for (i = 0u; i < *result; ++i)
        if (semu_bus_validate_write(bus, cpu->r[1] + i, 1u, error) != SEMU_OK)
            return error->code;
    return SEMU_OK;
}

static semu_status operate_file(semu_sapporo_239_files *files, semu_bus *bus,
                                semu_cpu_state *cpu, semu_layer_state *layer,
                                semu_logger *logger, semu_error *error)
{
    s239_handle_slot *handle = semu_s239_find_handle(files, cpu->r[0]);
    s239_slot_view file;
    size_t capacity, file_size;
    uint32_t result = 0u;
    uint32_t new_cursor;
    const char *operation;
    uint8_t *chunk = NULL;
    semu_status status = SEMU_OK;
    char name[65];
    if (handle == NULL) {
        if (synthetic_handle_value(cpu->r[0]))
            return refuse(error, "unknown Sapporo 2.39 logical handle");
        return SEMU_OK;
    }
    slot_view(files, handle->file, &file);
    capacity = semu_s239_file_capacity(files, handle->file);
    file_size = *file.size;
    semu_s239_slot_name(files, handle->file, name);
    new_cursor = handle->cursor;
    if (cpu->r[15] == FILE_CLOSE) { operation = "close"; result = 1u; }
    else if (cpu->r[15] == FILE_TELL) { operation = "tell"; result = handle->cursor; }
    else if (cpu->r[15] == FILE_FLUSH) { operation = "flush"; }
    else if (cpu->r[15] == FILE_SIZE) {
        operation = "size"; result = (uint32_t)file_size;
    }
    else if (cpu->r[15] == FILE_TRUNCATE) {
        operation = "truncate";
        if (handle->cursor > file_size)
            return refuse(error, "truncate cursor exceeds Sapporo 2.39 file");
    } else if (cpu->r[15] == FILE_SEEK) {
        operation = "seek";
        status = seek_cursor(handle, file_size, capacity, cpu, &new_cursor,
                             error);
        if (status == SEMU_OK) result = cpu->r[1]; /* Native wrapper ABI. */
    } else if (cpu->r[15] == FILE_WRITE) {
        operation = "write"; result = cpu->r[2];
        status = stage_write(capacity, handle, bus, cpu, &chunk, error);
        if (status == SEMU_OK) new_cursor = handle->cursor + result;
    } else {
        operation = "read";
        status = validate_read(&file, handle, bus, cpu, &result, error);
        if (status == SEMU_OK) new_cursor = handle->cursor + result;
    }
    if (status != SEMU_OK) { free(chunk); return status; }
    if (hit(layer, logger, error) != SEMU_OK) { free(chunk); return error->code; }
    if (cpu->r[15] == FILE_CLOSE) {
        handle->active = 0;
    } else if (cpu->r[15] == FILE_TRUNCATE) {
        if (file.data != NULL && handle->cursor < file_size)
            memset(file.data + handle->cursor, 0, file_size - handle->cursor);
        *file.size = handle->cursor;
    } else if (cpu->r[15] == FILE_SEEK) {
        handle->cursor = new_cursor;
    } else if (cpu->r[15] == FILE_WRITE) {
        if (handle->cursor > file_size)
            memset(file.data + file_size, 0, handle->cursor - file_size);
        if (result != 0u)
            memcpy(file.data + handle->cursor, chunk, result);
        handle->cursor = new_cursor;
        if (file_size < new_cursor) *file.size = new_cursor;
    } else if (cpu->r[15] == FILE_READ) {
        uint32_t i;
        for (i = 0u; i < result; ++i)
            if (semu_bus_write(bus, cpu->r[1] + i, 1u,
                    file.data[handle->cursor + i], error) != SEMU_OK) {
                free(chunk);
                return error->code;
            }
        handle->cursor = new_cursor;
    }
    semu_log_write(logger, SEMU_LOG_WARNING, "compat", "logical-file",
        "operation=%s path=%s result=%u size=%u cursor=%u",
        operation, name, (unsigned)result, (unsigned)*file.size,
        (unsigned)handle->cursor);
    free(chunk);
    return_from_hook(cpu, result);
    return SEMU_OK;
}

semu_status semu_sapporo_239_apply_file_hook(
    semu_sapporo_239_files *files, semu_bus *bus, semu_cpu_state *cpu,
    semu_layer_state *layer, semu_logger *logger, semu_error *error)
{
    if (files == NULL || bus == NULL || cpu == NULL || layer == NULL ||
        logger == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo 2.39 file hook arguments are incomplete");
        return SEMU_ERR_ARGUMENT;
    }
    if (!semu_sapporo_239_file_hook_pc(cpu->r[15])) return SEMU_OK;
    if (layer->descriptor != &semu_sapporo_239_wbsto_layer || !layer->enabled)
        return refuse(error, "disabled Sapporo 2.39 logical file layer");
    if (cpu->r[15] == FILE_OPEN)
        return open_file(files, bus, cpu, layer, logger, error);
    return operate_file(files, bus, cpu, layer, logger, error);
}
