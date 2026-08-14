#include "semu/manifest.h"
#include "semu/hash.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum parse_mode { PARSE_FIRMWARE = 0, PARSE_PROFILE } parse_mode;
typedef enum section_kind { SECTION_NONE = 0, SECTION_ROOT, SECTION_COMPONENT,
                            SECTION_LAYER } section_kind;

typedef struct parser {
    parse_mode mode;
    const char *path;
    unsigned line;
    section_kind section;
    unsigned root_seen;
    unsigned item_seen[SEMU_MAX_COMPONENTS];
    unsigned layer_seen;
    semu_firmware_manifest *manifest;
    semu_profile *profile;
} parser;

static char *trim(char *text)
{
    char *end;
    while (*text == ' ' || *text == '\t') ++text;
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\r' || end[-1] == '\n')) --end;
    *end = '\0';
    return text;
}

static semu_status parse_error(parser *state, semu_error *error,
                               const char *message)
{
    semu_error_set(error, SEMU_ERR_FORMAT, "%s:%u: %s",
                   state->path, state->line, message);
    return SEMU_ERR_FORMAT;
}

static int valid_id(const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;
    if (*cursor == '\0') return 0;
    while (*cursor != '\0') {
        if (!isalnum(*cursor) && *cursor != '-' && *cursor != '_' &&
            *cursor != '.') return 0;
        ++cursor;
    }
    return 1;
}

static int copy_text(char *output, size_t capacity, const char *value,
                     int require_id)
{
    size_t length = strlen(value);
    if (length == 0u || length >= capacity || (require_id && !valid_id(value))) {
        return 0;
    }
    (void)memcpy(output, value, length + 1u);
    return 1;
}

static int parse_u64(const char *text, uint64_t maximum, uint64_t *value)
{
    char *end;
    unsigned long long parsed;
    int base = 10;

    if (text == NULL || *text == '\0' || *text == '+' || *text == '-') return 0;
    if (text[0] == '0' && text[1] == 'x') {
        if (!isxdigit((unsigned char)text[2])) return 0;
        base = 16;
    } else if (!isdigit((unsigned char)text[0])) {
        return 0;
    }
    errno = 0;
    parsed = strtoull(text, &end, base);
    if (errno == ERANGE || *end != '\0' || parsed > maximum) return 0;
    *value = (uint64_t)parsed;
    return 1;
}

static int safe_relative_path(const char *path)
{
    const char *segment = path;
    const char *cursor;
    if (*path == '\0' || *path == '/' || *path == '\\' ||
        (isalpha((unsigned char)path[0]) && path[1] == ':')) return 0;
    for (cursor = path; ; ++cursor) {
        if (*cursor == '\\') return 0;
        if (*cursor == '/' || *cursor == '\0') {
            size_t length = (size_t)(cursor - segment);
            if (length == 0u || (length == 2u && segment[0] == '.' &&
                                 segment[1] == '.')) return 0;
            if (*cursor == '\0') break;
            segment = cursor + 1;
        }
    }
    return 1;
}

static int duplicate_component(const parser *state, const char *id)
{
    size_t index, count = state->mode == PARSE_FIRMWARE
        ? state->manifest->component_count : state->profile->required_count;
    for (index = 0u; index < count; ++index) {
        const semu_component *item = state->mode == PARSE_FIRMWARE
            ? &state->manifest->components[index] : &state->profile->required[index];
        if (strcmp(item->id, id) == 0) return 1;
    }
    return 0;
}

static semu_status finish_section(parser *state, semu_error *error)
{
    unsigned required;
    if (state->section == SECTION_COMPONENT) {
        size_t index = state->mode == PARSE_FIRMWARE
            ? state->manifest->component_count - 1u : state->profile->required_count - 1u;
        required = state->mode == PARSE_FIRMWARE ? 31u : 29u;
        if (state->item_seen[index] != required) {
            return parse_error(state, error, "component is missing required keys");
        }
    } else if (state->section == SECTION_LAYER && state->layer_seen != 7u) {
        return parse_error(state, error, "layer is missing required keys");
    }
    return SEMU_OK;
}

