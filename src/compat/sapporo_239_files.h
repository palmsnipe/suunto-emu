#ifndef SEMU_SAPPORO_239_FILES_H
#define SEMU_SAPPORO_239_FILES_H

#include "semu/bus.h"
#include "semu/compat.h"
#include "semu/cpu.h"
#include "../core/snapshot_io.h"

typedef struct semu_sapporo_239_files semu_sapporo_239_files;

semu_sapporo_239_files *semu_sapporo_239_files_create(semu_error *error);
void semu_sapporo_239_files_destroy(semu_sapporo_239_files *files);
void semu_sapporo_239_files_reset(semu_sapporo_239_files *files);

int semu_sapporo_239_file_hook_pc(uint32_t pc);
semu_status semu_sapporo_239_apply_file_hook(
    semu_sapporo_239_files *files, semu_bus *bus, semu_cpu_state *cpu,
    semu_layer_state *layer, semu_logger *logger, semu_error *error);

semu_status semu_sapporo_239_files_snapshot_write(
    const semu_sapporo_239_files *files, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_sapporo_239_files_snapshot_read(
    semu_sapporo_239_files *files, semu_snapshot_reader *reader,
    semu_error *error);

size_t semu_sapporo_239_files_count(const semu_sapporo_239_files *files);
size_t semu_sapporo_239_file_size(const semu_sapporo_239_files *files,
                                  const char *path);

#endif
