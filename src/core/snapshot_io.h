#ifndef SEMU_SNAPSHOT_IO_H
#define SEMU_SNAPSHOT_IO_H

#include "semu/trace.h"

typedef struct semu_snapshot_writer {
    uint8_t *data;
    size_t size;
    size_t capacity;
} semu_snapshot_writer;

typedef struct semu_snapshot_reader {
    const uint8_t *data;
    size_t size;
    size_t offset;
} semu_snapshot_reader;

void semu_snapshot_writer_init(semu_snapshot_writer *writer);
void semu_snapshot_writer_destroy(semu_snapshot_writer *writer);
semu_status semu_snapshot_writer_u8(semu_snapshot_writer *writer,
                                    uint8_t value, semu_error *error);
semu_status semu_snapshot_writer_u16(semu_snapshot_writer *writer,
                                     uint16_t value, semu_error *error);
semu_status semu_snapshot_writer_u32(semu_snapshot_writer *writer,
                                     uint32_t value, semu_error *error);
semu_status semu_snapshot_writer_u64(semu_snapshot_writer *writer,
                                     uint64_t value, semu_error *error);
semu_status semu_snapshot_writer_bytes(semu_snapshot_writer *writer,
                                       const uint8_t *data, size_t size,
                                       semu_error *error);

void semu_snapshot_reader_init(semu_snapshot_reader *reader,
                               const uint8_t *data, size_t size);
semu_status semu_snapshot_reader_u8(semu_snapshot_reader *reader,
                                    uint8_t *value, semu_error *error);
semu_status semu_snapshot_reader_u16(semu_snapshot_reader *reader,
                                     uint16_t *value, semu_error *error);
semu_status semu_snapshot_reader_u32(semu_snapshot_reader *reader,
                                     uint32_t *value, semu_error *error);
semu_status semu_snapshot_reader_u64(semu_snapshot_reader *reader,
                                     uint64_t *value, semu_error *error);
semu_status semu_snapshot_reader_bytes(semu_snapshot_reader *reader,
                                       uint8_t *data, size_t size,
                                       semu_error *error);
int semu_snapshot_reader_done(const semu_snapshot_reader *reader);

#endif