static semu_status begin_component(parser *state, const char *id,
                                   semu_error *error)
{
    semu_component *item;
    size_t *count;
    if (!valid_id(id) || strlen(id) >= SEMU_ID_MAX ||
        duplicate_component(state, id)) {
        return parse_error(state, error, "invalid or duplicate component id");
    }
    count = state->mode == PARSE_FIRMWARE
        ? &state->manifest->component_count : &state->profile->required_count;
    if (*count >= SEMU_MAX_COMPONENTS) {
        return parse_error(state, error, "too many components");
    }
    item = state->mode == PARSE_FIRMWARE
        ? &state->manifest->components[*count] : &state->profile->required[*count];
    (void)memset(item, 0, sizeof(*item));
    (void)memcpy(item->id, id, strlen(id) + 1u);
    state->item_seen[*count] = 0u;
    ++*count;
    state->section = SECTION_COMPONENT;
    return SEMU_OK;
}

static semu_status begin_layer(parser *state, const char *id, semu_error *error)
{
    size_t index;
    if (state->mode != PARSE_PROFILE || !valid_id(id) ||
        strlen(id) >= SEMU_ID_MAX) {
        return parse_error(state, error, "invalid layer section");
    }
    for (index = 0u; index < state->profile->layer_count; ++index) {
        if (strcmp(state->profile->layers[index], id) == 0)
            return parse_error(state, error, "duplicate layer id");
    }
    if (state->profile->layer_count >= SEMU_MAX_LAYERS)
        return parse_error(state, error, "too many layers");
    (void)memcpy(state->profile->layers[state->profile->layer_count], id,
                 strlen(id) + 1u);
    ++state->profile->layer_count;
    state->layer_seen = 0u;
    state->section = SECTION_LAYER;
    return SEMU_OK;
}

static semu_status parse_section(parser *state, char *line, semu_error *error)
{
    size_t length = strlen(line);
    char *content;
    semu_status status = finish_section(state, error);
    if (status != SEMU_OK) return status;
    if (length < 3u || line[length - 1u] != ']')
        return parse_error(state, error, "malformed section");
    line[length - 1u] = '\0';
    content = trim(line + 1);
    if ((state->mode == PARSE_FIRMWARE && strcmp(content, "firmware") == 0) ||
        (state->mode == PARSE_PROFILE && strcmp(content, "profile") == 0)) {
        if (state->root_seen != 0u || state->section != SECTION_NONE)
            return parse_error(state, error, "duplicate or misplaced root section");
        state->section = SECTION_ROOT;
        return SEMU_OK;
    }
    if (state->root_seen == 0u)
        return parse_error(state, error, "root section must be first");
    if (strncmp(content, "component ", 10u) == 0)
        return begin_component(state, content + 10u, error);
    if (strncmp(content, "layer ", 6u) == 0)
        return begin_layer(state, content + 6u, error);
    return parse_error(state, error, "unknown section");
}

static semu_component *current_component(parser *state, size_t *index)
{
    if (state->mode == PARSE_FIRMWARE) {
        *index = state->manifest->component_count - 1u;
        return &state->manifest->components[*index];
    }
    *index = state->profile->required_count - 1u;
    return &state->profile->required[*index];
}

static semu_status set_once(parser *state, unsigned *seen, unsigned bit,
                            semu_error *error)
{
    if ((*seen & bit) != 0u) return parse_error(state, error, "duplicate key");
    *seen |= bit;
    return SEMU_OK;
}

static semu_status parse_root_key(parser *state, const char *key,
                                  const char *value, semu_error *error)
{
    unsigned bit = 0u;
    char *output = NULL;
    uint64_t number;
    if (strcmp(key, "format") == 0) bit = 1u;
    else if (state->mode == PARSE_FIRMWARE && strcmp(key, "product") == 0)
        bit = 2u, output = state->manifest->product;
    else if (state->mode == PARSE_FIRMWARE && strcmp(key, "version") == 0)
        bit = 4u, output = state->manifest->version;
    else if (state->mode == PARSE_PROFILE && strcmp(key, "id") == 0)
        bit = 2u, output = state->profile->id;
    else if (state->mode == PARSE_PROFILE && strcmp(key, "board") == 0)
        bit = 4u, output = state->profile->board;
    else if (state->mode == PARSE_PROFILE && strcmp(key, "product") == 0)
        bit = 8u, output = state->profile->product;
    else if (state->mode == PARSE_PROFILE && strcmp(key, "version") == 0)
        bit = 16u, output = state->profile->version;
    else if (state->mode == PARSE_PROFILE && strcmp(key, "vector_table") == 0)
        bit = 32u;
    else if (state->mode == PARSE_PROFILE && strcmp(key, "display_width") == 0)
        bit = 64u;
    else if (state->mode == PARSE_PROFILE && strcmp(key, "display_height") == 0)
        bit = 128u;
    else return parse_error(state, error, "unknown root key");
    if (set_once(state, &state->root_seen, bit, error) != SEMU_OK)
        return SEMU_ERR_FORMAT;
    if (output != NULL) {
        if (!copy_text(output, SEMU_ID_MAX, value, 1))
            return parse_error(state, error, "invalid identifier value");
    } else if (!parse_u64(value, UINT32_MAX, &number)) {
        return parse_error(state, error, "invalid integer value");
    } else if (bit == 1u) {
        if (number != 1u) return parse_error(state, error, "unsupported format");
        if (state->mode == PARSE_FIRMWARE) state->manifest->format = (unsigned)number;
        else state->profile->format = (unsigned)number;
    } else if (bit == 32u) state->profile->vector_table = (uint32_t)number;
    else if (bit == 64u) state->profile->display_width = (uint32_t)number;
    else state->profile->display_height = (uint32_t)number;
    return SEMU_OK;
}

