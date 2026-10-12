#include "sapporo_233_files.h"

#include <limits.h>
#include <string.h>

static const char *const hashes[] = {
    "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5",
    "ba286a7bce55dd2f7aba81436d3c5152401c28d86bef89b2c9dbbacd04b7e9e5",
    "b06492a982faa188a14d0a458e7165f9e0b13d0917ded3c3e37077f6ef68d5c5"
};

const semu_layer_descriptor semu_sapporo_233_files_layer = {
    .id = "sapporo-2.33-logical-files",
    .kind = SEMU_LAYER_DEVICE_FIXTURE,
    .profile_id = "sapporo-2.33.16",
    .evidence = "E-SAP-0069",
    .component_hashes = hashes,
    .component_hash_count = 3u,
    .maximum_hits = 4096u
};

static semu_status refuse_path(semu_sapporo_233_files_context *c,
                               semu_error *error, const char *message,
                               const char *path)
{
    semu_error local;
    semu_error_set(&local, SEMU_ERR_STATE, "%s: %s", message, path);
    if (c != NULL && c->state != NULL && c->state->enabled &&
        c->refusal.code == SEMU_OK) {
        c->refusal = local;
    }
    if (error != NULL) *error = local;
    return local.code;
}

static semu_status refuse(semu_sapporo_233_files_context *c,
                          semu_error *error, const char *message)
{
    semu_error local;
    semu_error_set(&local, SEMU_ERR_STATE, "%s", message);
    if (c != NULL && c->state != NULL && c->state->enabled &&
        c->refusal.code == SEMU_OK) {
        c->refusal = local;
    }
    if (error != NULL) *error = local;
    return local.code;
}

static int ready(const semu_sapporo_233_files_context *c)
{
    return c != NULL && c->state != NULL && c->logger != NULL &&
        c->state->enabled &&
        c->state->descriptor == &semu_sapporo_233_files_layer &&
        c->refusal.code == SEMU_OK;
}

static void return_from_hook(semu_cpu_state *cpu, uint32_t result)
{
    cpu->r[0] = result;
    cpu->r[15] = cpu->r[14] & ~UINT32_C(1);
}

static void return_from_hook64(semu_cpu_state *cpu, uint32_t low,
                               uint32_t high)
{
    cpu->r[0] = low;
    cpu->r[1] = high;
    cpu->r[15] = cpu->r[14] & ~UINT32_C(1);
}

static semu_sapporo_233_file *find_file(semu_sapporo_233_files_context *c,
    const char *path)
{
    unsigned i;
    for (i = 0u; i < SEMU_SAPPORO_233_FILES_MAX; ++i) {
        if (strcmp(c->files[i].path, path) == 0 &&
            (c->files[i].handle != 0u || c->files[i].seeded ||
             c->files[i].size != 0u)) {
            return &c->files[i];
        }
    }
    return NULL;
}

static semu_sapporo_233_file *free_slot(semu_sapporo_233_files_context *c)
{
    unsigned i;
    for (i = 0u; i < SEMU_SAPPORO_233_FILES_MAX; ++i) {
        if (c->files[i].handle == 0u && !c->files[i].seeded &&
            c->files[i].size == 0u && c->files[i].cursor == 0u &&
            c->files[i].size_hits == 0u) {
            return &c->files[i];
        }
    }
    return NULL;
}

static int path_char_ok(uint8_t value)
{
    if (value >= 'A' && value <= 'Z') value = (uint8_t)(value + 'a' - 'A');
    return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') ||
           value == '/' || value == '.' || value == '_' || value == '-';
}

static int read_path(semu_bus *bus, uint32_t address, char path[96],
                     semu_error *error)
{
    size_t i;
    for (i = 0u; i < 95u; ++i) {
        uint32_t value;
        if (semu_bus_read(bus, address + (uint32_t)i, 1u, &value, error) !=
            SEMU_OK) return 0;
        if (value == 0u) {
            if (i == 0u) return 0;
            path[i] = '\0';
            return 1;
        }
        if (!path_char_ok((uint8_t)value)) return 0;
        path[i] = (char)value;
    }
    return 0;
}

/* The session seed: the E-SAP-0065 schema-valid ui.txt. Synthetic content,
 * written once per install; the guest deletes it after its own parse. */
static const uint8_t ui_seed[] = { 'w', 'f', 'n', ':', '0', '0', '\r', '\n',
                                   'f', 'u', 'w', 'e', ':', 'T', '\r', '\n' };

