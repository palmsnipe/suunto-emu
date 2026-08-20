#include "snapshot_io.h"

#include <stdlib.h>
#include <string.h>

static semu_status writer_reserve(semu_snapshot_writer *writer, size_t extra,
                                  semu_error *error)
{
    size_t required;
    size_t capacity;
    uint8_t *replacement;

    if (writer == NULL || writer->size > SEMU_SNAPSHOT_MAX_SECTION_SIZE ||
        extra > SEMU_SNAPSHOT_MAX_SECTION_SIZE - writer->size) {
        semu_error_set(error, SEMU_ERR_RANGE, "snapshot section is too large");
        return SEMU_ERR_RANGE;
    }
    required = writer->size + extra;
    if (required <= writer->capacity) return SEMU_OK;
    capacity = writer->capacity == 0u ? 256u : writer->capacity;
    while (capacity < required) {
        if (capacity > SEMU_SNAPSHOT_MAX_SECTION_SIZE / 2u) {
            capacity = SEMU_SNAPSHOT_MAX_SECTION_SIZE;
        } else {
            capacity *= 2u;
        }
    }
    replacement = (uint8_t *)realloc(writer->data, capacity);
    if (replacement == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate snapshot section");
        return SEMU_ERR_NOMEM;
    }
    writer->data = replacement;
    writer->capacity = capacity;
    return SEMU_OK;
}

void semu_snapshot_writer_init(semu_snapshot_writer *writer)
{
    if (writer != NULL) memset(writer, 0, sizeof(*writer));
}

void semu_snapshot_writer_destroy(semu_snapshot_writer *writer)
{
    if (writer != NULL) {
        free(writer->data);
        memset(writer, 0, sizeof(*writer));
    }
}

semu_status semu_snapshot_writer_bytes(semu_snapshot_writer *writer,
                                       const uint8_t *data, size_t size,
                                       semu_error *error)
{
    semu_status status;
    if (writer == NULL || (data == NULL && size != 0u)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "snapshot writer argument is invalid");
        return SEMU_ERR_ARGUMENT;
    }
    status = writer_reserve(writer, size, error);
    if (status != SEMU_OK) return status;
    if (size != 0u) memcpy(writer->data + writer->size, data, size);
    writer->size += size;
    return SEMU_OK;
}

semu_status semu_snapshot_writer_u8(semu_snapshot_writer *writer,
                                    uint8_t value, semu_error *error)
{
    return semu_snapshot_writer_bytes(writer, &value, 1u, error);
}

semu_status semu_snapshot_writer_u16(semu_snapshot_writer *writer,
                                     uint16_t value, semu_error *error)
{
    uint8_t data[2] = {(uint8_t)value, (uint8_t)(value >> 8u)};
    return semu_snapshot_writer_bytes(writer, data, sizeof(data), error);
}

semu_status semu_snapshot_writer_u32(semu_snapshot_writer *writer,
                                     uint32_t value, semu_error *error)
{
    uint8_t data[4] = {(uint8_t)value, (uint8_t)(value >> 8u),
                       (uint8_t)(value >> 16u), (uint8_t)(value >> 24u)};
    return semu_snapshot_writer_bytes(writer, data, sizeof(data), error);
}

semu_status semu_snapshot_writer_u64(semu_snapshot_writer *writer,
                                    uint64_t value, semu_error *error)
{
    uint8_t data[8] = {
        (uint8_t)value, (uint8_t)(value >> 8u),
        (uint8_t)(value >> 16u), (uint8_t)(value >> 24u),
        (uint8_t)(value >> 32u), (uint8_t)(value >> 40u),
        (uint8_t)(value >> 48u), (uint8_t)(value >> 56u)
    };
    return semu_snapshot_writer_bytes(writer, data, sizeof(data), error);
}

void semu_snapshot_reader_init(semu_snapshot_reader *reader,
                               const uint8_t *data, size_t size)
{
    if (reader != NULL) {
        reader->data = data;
        reader->size = size;
        reader->offset = 0u;
    }
}

semu_status semu_snapshot_reader_bytes(semu_snapshot_reader *reader,
                                       uint8_t *data, size_t size,
                                       semu_error *error)
{
    if (reader == NULL || (data == NULL && size != 0u) ||
        reader->offset > reader->size ||
        size > reader->size - reader->offset) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "snapshot section is truncated");
        return SEMU_ERR_FORMAT;
    }
    if (size != 0u) memcpy(data, reader->data + reader->offset, size);
    reader->offset += size;
    return SEMU_OK;
}

semu_status semu_snapshot_reader_u8(semu_snapshot_reader *reader,
                                    uint8_t *value, semu_error *error)
{
    return semu_snapshot_reader_bytes(reader, value, 1u, error);
}

semu_status semu_snapshot_reader_u16(semu_snapshot_reader *reader,
                                     uint16_t *value, semu_error *error)
{
    uint8_t data[2];
    semu_status status = semu_snapshot_reader_bytes(reader, data, sizeof(data),
                                                    error);
    if (status != SEMU_OK) return status;
    *value = (uint16_t)data[0] | ((uint16_t)data[1] << 8u);
    return SEMU_OK;
}

semu_status semu_snapshot_reader_u32(semu_snapshot_reader *reader,
                                     uint32_t *value, semu_error *error)
{
    uint8_t data[4];
    semu_status status = semu_snapshot_reader_bytes(reader, data, sizeof(data),
                                                    error);
    if (status != SEMU_OK) return status;
    *value = (uint32_t)data[0] | ((uint32_t)data[1] << 8u) |
             ((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u);
    return SEMU_OK;
}

semu_status semu_snapshot_reader_u64(semu_snapshot_reader *reader,
                                     uint64_t *value, semu_error *error)
{
    uint32_t low;
    uint32_t high;
    semu_status status = semu_snapshot_reader_u32(reader, &low, error);
    if (status != SEMU_OK) return status;
    status = semu_snapshot_reader_u32(reader, &high, error);
    if (status != SEMU_OK) return status;
    *value = (uint64_t)low | ((uint64_t)high << 32u);
    return SEMU_OK;
}

int semu_snapshot_reader_done(const semu_snapshot_reader *reader)
{
    return reader != NULL && reader->offset == reader->size;
}