static semu_status parse_component_key(parser *state, const char *key,
                                       const char *value, semu_error *error)
{
    size_t index;
    semu_component *item = current_component(state, &index);
    unsigned bit;
    uint64_t number;
    if (strcmp(key, "role") == 0) bit = 1u;
    else if (strcmp(key, "path") == 0) bit = 2u;
    else if (strcmp(key, "load_address") == 0) bit = 4u;
    else if (strcmp(key, "size") == 0) bit = 8u;
    else if (strcmp(key, "sha256") == 0) bit = 16u;
    else return parse_error(state, error, "unknown component key");
    if (state->mode == PARSE_PROFILE && bit == 2u)
        return parse_error(state, error, "profile component cannot have path");
    if (set_once(state, &state->item_seen[index], bit, error) != SEMU_OK)
        return SEMU_ERR_FORMAT;
    if (bit == 1u) {
        if (!copy_text(item->role, sizeof(item->role), value, 1))
            return parse_error(state, error, "invalid component role");
    } else if (bit == 2u) {
        if (!safe_relative_path(value) ||
            !copy_text(item->path, sizeof(item->path), value, 0))
            return parse_error(state, error, "unsafe component path");
    } else if (bit == 4u) {
        if (!parse_u64(value, UINT32_MAX, &number))
            return parse_error(state, error, "invalid load address");
        item->load_address = (uint32_t)number;
    } else if (bit == 8u) {
        if (!parse_u64(value, UINT64_MAX, &number) || number == 0u)
            return parse_error(state, error, "invalid component size");
        item->size = number;
    } else if (!semu_sha256_parse(value, item->sha256)) {
        return parse_error(state, error, "invalid SHA-256");
    }
    return SEMU_OK;
}

static semu_status parse_layer_key(parser *state, const char *key,
                                   const char *value, semu_error *error)
{
    unsigned bit;
    uint64_t number;
    if (strcmp(key, "kind") == 0) bit = 1u;
    else if (strcmp(key, "evidence") == 0) bit = 2u;
    else if (strcmp(key, "max_hits") == 0) bit = 4u;
    else return parse_error(state, error, "unknown layer key");
    if (set_once(state, &state->layer_seen, bit, error) != SEMU_OK)
        return SEMU_ERR_FORMAT;
    if (bit == 1u && !valid_id(value))
        return parse_error(state, error, "invalid layer kind");
    if (bit == 2u && *value == '\0')
        return parse_error(state, error, "empty layer evidence");
    if (bit == 4u && (!parse_u64(value, UINT64_MAX, &number) || number == 0u))
        return parse_error(state, error, "invalid layer hit budget");
    return SEMU_OK;
}

static semu_status parse_key(parser *state, char *line, semu_error *error)
{
    char *equals = strchr(line, '=');
    char *key, *value;
    if (state->section == SECTION_NONE || equals == NULL ||
        strchr(equals + 1, '=') != NULL)
        return parse_error(state, error, "expected one key=value pair");
    *equals = '\0';
    key = trim(line);
    value = trim(equals + 1);
    if (!valid_id(key)) return parse_error(state, error, "invalid key");
    if (state->section == SECTION_ROOT)
        return parse_root_key(state, key, value, error);
    if (state->section == SECTION_COMPONENT)
        return parse_component_key(state, key, value, error);
    return parse_layer_key(state, key, value, error);
}