semu_status semu_sapporo_233_files_install(semu_sapporo_233_files_context *c,
    semu_bus *bus, semu_error *error)
{
    semu_sapporo_233_file *file;
    if (c == NULL || bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "incomplete 2.33 logical-file target");
        return SEMU_ERR_ARGUMENT;
    }
    if (!ready(c) || c->state->hits != 0u) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "2.33 logical-file layer disabled or exhausted");
        return SEMU_ERR_STATE;
    }
    file = find_file(c, "settings/ui.txt");
    if (file == NULL) {
        file = free_slot(c);
        if (file == NULL) {
            semu_error_set(error, SEMU_ERR_RANGE,
                           "2.33 logical-file table is full");
            return SEMU_ERR_RANGE;
        }
        strcpy(file->path, "settings/ui.txt");
    } else if (file->size != 0u) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "2.33 ui.txt seed already populated");
        return SEMU_ERR_CONFLICT;
    }
    memcpy(file->data, ui_seed, sizeof(ui_seed));
    file->size = (uint32_t)sizeof(ui_seed);
    file->cursor = 0u;
    file->seeded = 1;
    return semu_layer_hit(c->state, c->logger,
        "trigger=logical-file-seed ordinal=1 seed settings/ui.txt wfn:00 fuwe:T",
        error);
}

static semu_sapporo_233_file *by_handle(semu_sapporo_233_files_context *c,
    uint32_t handle)
{
    unsigned i;
    for (i = 0u; i < SEMU_SAPPORO_233_FILES_MAX; ++i) {
        if (c->files[i].handle != 0u && c->files[i].handle == handle) {
            return &c->files[i];
        }
    }
    return NULL;
}

static semu_status apply_size(semu_sapporo_233_files_context *c,
    semu_cpu_state *cpu, semu_error *error)
{
    semu_sapporo_233_file *file = by_handle(c, cpu->r[0]);
    if (file == NULL) return SEMU_OK;
    if (file->size == 0u || file->size_hits >= 1u) {
        return refuse_path(c, error, "2.33 logical-file size op refused",
                           file->path);
    }
    file->size_hits += 1u;
    return_from_hook64(cpu, file->size, 0u);
    return SEMU_OK;
}

static semu_status apply_open(semu_sapporo_233_files_context *c,
    semu_bus *bus, semu_cpu_state *cpu, semu_error *error)
{
    char path[96];
    uint32_t mode = cpu->r[1];
    int is_read = mode == 1u;
    int is_create = mode == 2u;
    int is_update = mode == 3u;
    semu_sapporo_233_file *file;
    int existing;
    if (!read_path(bus, cpu->r[0], path, error)) return SEMU_OK;
    existing = find_file(c, path) != NULL;
    if (!((is_read || is_update) && existing) && !is_create) return SEMU_OK;
    if (is_create && existing) {
        file = find_file(c, path);
        if (file->handle != 0u) {
            return refuse_path(c, error, "2.33 logical-file already open",
                               path);
        }
        file->size = 0u;
        file->cursor = 0u;
        file->size_hits = 0u;
    } else if (existing) {
        file = find_file(c, path);
        if (file->handle != 0u) {
            return refuse_path(c, error, "2.33 logical-file already open",
                               path);
        }
    } else {
        if (!is_create) return SEMU_OK;
        file = free_slot(c);
        if (file == NULL) {
            return refuse(c, error, "2.33 logical-file table is full");
        }
        strcpy(file->path, path);
    }
    file->handle = SEMU_SAPPORO_233_FILES_HANDLE_BASE +
                   c->next_handle * SEMU_SAPPORO_233_FILES_HANDLE_STRIDE;
    file->cursor = 0u;
    c->next_handle += 1u;
    return_from_hook(cpu, file->handle);
    return SEMU_OK;
}

