/* Bounded machine snapshot file I/O for the headless and SDL frontends. */

#include "semu/machine.h"

#include <stdio.h>
#include <stdlib.h>

#define SEMU_CLI_SNAPSHOT_HEADER_SIZE \
    (4u + 4u + SEMU_ID_MAX + SEMU_REPLAY_HASH_HEX_LEN + 4u)
#define SEMU_CLI_SNAPSHOT_SECTION_HEADER_SIZE (4u + 8u)
#define SEMU_CLI_SNAPSHOT_MAX_IMAGE_SIZE \
    (SEMU_CLI_SNAPSHOT_HEADER_SIZE + \
     SEMU_SNAPSHOT_MAX_SECTIONS * \
         (SEMU_CLI_SNAPSHOT_SECTION_HEADER_SIZE + \
          SEMU_SNAPSHOT_MAX_SECTION_SIZE))

static semu_status snapshot_file_size(FILE *stream, size_t *size,
                                       semu_error *error)
{
    long length;
    if (fseek(stream, 0L, SEEK_END) != 0) {
        semu_error_set(error, SEMU_ERR_IO,
                       "cannot seek snapshot file");
        return SEMU_ERR_IO;
    }
    length = ftell(stream);
    if (length < 0L || (unsigned long)length >
            (unsigned long)SEMU_CLI_SNAPSHOT_MAX_IMAGE_SIZE) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "snapshot file exceeds the bounded image size");
        return SEMU_ERR_RANGE;
    }
    if (fseek(stream, 0L, SEEK_SET) != 0) {
        semu_error_set(error, SEMU_ERR_IO,
                       "cannot rewind snapshot file");
        return SEMU_ERR_IO;
    }
    *size = (size_t)length;
    return SEMU_OK;
}

semu_status semu_cli_snapshot_load_file(const char *path,
                                        semu_snapshot *snapshot,
                                        semu_error *error)
{
    FILE *stream;
    uint8_t *data;
    size_t size;
    semu_status status;

    if (path == NULL || snapshot == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "snapshot load arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    stream = fopen(path, "rb");
    if (stream == NULL) {
        semu_error_set(error, SEMU_ERR_IO, "cannot open snapshot %s", path);
        return SEMU_ERR_IO;
    }
    status = snapshot_file_size(stream, &size, error);
    if (status != SEMU_OK) {
        (void)fclose(stream);
        return status;
    }
    data = (uint8_t *)malloc(size != 0u ? size : 1u);
    if (data == NULL) {
        (void)fclose(stream);
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate snapshot image");
        return SEMU_ERR_NOMEM;
    }
    if (size != 0u && fread(data, 1u, size, stream) != size) {
        free(data);
        (void)fclose(stream);
        semu_error_set(error, SEMU_ERR_IO,
                       "cannot read snapshot %s", path);
        return SEMU_ERR_IO;
    }
    if (fclose(stream) != 0) {
        free(data);
        semu_error_set(error, SEMU_ERR_IO,
                       "cannot close snapshot %s", path);
        return SEMU_ERR_IO;
    }
    status = semu_snapshot_deserialize(snapshot, data, size, error);
    free(data);
    return status;
}

semu_status semu_cli_snapshot_save_file(const char *path,
                                        const semu_snapshot *snapshot,
                                        semu_error *error)
{
    FILE *stream;
    uint8_t *data;
    size_t size;
    semu_status status;

    if (path == NULL || snapshot == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "snapshot save arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    data = (uint8_t *)malloc(SEMU_CLI_SNAPSHOT_MAX_IMAGE_SIZE);
    if (data == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate snapshot image");
        return SEMU_ERR_NOMEM;
    }
    size = semu_snapshot_serialize(snapshot, data,
                                   SEMU_CLI_SNAPSHOT_MAX_IMAGE_SIZE);
    if (size == 0u) {
        free(data);
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "snapshot cannot be serialized");
        return SEMU_ERR_FORMAT;
    }
    stream = fopen(path, "wb");
    if (stream == NULL) {
        free(data);
        semu_error_set(error, SEMU_ERR_IO, "cannot open snapshot %s", path);
        return SEMU_ERR_IO;
    }
    status = SEMU_OK;
    if (fwrite(data, 1u, size, stream) != size) {
        semu_error_set(error, SEMU_ERR_IO,
                       "cannot write snapshot %s", path);
        status = SEMU_ERR_IO;
        (void)fclose(stream);
    } else if (fclose(stream) != 0) {
        semu_error_set(error, SEMU_ERR_IO,
                       "cannot close snapshot %s", path);
        status = SEMU_ERR_IO;
    }
    free(data);
    return status;
}