static semu_status resolve_paths(const char *manifest_path,
                                 semu_firmware_manifest *manifest,
                                 semu_error *error)
{
    const char *slash = strrchr(manifest_path, '/');
    size_t prefix = slash == NULL ? 0u : (size_t)(slash - manifest_path + 1);
    size_t index;
    for (index = 0u; index < manifest->component_count; ++index) {
        char joined[SEMU_PATH_MAX];
        size_t path_length = strlen(manifest->components[index].path);
        size_t add_dot = prefix == 0u ? 2u : 0u;
        if (prefix + add_dot + path_length >= sizeof(joined)) {
            semu_error_set(error, SEMU_ERR_RANGE, "resolved component path is too long");
            return SEMU_ERR_RANGE;
        }
        if (prefix != 0u) (void)memcpy(joined, manifest_path, prefix);
        else (void)memcpy(joined, "./", 2u);
        (void)memcpy(joined + prefix + add_dot,
                     manifest->components[index].path, path_length + 1u);
        (void)memcpy(manifest->components[index].path, joined,
                     prefix + add_dot + path_length + 1u);
    }
    return SEMU_OK;
}

static semu_status load_file(const char *path, parse_mode mode, void *output,
                             semu_error *error)
{
    char buffer[1024];
    FILE *stream;
    parser state;
    semu_status status = SEMU_OK;
    unsigned required_root = mode == PARSE_FIRMWARE ? 7u : 255u;

    if (path == NULL || output == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "path and output required");
        return SEMU_ERR_ARGUMENT;
    }
    stream = fopen(path, "rb");
    if (stream == NULL) {
        semu_error_set(error, SEMU_ERR_IO, "cannot open %s: %s", path, strerror(errno));
        return SEMU_ERR_IO;
    }
    (void)memset(&state, 0, sizeof(state));
    state.mode = mode; state.path = path;
    if (mode == PARSE_FIRMWARE) {
        state.manifest = (semu_firmware_manifest *)output;
        (void)memset(state.manifest, 0, sizeof(*state.manifest));
    } else {
        state.profile = (semu_profile *)output;
        (void)memset(state.profile, 0, sizeof(*state.profile));
    }
    while (fgets(buffer, sizeof(buffer), stream) != NULL) {
        char *line;
        size_t index, length;
        ++state.line;
        length = strlen(buffer);
        if (length == sizeof(buffer) - 1u && buffer[length - 1u] != '\n') {
            status = parse_error(&state, error, "line is too long"); break;
        }
        for (index = 0u; index < length; ++index) {
            unsigned char ch = (unsigned char)buffer[index];
            if ((ch < 32u && ch != '\t' && ch != '\r' && ch != '\n') || ch > 126u) {
                status = parse_error(&state, error, "file is not printable ASCII"); break;
            }
        }
        if (status != SEMU_OK) break;
        line = trim(buffer);
        if (*line == '\0' || *line == '#') continue;
        status = *line == '[' ? parse_section(&state, line, error)
                              : parse_key(&state, line, error);
        if (status != SEMU_OK) break;
    }
    if (status == SEMU_OK && ferror(stream)) {
        semu_error_set(error, SEMU_ERR_IO, "cannot read %s", path); status = SEMU_ERR_IO;
    }
    if (status == SEMU_OK) status = finish_section(&state, error);
    if (status == SEMU_OK && state.root_seen != required_root)
        status = parse_error(&state, error, "root section is missing required keys");
    if (status == SEMU_OK && mode == PARSE_FIRMWARE &&
        state.manifest->component_count == 0u)
        status = parse_error(&state, error, "manifest has no components");
    if (status == SEMU_OK && mode == PARSE_PROFILE &&
        (state.profile->required_count == 0u || state.profile->display_width == 0u ||
         state.profile->display_height == 0u))
        status = parse_error(&state, error, "profile dimensions/components are empty");
    if (fclose(stream) != 0 && status == SEMU_OK) {
        semu_error_set(error, SEMU_ERR_IO, "cannot close %s", path); status = SEMU_ERR_IO;
    }
    if (status == SEMU_OK && mode == PARSE_FIRMWARE)
        status = resolve_paths(path, state.manifest, error);
    if (status == SEMU_OK) semu_error_clear(error);
    else if (mode == PARSE_FIRMWARE) (void)memset(output, 0, sizeof(semu_firmware_manifest));
    else (void)memset(output, 0, sizeof(semu_profile));
    return status;
}

semu_status semu_manifest_load(const char *path, semu_firmware_manifest *manifest,
                               semu_error *error)
{
    return load_file(path, PARSE_FIRMWARE, manifest, error);
}

semu_status semu_profile_load(const char *path, semu_profile *profile,
                              semu_error *error)
{
    return load_file(path, PARSE_PROFILE, profile, error);
}