static semu_status apply_operation(semu_sapporo_233_files_context *c,
    semu_bus *bus, semu_cpu_state *cpu, uint32_t pc, semu_error *error)
{
    semu_sapporo_233_file *file = by_handle(c, cpu->r[0]);
    if (file == NULL) return SEMU_OK;
    switch (pc) {
    case SEMU_SAPPORO_233_FILES_CLOSE:
        file->handle = 0u;
        file->cursor = 0u;
        return_from_hook(cpu, 1u);
        return SEMU_OK;
    case SEMU_SAPPORO_233_FILES_TELL:
        return_from_hook64(cpu, file->cursor, 0u);
        return SEMU_OK;
    case SEMU_SAPPORO_233_FILES_SEEK: {
        /* Lane ABI (E-SAP-0069 layer): the signed 64-bit offset is
         * r2|r3<<32 and the fifth argument (origin) is at sp+0. On success
         * the wrapper returns the new 64-bit position in r0/r1. */
        uint32_t origin;
        int64_t offset = (int64_t)((uint64_t)cpu->r[2] |
                                   ((uint64_t)cpu->r[3] << 32));
        int64_t cursor;
        if (semu_bus_read(bus, cpu->r[13], 4u, &origin, error) != SEMU_OK)
            return error->code;
        if (origin == 0u) cursor = offset;
        else if (origin == 1u) cursor = (int64_t)file->cursor + offset;
        else if (origin == 2u) cursor = (int64_t)file->size + offset;
        else return refuse(c, error, "2.33 logical-file seek origin unsupported");
        if (cursor < 0) return refuse(c, error, "2.33 logical-file seek cursor negative");
        if ((uint64_t)cursor > SEMU_SAPPORO_233_FILES_MAX_BYTES) {
            return refuse(c, error, "2.33 logical-file seek cursor overflow");
        }
        file->cursor = (uint32_t)cursor;
        return_from_hook64(cpu, file->cursor, 0u);
        return SEMU_OK;
    }
    case SEMU_SAPPORO_233_FILES_WRITE: {
        uint32_t source = cpu->r[1], count = cpu->r[2], required, i;
        if (count > SEMU_SAPPORO_233_FILES_MAX_BYTES) {
            return refuse_path(c, error, "2.33 logical-file write count overflow",
                               file->path);
        }
        required = file->cursor + count;
        if (required > SEMU_SAPPORO_233_FILES_MAX_BYTES ||
            required < file->cursor) {
            return refuse_path(c, error, "2.33 logical-file size overflow",
                               file->path);
        }
        for (i = 0u; i < count; ++i) {
            uint32_t value;
            if (semu_bus_read(bus, source + i, 1u, &value, error) != SEMU_OK)
                return error->code;
            file->data[file->cursor + i] = (uint8_t)value;
        }
        if (required > file->size) file->size = required;
        file->cursor = required;
        return_from_hook(cpu, count);
        return SEMU_OK;
    }
    case SEMU_SAPPORO_233_FILES_READ: {
        uint32_t destination = cpu->r[1], count = cpu->r[2], chunk, i;
        chunk = file->size - file->cursor < count
                    ? file->size - file->cursor : count;
        for (i = 0u; i < chunk; ++i) {
            if (semu_bus_write(bus, destination + i, 1u,
                               file->data[file->cursor + i],
                               error) != SEMU_OK) return error->code;
        }
        file->cursor += chunk;
        return_from_hook(cpu, chunk);
        return SEMU_OK;
    }
    case SEMU_SAPPORO_233_FILES_FLUSH:
        return_from_hook(cpu, 0u);
        return SEMU_OK;
    default:
        return refuse(c, error, "2.33 logical-file operation unsupported");
    }
}

semu_status semu_sapporo_233_files_apply(semu_sapporo_233_files_context *c,
    semu_bus *bus, semu_cpu_state *cpu, semu_error *error)
{
    if (c == NULL || bus == NULL || cpu == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "2.33 logical-file hook arguments are incomplete");
        return SEMU_ERR_ARGUMENT;
    }
    if (!ready(c)) return SEMU_OK;
    switch (cpu->r[15]) {
    case SEMU_SAPPORO_233_FILES_SIZE:
        return apply_size(c, cpu, error);
    case SEMU_SAPPORO_233_FILES_OPEN:
        return apply_open(c, bus, cpu, error);
    case SEMU_SAPPORO_233_FILES_CLOSE:
    case SEMU_SAPPORO_233_FILES_TELL:
    case SEMU_SAPPORO_233_FILES_SEEK:
    case SEMU_SAPPORO_233_FILES_WRITE:
    case SEMU_SAPPORO_233_FILES_READ:
    case SEMU_SAPPORO_233_FILES_FLUSH:
        return apply_operation(c, bus, cpu, cpu->r[15], error);
    default:
        return SEMU_OK;
    }
}
